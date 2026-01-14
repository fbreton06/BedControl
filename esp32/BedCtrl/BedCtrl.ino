#include <esp_task_wdt.h>
#include "bedctrl.h"

#define IO_PIN_ON_BOARD_LED 2

#define WDT_TIMEOUT_S 5
#define LOOP_TIME_MS 100

static BedCtrl* bedctrl;

void setup() {

    esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WDT_TIMEOUT_S * 1000,
      .idle_core_mask = 1,
      .trigger_panic = true,
    };

    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
    Serial.begin(115200);
    Serial.print("__________ REBOOT ");
    Serial.print(VERSION);
    Serial.println(" __________");
    wifi_setup();
    Serial.println("WIFI setup");
    ota_setup();
    Serial.println("OTA setup");
    esp_task_wdt_reset();
    bedctrl = new BedCtrl(LOOP_TIME_MS);
    Serial.println("Setup done");
    pinMode(IO_PIN_ON_BOARD_LED, OUTPUT);
    digitalWrite(IO_PIN_ON_BOARD_LED, LOW);
}

void loop() {

    bool connected = wifi_loop();
    if (connected) {

        ota_loop();
    }
    bedctrl->loop();
    delay(LOOP_TIME_MS);
    esp_task_wdt_reset();
}
