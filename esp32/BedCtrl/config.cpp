#include "globals.h"

DeviceConfig g_config;

static Preferences prefs;

void config_load(void) {

    prefs.begin(NVS_NAMESPACE, true); // read-only
    g_config.hostname       = prefs.getString(NVS_KEY_HOSTNAME, DEFAULT_HOSTNAME);
    g_config.wdt_s          = prefs.getUInt  (NVS_KEY_WDT_S, DEFAULT_WDT_TIMEOUT_S);
    g_config.wifi_ssid      = prefs.getString(NVS_KEY_SSID, "");
    g_config.wifi_pass      = prefs.getString(NVS_KEY_PASS, "");
    g_config.mqtt_host      = prefs.getString(NVS_KEY_MQTT_HOST, "");
    g_config.mqtt_port      = prefs.getUInt  (NVS_KEY_MQTT_PORT, 1883);
    g_config.mqtt_user      = prefs.getString(NVS_KEY_MQTT_USER, "");
    g_config.mqtt_pass      = prefs.getString(NVS_KEY_MQTT_PASS, "");
    g_config.mqtt_topic     = prefs.getString(NVS_KEY_MQTT_TOPIC, "");
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
    Serial.println("\tMQTT     : " + g_config.mqtt_host + ":" + String(g_config.mqtt_port));
    Serial.println("\tTopic pub: " + g_config.mqtt_topic);
}

void config_save(void) {

    prefs.begin(NVS_NAMESPACE, false); // read-write
    prefs.putString(NVS_KEY_HOSTNAME,    g_config.hostname);
    prefs.putUInt  (NVS_KEY_WDT_S,       g_config.wdt_s);
    prefs.putString(NVS_KEY_SSID,        g_config.wifi_ssid);
    prefs.putString(NVS_KEY_PASS,        g_config.wifi_pass);
    prefs.putString(NVS_KEY_MQTT_HOST,   g_config.mqtt_host);
    prefs.putUInt  (NVS_KEY_MQTT_PORT,   g_config.mqtt_port);
    prefs.putString(NVS_KEY_MQTT_USER,   g_config.mqtt_user);
    prefs.putString(NVS_KEY_MQTT_PASS,   g_config.mqtt_pass);
    prefs.putString(NVS_KEY_MQTT_TOPIC,  g_config.mqtt_topic);
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

void config_reset(void) {

    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();

    Serial.println("[Config] NVS cleared — rebooting...");
    delay(500);
    ESP.restart();
}
