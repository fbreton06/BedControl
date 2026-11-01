#include <ArduinoJson.h>
#include "globals.h"

DeviceConfig g_config;

static Preferences prefs;

void config_load(void) {

    prefs.begin(NVS_NAMESPACE, true); // read-only
    g_config.hostname       = prefs.getString(NVS_KEY_HOSTNAME, DEFAULT_HOSTNAME);
    g_config.wdt_s          = prefs.getUInt  (NVS_KEY_WDT_S, DEFAULT_WDT_TIMEOUT_S);
    g_config.wifi_ssid      = prefs.getString(NVS_KEY_SSID, "");
    g_config.wifi_pass      = prefs.getString(NVS_KEY_PASS, "");
    g_config.light_delay    = prefs.getUInt  (NVS_KEY_LIGHT_DELAY, DEFAULT_LIGHT_DELAY);
    for (uint u = 0 ; u < NB_MEM_UNITS ; u++) {

        for (uint b = 0 ; b < NB_BEDS ; b++) {

            g_config.mem_head[u][b] = prefs.getUInt(("mh" + String(u) + String(b)).c_str(), 0);
            g_config.mem_foot[u][b] = prefs.getUInt(("mf" + String(u) + String(b)).c_str(), 0);
        }
    }
    prefs.end();

    Serial.println("[Config] Loaded from NVS:");
    Serial.println("\tHostname : " + g_config.hostname);
    Serial.print  ("\tWDT (s)  : "); Serial.println(g_config.wdt_s);
    Serial.println("\tSSID     : " + g_config.wifi_ssid);
    Serial.print  ("\tLight dly: "); Serial.println(g_config.light_delay);
}

void config_save(void) {

    prefs.begin(NVS_NAMESPACE, false); // read-write
    prefs.putString(NVS_KEY_HOSTNAME,    g_config.hostname);
    prefs.putUInt  (NVS_KEY_WDT_S,       g_config.wdt_s);
    prefs.putString(NVS_KEY_SSID,        g_config.wifi_ssid);
    prefs.putString(NVS_KEY_PASS,        g_config.wifi_pass);
    prefs.putUInt  (NVS_KEY_LIGHT_DELAY, g_config.light_delay);
    for (uint u = 0 ; u < NB_MEM_UNITS ; u++) {

        for (uint b = 0 ; b < NB_BEDS ; b++) {

            prefs.putUInt(("mh" + String(u) + String(b)).c_str(), g_config.mem_head[u][b]);
            prefs.putUInt(("mf" + String(u) + String(b)).c_str(), g_config.mem_foot[u][b]);
        }
    }
    prefs.end();

    Serial.println("[Config] Saved to NVS");
}

// Applique à g_config les champs présents dans 'json' (format de /save-wifi,
// partagé par le portail AP et le service BLE). Les champs absents, vides
// (hostname) ou hors bornes sont ignorés.
// Retourne false (et 'error') si le JSON est invalide ou le SSID vide : dans
// ce cas g_config n'est pas modifié. Ne sauvegarde pas en NVS.
bool config_update_from_json(const String& json, String& error) {

    JsonDocument doc;
    if (deserializeJson(doc, json)) {

        error = "JSON invalide";
        return false;
    }
    if (doc.containsKey("wifi_ssid") && doc["wifi_ssid"].as<String>().isEmpty()) {

        error = "SSID requis";
        return false;
    }

    if (doc.containsKey("wifi_ssid")) {

        g_config.wifi_ssid = doc["wifi_ssid"].as<String>();
        Serial.println("SSID: " + g_config.wifi_ssid);
    }
    if (doc.containsKey("wifi_pass")) {

        // Vide autorisé (réseau ouvert)
        g_config.wifi_pass = doc["wifi_pass"].as<String>();
    }
    if (doc.containsKey("hostname") && !doc["hostname"].as<String>().isEmpty()) {

        g_config.hostname = doc["hostname"].as<String>();
        Serial.println("Host name: " + g_config.hostname);
    }
    if (doc.containsKey("wdt_s")) {

        uint value = doc["wdt_s"].as<uint>();
        if (value >= 3 && value <= 300) {

            g_config.wdt_s = value;
        }
        Serial.println("WDT(s): " + String(g_config.wdt_s));
    }
    if (doc.containsKey("light_delay")) {

        uint value = doc["light_delay"].as<uint>();
        if (value <= 600) {

            g_config.light_delay = value;
        }
    }
    return true;
}

void config_reset(void) {

    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();

    Serial.println("[Config] NVS cleared — rebooting...");
    delay(500);
    ESP.restart();
}
