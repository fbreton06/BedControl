#include <esp_task_wdt.h>
#include "globals.h"
#include "bedctrl.h"

#define IO_PIN_ON_BOARD_LED 2
#define LOOP_TIME_MS        100

static BedCtrl* bedctrl = nullptr;
static bool led = false;

void setup() {

    Serial.begin(115200);
    Serial.print("__________ REBOOT ");
    Serial.print(VERSION);
    Serial.println(" __________");

    // 1. Charger la config persistante (NVS)
    config_load();

    // 2. Watchdog avec timeout issu de la config
    esp_task_wdt_config_t wdt_config = {

        .timeout_ms   = g_config.wdt_s * 1000,
        .idle_core_mask = 1,
        .trigger_panic  = true,
    };
    esp_task_wdt_deinit();
    pinMode(IO_PIN_ON_BOARD_LED, OUTPUT);
    digitalWrite(IO_PIN_ON_BOARD_LED, LOW);
    bedctrl = new BedCtrl(LOOP_TIME_MS);
    Serial.println("[Setup] BedCtrl ready");
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
}

void loop() {

    bedctrl->loop();
    delay(LOOP_TIME_MS);
    esp_task_wdt_reset();
}
