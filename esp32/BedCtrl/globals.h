#ifndef __GLOBALS__
#define __GLOBALS__

#include <Arduino.h>
#include <assert.h>
#include <Preferences.h>
#include <ArduinoJson.h>

/*

 USE Arduino card: ESP32 WROOM DA Module
     Partition Scheme: Minimal SPIFFS (1.9MB APP with OTA) — requis par la pile BLE

*/
#define DEBUG_EMUL          0
#define DEBUG_PID_TRACE     0
#define DEBUG_TRACE         0

#define VERSION "V3.0.0"

// Identifiant unique de ce device (modifiable via portail web)
#define DEFAULT_HOSTNAME  "bedctrl"       // <HOSTNAME>.local


// Point d'accès du portail de config, activé par la commande BLE "wifi_ap"
#define AP_SSID           "BedCtrlConfig"
#define AP_PASSWORD       "" // Réseau ouvert en mode config
#define AP_IP             "192.168.4.1"

// Service BLE (toujours actif) : commandes du lit + configuration.
// Ces UUID sont partagés avec l'appli (bed_ctrl_app/lib/ble_service.dart).
#define BLE_SERVICE_UUID         "6e1d0001-8b4f-4c1a-9a3e-b3d1c0ffee01"
#define BLE_CHAR_CONFIG_UUID     "6e1d0002-8b4f-4c1a-9a3e-b3d1c0ffee01" // R/W    : config JSON
#define BLE_CHAR_STATUS_UUID     "6e1d0003-8b4f-4c1a-9a3e-b3d1c0ffee01" // R/notify : résultat
#define BLE_CHAR_COMMAND_UUID    "6e1d0004-8b4f-4c1a-9a3e-b3d1c0ffee01" // W      : commande JSON
#define BLE_RESTART_DELAY_MS     2000   // Laisse à l'appli le temps de recevoir STATUS
#define BLE_MSG_MAX_LEN          384    // Taille max d'un JSON reçu (config ou commande)
#define BLE_QUEUE_LEN            16     // Messages en attente de traitement par loop()

// Durée max d'attente de la connexion WiFi au démarrage (ms)
#define WIFI_CONNECT_TIMEOUT_MS  10000

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
#define NVS_KEY_LIGHT_DELAY "light_delay"

// -------------------------------------------------------
//  Config runtime chargée depuis NVS au démarrage
// -------------------------------------------------------
struct DeviceConfig {

    /* Global network settings */
    String  hostname; // Nom BLE et mDNS
    uint    wdt_s;
    /* WIFI AD HOC settings */
    String  wifi_ssid;
    String  wifi_pass;
    /* APP settings */
    uint    light_delay; // Loop cycles between light power-on and LED switch-on
    uint    mem_head[NB_MEM_UNITS][NB_BEDS]; // Stored head positions per memory slot [M1/M2][bed]
    uint    mem_foot[NB_MEM_UNITS][NB_BEDS]; // Stored foot positions per memory slot [M1/M2][bed]
};

extern DeviceConfig g_config;

void config_load(void);
void config_save(void);
void config_reset(void);
bool config_update_from_json(const String& json, String& error); // n'appelle pas config_save()

// -------------------------------------------------------
//  OTA / Web portal
// -------------------------------------------------------
void webportal_setup(bool ap_mode);
void webportal_loop(void);

// -------------------------------------------------------
//  BLE (commandes du lit + configuration)
// -------------------------------------------------------
typedef void (*ble_on_command_callback)(void* context, JsonDocument& command);
typedef void (*ble_on_disconnect_callback)(void* context);

void ble_setup(ble_on_command_callback on_command, ble_on_disconnect_callback on_disconnect, void* context);
void ble_loop(void); // traite les messages BLE reçus (appelé depuis loop())

// -------------------------------------------------------
//  WIFI
// -------------------------------------------------------
String wifi_get_ip_address(void);
const char* wifi_get_mode(void); // "sta", "ap" ou "off"
void wifi_setup(void);
void wifi_start_ap(void);
void wifi_loop(void);

#endif // __GLOBALS__
