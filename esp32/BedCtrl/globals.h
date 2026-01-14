#ifndef __GLOBALS__
#define __GLOBALS__

#include <Arduino.h>
#include <assert.h>

/*

 USE Arduino card: ESP32 WROOM DA Module

*/
#define DEBUG_EMUL 1
#define DEBUG_PID_TRACE 0
#define DEBUG_TRACE 1

#define HOSTNAME "bedctrl"      // <HOSTNAME>.local

#define VERSION "V1.0.0"
#define MQTT_SUB_TOPIC "bed"
#define MQTT_PUB_TOPIC "server/bed"
#define WIFI_SSID " set wifi ssid here "
#define WIFI_PASSWORD " set wifi password here "

#define OTA_USERNAME " set username for ota login "
#define OTA_PASSWORD " set password for ota login "

#define MQTT_BROKER_ADDRESS " set ip address of the mqtt broker here "
#define MQTT_BROKER_PORT 1883
#define MQTT_USERNAME " set the mqtt broker usename here "
#define MQTT_PASSWORD " set the mqtt broker password here "

#define MQTT_ID "bedctrl"

// MACRO
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define BIT(x)        (1UL << (x))

typedef unsigned char byte;

// OTA
void ota_setup(void);
void ota_loop(void);
// WIFI
String wifi_get_ip_address(void);
void wifi_setup(void);
bool wifi_loop(void);
#endif // __GLOBALS__
