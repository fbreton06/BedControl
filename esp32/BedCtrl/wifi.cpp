#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_ping.h>
#include <ping/ping_sock.h>
#include <lwip/inet.h>
#include "globals.h"

// -------------------------------------------------------
//  État interne
// -------------------------------------------------------
static bool sta_connected = false;
static bool ap_mode = false;

// Ping watchdog
static uint32_t ping_last_ok_ms    = 0;   // millis() du dernier ping réussi
static uint32_t ping_last_check_ms = 0;   // millis() du dernier déclenchement de ping
static bool     ping_in_progress   = false;
static bool     ping_last_result   = true; // optimiste au démarrage

// -------------------------------------------------------
//  Callbacks ping asynchrone (API ESP-IDF)
// -------------------------------------------------------
static esp_ping_handle_t s_ping_hdl = NULL;

static void cb_ping_success(esp_ping_handle_t hdl, void*) {

    uint32_t elapsed;
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed, sizeof(elapsed));
    //Serial.printf("[WiFi] Ping OK (%u ms)\n", elapsed);
    ping_last_result  = true;
    ping_last_ok_ms   = millis();
    ping_in_progress  = false;
}

static void cb_ping_timeout(esp_ping_handle_t hdl, void*) {

    Serial.println("[WiFi] Ping timeout");
    ping_last_result = false;
    ping_in_progress = false;
}

static void cb_ping_end(esp_ping_handle_t hdl, void*) {

    esp_ping_stop(hdl);
    esp_ping_delete_session(hdl);
    s_ping_hdl       = NULL;
    ping_in_progress = false;
}

// Résout 'target' (IP littérale OU nom d'hôte) en ip_addr_t.
// Le résultat est mis en cache : la résolution DNS (bloquante) n'est refaite
// que si l'hôte configuré change, pour ne pas figer la boucle tous les
// PING_INTERVAL_S — et limiter l'exposition au watchdog si le DNS est injoignable.
static bool resolve_target(String& target, ip_addr_t& out) {

    static String    cached_host;
    static ip_addr_t cached_ip;
    static bool      cached_ok = false;

    if (!cached_ok || cached_host != target) {

        cached_ok   = false;
        cached_host = target;

        if (ipaddr_aton(target.c_str(), &cached_ip)) {

            // Déjà une IP numérique (ex. "192.168.1.50")
            cached_ok = true;
        } else {

            // Nom d'hôte → résolution DNS, puis re-parsing de l'IP obtenue
            IPAddress resolved;
            if (WiFi.hostByName(target.c_str(), resolved) == 1 &&
                ipaddr_aton(resolved.toString().c_str(), &cached_ip)) {

                Serial.println("[WiFi] Ping: " + target + " → " + resolved.toString());
                cached_ok = true;
            } else {

                Serial.println("[WiFi] Ping: résolution DNS échouée pour " + target);
            }
        }
    }
    if (cached_ok) {

        out = cached_ip;
    }
    return cached_ok;
}

// Start an asynchronous ping
static void ping_mqtt_broker(String& target) {

    if (!ping_in_progress) {

        ip_addr_t ip_target;
        if (!resolve_target(target, ip_target)) {

            // Pas d'IP exploitable : on ne lance pas de ping ce cycle.
            return;
        }

        esp_ping_config_t cfg   = ESP_PING_DEFAULT_CONFIG();
        cfg.target_addr         = ip_target;
        cfg.count               = PING_COUNT;
        cfg.timeout_ms          = PING_TIMEOUT_MS;
        cfg.task_stack_size     = 4096;
        cfg.task_prio           = 1;

        esp_ping_callbacks_t cbs = {

            .cb_args         = NULL,
            .on_ping_success = cb_ping_success,
            .on_ping_timeout = cb_ping_timeout,
            .on_ping_end     = cb_ping_end,
        };

        if (esp_ping_new_session(&cfg, &cbs, &s_ping_hdl) == ESP_OK) {

            esp_ping_start(s_ping_hdl);
            ping_in_progress = true;
            //Serial.printf("[WiFi] Ping → %s\n", target.c_str());
        } else {

            Serial.println("[WiFi] Ping session creation failed");
        }
    }
}

