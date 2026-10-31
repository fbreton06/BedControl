#ifndef __GLOBALS__
#define __GLOBALS__

#include <Arduino.h>
#include <assert.h>
#include <Preferences.h>

/*

 USE Arduino card: ESP32 WROOM DA Module

*/
#define DEBUG_EMUL          1
#define DEBUG_PID_TRACE     0
#define DEBUG_TRACE         1

#define VERSION "V2.0.0"

// Identifiant unique de ce device (modifiable via portail web)
#define DEFAULT_HOSTNAME  "bedctrl"       // <HOSTNAME>.local


// Nom du point d'accès en mode AP (fallback)
#define AP_SSID           "BedCtrlConfig"
#define AP_PASSWORD       "" // Réseau ouvert en mode config
#define AP_IP             "192.168.4.1"

// Durée max d'attente WiFi avant bascule AP (ms)
#define WIFI_CONNECT_TIMEOUT_MS  10000

// Surveillance de connectivité par ping
#define PING_INTERVAL_S          30               // Ping toutes les 30 secondes
#define PING_TIMEOUT_MS          3000             // Timeout d'un ping (ms)
#define PING_COUNT               1                // Un seul ping par cycle
#define WIFI_LOSS_FALLBACK_S     100              // Bascule AP après 100 s sans réponse ping

// Watchdog par défaut (secondes) — écrasé par NVS si configuré
#define DEFAULT_WDT_TIMEOUT_S    5

// Délai par défaut entre la mise sous tension de la lumière et l'allumage des
// LEDs, exprimé en cycles de boucle (LOOP_TIME_MS = 100 ms) — écrasé par NVS.
#define DEFAULT_LIGHT_DELAY      5


// MACRO
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define BIT(x)        (1UL << (x))

// Beds & memory slots (M1/M2)
#define NB_BEDS       2
#define NB_MEM_UNITS  2

typedef unsigned char byte;

// -------------------------------------------------------
//  NVS — clés de stockage persistant
// -------------------------------------------------------
#define NVS_NAMESPACE       "esp32cfg"
#define NVS_KEY_HOSTNAME    "hostname"
#define NVS_KEY_WDT_S       "wdt_s"
#define NVS_KEY_SSID        "wifi_ssid"
#define NVS_KEY_PASS        "wifi_pass"
#define NVS_KEY_MQTT_HOST   "mqtt_host"
#define NVS_KEY_MQTT_PORT   "mqtt_port"
#define NVS_KEY_MQTT_USER   "mqtt_user"
#define NVS_KEY_MQTT_PASS   "mqtt_pass"
#define NVS_KEY_MQTT_TOPIC  "mqtt_topic"
#define NVS_KEY_LIGHT_DELAY "light_delay"

// -------------------------------------------------------
//  Config runtime chargée depuis NVS au démarrage
// -------------------------------------------------------
struct DeviceConfig {

    /* Global network settings */
    String  hostname; // Used as MQTT Id and as sub topic root 
    uint    wdt_s;
    /* WIFI AD HOC settings */
    String  wifi_ssid;
    String  wifi_pass;
    /* MQTT settings */
    String  mqtt_host;
    uint    mqtt_port;
    String  mqtt_user;
    String  mqtt_pass;
    String  mqtt_topic; // Used as MQTT pub topic target
    /* APP settings */
    uint    light_delay; // Loop cycles between light power-on and LED switch-on
    uint    mem_head[NB_MEM_UNITS][NB_BEDS]; // Stored head positions per memory slot [M1/M2][bed]
    uint    mem_foot[NB_MEM_UNITS][NB_BEDS]; // Stored foot positions per memory slot [M1/M2][bed]
};

extern DeviceConfig g_config;

void config_load(void);
void config_save(void);
void config_reset(void);

// -------------------------------------------------------
//  OTA / Web portal
// -------------------------------------------------------
void webportal_setup(bool ap_mode);
void webportal_loop(void);

// -------------------------------------------------------
//  WIFI
// -------------------------------------------------------
String wifi_get_ip_address(void);
bool wifi_setup(void);
bool wifi_loop(void); // returns true when the device is in STA mode (app should run)
uint32_t wifi_ping_elapsed_s(void);
void wifi_reset_ping_watchdog(void); // réarme les horodatages après un light sleep

#endif // __GLOBALS__
