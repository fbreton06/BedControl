#include <WiFi.h>
#include <ESPmDNS.h>
#include "globals.h"

static bool connected;
static const char ssid[] = WIFI_SSID;

void wifi_setup(void) {

    const char password[] = WIFI_PASSWORD;
    WiFi.begin(ssid, password);
    connected = (WiFi.status() == WL_CONNECTED);
}

String wifi_get_ip_address(void) {

    return(WiFi.localIP().toString());
}

bool wifi_loop(void) {

    if (WiFi.status() == WL_CONNECTED)  {

        if (!connected) {

            // Connection
            Serial.println("WIFI ready:");
            Serial.print("Connected to ");
            Serial.println(ssid);
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            // http://<host>.local
            const char host[] = HOSTNAME;
            if (MDNS.begin(host)) {

                Serial.println("mDNS responder started");
                connected = true;
            } else {

                Serial.println("Error setting up MDNS responder!");
            }
        }
    } else {

        connected = false;
    }
    return connected;
}