static void activate_ap(void) {

    ap_mode   = true;
    sta_connected = false;
    // NB: on conserve volontairement le SSID en NVS pour permettre une
    // reconnexion automatique au prochain reboot quand la box revient.
    WiFi.mode(WIFI_AP);

    // Sans cette config, l'ESP32 démarre son AP sur 192.168.4.1 par défaut,
    // alors que tout le portail annonce AP_IP. On force donc l'IP softAP.
    IPAddress ap_ip, ap_mask(255, 255, 255, 0);
    ap_ip.fromString(AP_IP);
    if (!WiFi.softAPConfig(ap_ip, ap_ip, ap_mask)) {

        Serial.println("[WiFi] softAPConfig failed");
    }

    if (strlen(AP_PASSWORD) > 0) {

        WiFi.softAP(AP_SSID, AP_PASSWORD);
    } else {

        WiFi.softAP(AP_SSID);
    }
    Serial.println("[WiFi] AP started: " AP_SSID " → " + WiFi.softAPIP().toString());
    webportal_setup(true);
    Serial.println("[WiFi] WebPortal for AP done");
}

static void activate_sta(void) {

    bool now_connected;
    uint32_t start;
    Serial.print("[WiFi] Connecting to: ");
    Serial.println(g_config.wifi_ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(g_config.wifi_ssid.c_str(), g_config.wifi_pass.c_str());

    now_connected = true;
    start = millis();
    while (WiFi.status() != WL_CONNECTED) {

        Serial.print(".");
        if (millis() - start > WIFI_CONNECT_TIMEOUT_MS) {

            now_connected = false;
            break;
        }
        delay(500);
    }
    if (now_connected) {

        // Connexion réussie
        ap_mode = false;
        Serial.println("\n[WiFi] Connected: " + WiFi.localIP().toString());

        if (MDNS.begin(g_config.hostname.c_str())) {

            Serial.println("[WiFi] mDNS: " + g_config.hostname + ".local");
        }
        webportal_setup(false);
        Serial.println("[WiFi] WebPortal for STA done");
        ping_last_ok_ms = 0;
    }
    sta_connected = now_connected;
}

// -------------------------------------------------------
//  API
// -------------------------------------------------------

// Retourne le nombre de secondes sans ping réussi (0 = tout va bien)
uint32_t wifi_ping_elapsed_s(void) {

    uint32_t elapsed_s = 0;
    if (sta_connected && ping_last_ok_ms) {

        elapsed_s = (millis() - ping_last_ok_ms) / 1000;
    }
    return elapsed_s;
}

// Réarme les horodatages du ping-watchdog. À appeler après un light sleep, car
// millis() a sauté de la durée du sommeil : sans cela, la fenêtre de perte de
// connectivité serait dépassée d'un coup et provoquerait une fausse bascule AP.
void wifi_reset_ping_watchdog(void) {

    uint32_t now       = millis();
    ping_last_ok_ms    = now;
    ping_last_check_ms = now;
}

bool wifi_setup(void) {

    if (g_config.wifi_ssid.isEmpty()) {

        // No SSID → switch to AP mode
        Serial.println("[WiFi] No SSID configured → AP mode");
        activate_ap();
    } else {

        activate_sta();
        if (!sta_connected) {

            Serial.println("[WiFi] Connection Timeout → Fallback to AP mode");
            WiFi.disconnect(true);
            activate_ap();
        }
    }
    return ap_mode;
}

String wifi_get_ip_address(void) {

    return ap_mode ? String(AP_IP) : WiFi.localIP().toString();
}

// Returns true when the device is in STA mode (i.e. the application must run).
// In AP mode the app is paused (no broker on the config network).
bool wifi_loop(void) {

    bool app_should_run = !ap_mode;

    if (sta_connected && !g_config.mqtt_host.isEmpty()) {

        uint32_t now = millis();

        // Start the connectivity-loss window from now (not from boot epoch 0).
        if (ping_last_ok_ms == 0) {

            ping_last_ok_ms = now;
        }
        // ---- Ping the broker every PING_INTERVAL_S seconds ----
        if ((now - ping_last_check_ms) >= (uint32_t)(PING_INTERVAL_S * 1000UL)) {

            ping_last_check_ms = now;
            ping_mqtt_broker(g_config.mqtt_host);
        }
        // ---- Watchdog: switch to AP after WIFI_LOSS_FALLBACK_S without any ping reply ----
        uint32_t loss_s = (now - ping_last_ok_ms) / 1000;
        if (loss_s >= WIFI_LOSS_FALLBACK_S) {

            Serial.printf("[WiFi] No ping reply for %u s → switching to AP mode\n", loss_s);
            WiFi.disconnect(true);
            delay(200);
            activate_ap();
            app_should_run = false;
        }
    }
    webportal_loop();
    return app_should_run;
}
