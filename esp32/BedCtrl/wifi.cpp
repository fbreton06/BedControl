#include <WiFi.h>
#include <ESPmDNS.h>
#include "globals.h"

// -------------------------------------------------------
//  WiFi : uniquement pour le portail web (config + OTA).
//  Le lit est piloté en BLE et fonctionne quel que soit l'état du WiFi.
//    - démarrage : STA si un SSID est configuré, sinon WiFi coupé
//    - mode AP   : uniquement sur la commande BLE "wifi_ap"
// -------------------------------------------------------
static bool sta_connected = false;
static bool ap_mode = false;

static void activate_ap(void) {

    ap_mode   = true;
    sta_connected = false;
    // NB: on conserve volontairement le SSID en NVS pour permettre une
    // reconnexion automatique au prochain reboot.
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
    }
    sta_connected = now_connected;
}

// -------------------------------------------------------
//  API
// -------------------------------------------------------
void wifi_setup(void) {

    if (g_config.wifi_ssid.isEmpty()) {

        Serial.println("[WiFi] No SSID configured → WiFi off (AP on BLE request)");
        WiFi.mode(WIFI_OFF);
    } else {

        activate_sta();
        if (!sta_connected) {

            Serial.println("[WiFi] Connection Timeout → WiFi off (AP on BLE request)");
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
        }
    }
}

// Bascule en mode AP (portail de config) — demandé par l'appli en BLE.
// Le retour en STA se fait par redémarrage (sauvegarde du portail ou BLE).
void wifi_start_ap(void) {

    if (!ap_mode) {

        WiFi.disconnect(true);
        delay(200);
        activate_ap();
    }
}

String wifi_get_ip_address(void) {

    if (ap_mode) {

        return String(AP_IP);
    }
    return sta_connected ? WiFi.localIP().toString() : String("");
}

const char* wifi_get_mode(void) {

    return ap_mode ? "ap" : (sta_connected ? "sta" : "off");
}

void wifi_loop(void) {

    webportal_loop();
}
