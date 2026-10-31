#include "bedctrl.h"

#define CMD_NONE             0
#define CMD_START_HEAD_UP    BIT(0)
#define CMD_STOP_HEAD_UP     BIT(1)
#define CMD_START_HEAD_DOWN  BIT(2)
#define CMD_STOP_HEAD_DOWN   BIT(3)
#define CMD_START_FOOT_UP    BIT(4)
#define CMD_STOP_FOOT_UP     BIT(5)
#define CMD_START_FOOT_DOWN  BIT(6)
#define CMD_STOP_FOOT_DOWN   BIT(7)
#define CMD_SYNCH_TO_TARGET  BIT(8)
#define CMD_LIGHT_SWITCH_ON  BIT(9)

static void on_message(void* context, String& sub_topic, String& message) {

    JsonDocument document;
    DeserializationError error = deserializeJson(document, message);
    if (error) {

        Serial.println("MQTT unexpected message: " + message);
        Serial.println(error.f_str());
        return;
    }
    if (!(document.containsKey("source") && document.containsKey("beds") &&
          document.containsKey("parts") && document.containsKey("push"))) {

        Serial.println("MQTT unsupported message: " + message);
        return;
    }

    BedCtrl* bedctrl = (BedCtrl *)context;
    String source = document["source"];
    String push = document["push"];
    Serial.println("MQTT message: " + message);
    byte bed_mask = 0;
    if ((document["beds"] == "left") || (document["beds"] == "both")) {

        bed_mask |= BED2_MASK; // BED2 = Left
    }
    if ((document["beds"] == "right") || (document["beds"] == "both")) {

        bed_mask |= BED1_MASK; // BED1 = Right
    }

    // A press on up/down/flat/same while a reset(flat) or synch(same) is in
    // progress stops it; the press is then consumed (no new motion started).
    if ((push == "on") &&
        (source == "up" || source == "down" || source == "flat" || source == "same")) {

        byte stop_mask = (source == "same") ? (byte)(BED1_MASK | BED2_MASK) : bed_mask;
        if (bedctrl->stop_synch(stop_mask)) {

            return;
        }
    }

    if (source == "same") {

        if (push == "on") {

            bedctrl->synch();
        }
    } else {

        if (source == "flat") {

            if (push == "on") {

                bedctrl->reset(bed_mask);
            }
        } else if (source == "light") {

            // Toggle once per tap: act on the press only, ignore the release.
            if (push == "on") {

                bedctrl->set_light(bed_mask);
            }
        } else {

            for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

                if (bed_mask & BIT(bed)) {

                    if (source == "up") {

                        if ((document["parts"] == "head") || (document["parts"] == "both")) {

                            if (push == "on") {

                                Serial.println("BED" + String(bed) + ": HEAD UP started...");
                                bedctrl->add_cmd(bed, CMD_START_HEAD_UP, CMD_START_HEAD_DOWN);

                            } else {

                                bedctrl->add_cmd(bed, CMD_STOP_HEAD_UP, CMD_START_HEAD_UP);
                            }
                        }
                        if ((document["parts"] == "leg") || (document["parts"] == "both")) {

                            if (push == "on") {

                                Serial.println("BED" + String(bed) + ": FOOT UP started...");
                                bedctrl->add_cmd(bed, CMD_START_FOOT_UP, CMD_START_FOOT_DOWN);

                            } else {

                                bedctrl->add_cmd(bed, CMD_STOP_FOOT_UP, CMD_START_FOOT_UP);
                            }
                        }
                    } else if (source == "down") {

                        if ((document["parts"] == "head") || (document["parts"] == "both")) {

                            if (push == "on") {

                                Serial.println("BED" + String(bed) + ": HEAD DOWN started...");
                                bedctrl->add_cmd(bed, CMD_START_HEAD_DOWN, CMD_START_HEAD_UP);

                            } else {

                                bedctrl->add_cmd(bed, CMD_STOP_HEAD_DOWN, CMD_START_HEAD_DOWN);
                            }
                        }
                        if ((document["parts"] == "leg") || (document["parts"] == "both")) {

                            if (push == "on") {

                                Serial.println("BED" + String(bed) + ": FOOT DOWN started...");
                                bedctrl->add_cmd(bed, CMD_START_FOOT_DOWN, CMD_START_FOOT_UP);

                            } else {

                                bedctrl->add_cmd(bed, CMD_STOP_FOOT_DOWN, CMD_START_FOOT_DOWN);
                            }
                        }
                    } else if (source == "m1") {

                        bedctrl->memory(bed, 1, push == "long");
                    } else if (source == "m2") {

                        bedctrl->memory(bed, 2, push == "long");
                    } else {

                        Serial.println("MQTT unsupported source: " + source);
                    }
                }
            }
        }
    }
}

BedCtrl::BedCtrl(uint loop_time_ms) {

    if (wifi_setup()) {

        Serial.println("[Setup] AP mode — waiting for WiFi config");
    } else {

        Serial.println("[Setup] BedCtrl — working...");
    }

    this->loop_time_ms = loop_time_ms;

    // MQTT
    this->mqtt = mqtt_create(on_message, this);

    // App. init.
    for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

        // MOTORS
        this->command[bed] = CMD_NONE;
        this->head_pos[bed] = 0;
        this->foot_pos[bed] = 0;
        this->head_tgt[bed] = -1;
        this->foot_tgt[bed] = -1;
        for (uint idx = 0 ; idx < NB_IO_PINS ; idx++) {

            pinMode(io_pins[bed][idx], OUTPUT);
            digitalWrite(io_pins[bed][idx], HIGH);
        }
        // LEDS
        this->light_tgt[bed] = -1;
        pinMode(io_lights[bed][LIGHT_PWR], OUTPUT);
        digitalWrite(io_lights[bed][LIGHT_PWR], HIGH);
        this->leds[bed] = new Adafruit_NeoPixel(60, io_lights[bed][LIGHT_CMD], NEO_RGB + NEO_KHZ800);
        this->leds[bed]->begin();
        this->leds[bed]->show();
    }
}

void BedCtrl::set_light(byte bed_mask)  {

    // Toggle: turn the light ON if it is currently OFF, OFF if it is ON.
    // light_tgt[bed] < 0 means OFF; >= 0 means ON or powering up.
    for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

        if (bed_mask & BIT(bed)) {

            if (this->light_tgt[bed] < 0) {

                // OFF → ON : step 1 power on now, step 2 LEDs after light_delay cycles
                digitalWrite(io_lights[bed][LIGHT_PWR], LOW);
                this->light_tgt[bed] = g_config.light_delay;
                this->command[bed] |= CMD_LIGHT_SWITCH_ON;
                Serial.println("BED" + String(bed) + ": light powering on, delay=" + String(this->light_tgt[bed]));
            } else {

                // ON (or powering up) → OFF
                Serial.println("BED" + String(bed) + ": light=OFF");
                for(uint led = 0 ; led < this->leds[bed]->numPixels() ; led++) {

                    this->leds[bed]->setPixelColor(led, this->leds[bed]->Color(0, 0, 0));
                }
                this->leds[bed]->show();
                digitalWrite(io_lights[bed][LIGHT_PWR], HIGH);
                this->light_tgt[bed] = -1;
                this->command[bed] &= ~CMD_LIGHT_SWITCH_ON;
            }
        }
    }
}

void BedCtrl::add_cmd(byte bed, uint append, uint clear) {

    this->command[bed] = (this->command[bed] & ~clear) | append;
}

void BedCtrl::reset(byte bed_mask) {

    for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

        if (bed_mask & BIT(bed)) {

            this->head_pos[bed] += 20;
            this->foot_pos[bed] += 20;
            this->set_target(bed, 0, 0);
        }
    }
}

void BedCtrl::synch(void) {

    // -1 = no move for that bed; the bed that is behind catches up to the other
    int head_tgt[NB_BEDS] = { -1, -1 };
    int foot_tgt[NB_BEDS] = { -1, -1 };
    if (this->head_pos[BED1] > this->head_pos[BED2]) {

        head_tgt[BED2] = this->head_pos[BED1];
    } else if (this->head_pos[BED1] < this->head_pos[BED2]) {

        head_tgt[BED1] = this->head_pos[BED2];
    }
    if (this->foot_pos[BED1] > this->foot_pos[BED2]) {

        foot_tgt[BED2] = this->foot_pos[BED1];
    } else if (this->foot_pos[BED1] < this->foot_pos[BED2]) {

        foot_tgt[BED1] = this->foot_pos[BED2];
    }
    for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

        this->set_target(bed, head_tgt[bed], foot_tgt[bed]);
    }
}

bool BedCtrl::stop_synch(byte bed_mask) {

    bool stopped = false;
    for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

        if ((bed_mask & BIT(bed)) && (this->command[bed] & CMD_SYNCH_TO_TARGET)) {

            // Stop all motors and cancel the pending target for this bed
            for (uint idx = 0 ; idx < NB_IO_PINS ; idx++) {

                digitalWrite(io_pins[bed][idx], HIGH);
            }
            this->command[bed] &= ~CMD_SYNCH_TO_TARGET;
            this->head_tgt[bed] = -1;
            this->foot_tgt[bed] = -1;
            Serial.println("BED" + String(bed) + ": reset/synch stopped by button");
            stopped = true;
        }
    }
    return stopped;
}

void BedCtrl::memory(byte bed, uint unit, bool is_long) {

    uint slot = unit - 1;
    if (is_long) {

        // Long press → store current position into persistent memory (NVS)
        g_config.mem_head[slot][bed] = this->head_pos[bed];
        g_config.mem_foot[slot][bed] = this->foot_pos[bed];
        config_save();
        Serial.println("BED" + String(bed) + ": store M" + String(unit) +
                       " => head=" + String(this->head_pos[bed]) +
                       ", foot=" + String(this->foot_pos[bed]));
    } else {

        // Short press → move bed to the stored memory position
        this->set_target(bed, g_config.mem_head[slot][bed], g_config.mem_foot[slot][bed]);
        Serial.println("BED" + String(bed) + ": restore M" + String(unit) +
                       " => head=" + String(g_config.mem_head[slot][bed]) +
                       ", foot=" + String(g_config.mem_foot[slot][bed]));
    }
}

void BedCtrl::set_target(byte bed, int head, int foot) {

    switch (bed) {

        case BED1:
        case BED2:
            this->head_tgt[bed] = head;
            this->foot_tgt[bed] = foot;
            this->command[bed] = CMD_SYNCH_TO_TARGET;
            Serial.println("BED" + String(bed) + ": SYNCH started => target: head=" + String(head) + ", foot=" + String(foot));
            break;
    }
}

void BedCtrl::do_command(uint bed) {

    // RESET
    if (this->command[bed] & CMD_SYNCH_TO_TARGET) {

        if (this->head_tgt[bed] >= 0) {

            if (this->head_pos[bed] > this->head_tgt[bed]) {

                digitalWrite(io_pins[bed][HEAD_DOWN], LOW);
                this->head_pos[bed]--;
                Serial.println("BED" + String(bed) + ": head=" + String(this->head_pos[bed]));
            } else if (this->head_pos[bed] < this->head_tgt[bed]) {

                digitalWrite(io_pins[bed][HEAD_UP], LOW);
                this->head_pos[bed]++;
                Serial.println("BED" + String(bed) + ": head=" + String(this->head_pos[bed]));
            } else {

                digitalWrite(io_pins[bed][HEAD_DOWN], HIGH);
                digitalWrite(io_pins[bed][HEAD_UP], HIGH);
                this->head_tgt[bed] = -1;
            }
        }
        if (this->foot_tgt[bed] >= 0) {

            if (this->foot_pos[bed] > this->foot_tgt[bed]) {

                digitalWrite(io_pins[bed][FOOT_DOWN], LOW);
                this->foot_pos[bed]--;
                Serial.println("BED" + String(bed) + ": foot=" + String(this->foot_pos[bed]));
            } else if (this->foot_pos[bed] < this->foot_tgt[bed]) {

                digitalWrite(io_pins[bed][FOOT_UP], LOW);
                this->foot_pos[bed]++;
                Serial.println("BED" + String(bed) + ": foot=" + String(this->foot_pos[bed]));
            } else {

                digitalWrite(io_pins[bed][FOOT_DOWN], HIGH);
                digitalWrite(io_pins[bed][FOOT_UP], HIGH);
                this->foot_tgt[bed] = -1;
            }
        }
        if ((this->head_tgt[bed] < 0) && (this->foot_tgt[bed] < 0)) {

            this->command[bed] &= ~CMD_SYNCH_TO_TARGET;
            Serial.println("BED" + String(bed) + ": SYNCH done: head=" + String(this->head_pos[bed]) + ", foot=" + String(this->foot_pos[bed]));
        }
    } else {

        // HEAD UP
        uint max = (20000 / this->loop_time_ms);
        if (this->command[bed] & CMD_START_HEAD_UP) {

            if (this->head_pos[bed] < max) {
                
                digitalWrite(io_pins[bed][HEAD_UP], LOW);
                this->head_pos[bed]++;
            }
            Serial.println("BED" + String(bed) + ": head=" + String(this->head_pos[bed]));
        } else if (this->command[bed] & CMD_STOP_HEAD_UP) {

            digitalWrite(io_pins[bed][HEAD_UP], HIGH);
            this->command[bed] &= ~CMD_STOP_HEAD_UP;
            Serial.println("BED" + String(bed) + ": HEAD UP done!");
        }
        // HEAD DOWN
        if (this->command[bed] & CMD_START_HEAD_DOWN) {

            if (this->head_pos[bed] > 0) {

                digitalWrite(io_pins[bed][HEAD_DOWN], LOW);
                this->head_pos[bed]--;
            }
            Serial.println("BED" + String(bed) + ": head=" + String(this->head_pos[bed]));
        } else if (this->command[bed] & CMD_STOP_HEAD_DOWN) {

            digitalWrite(io_pins[bed][HEAD_DOWN], HIGH);
            this->command[bed] &= ~CMD_STOP_HEAD_DOWN;
            Serial.println("BED" + String(bed) + ": HEAD DOWN done!");
        }
        // FOOT UP
        if (this->command[bed] & CMD_START_FOOT_UP) {

            if (this->foot_pos[bed] < max) {

                digitalWrite(io_pins[bed][FOOT_UP], LOW);
                this->foot_pos[bed]++;
            }
            Serial.println("BED" + String(bed) + ": foot=" + String(this->foot_pos[bed]));
        } else if (this->command[bed] & CMD_STOP_FOOT_UP) {

            digitalWrite(io_pins[bed][FOOT_UP], HIGH);
            this->command[bed] &= ~CMD_STOP_FOOT_UP;
            Serial.println("BED" + String(bed) + ": FOOT UP done!");
        }
        // FOOT DOWN
        if (this->command[bed] & CMD_START_FOOT_DOWN) {

            if (this->foot_pos[bed] > 0) {

                digitalWrite(io_pins[bed][FOOT_DOWN], LOW);
                this->foot_pos[bed]--;
            }
            Serial.println("BED" + String(bed) + ": foot=" + String(this->foot_pos[bed]));
        } else if (this->command[bed] & CMD_STOP_FOOT_DOWN) {

            digitalWrite(io_pins[bed][FOOT_DOWN], HIGH);
            this->command[bed] &= ~CMD_STOP_FOOT_DOWN;
            Serial.println("BED" + String(bed) + ": FOOT DOWN done!");
        }
        // LIGHT SWITCH ON
        if (this->command[bed] & CMD_LIGHT_SWITCH_ON) {

            if (this->light_tgt[bed] > 0) {

                this->light_tgt[bed]--;
                Serial.println("BED" + String(bed) + ": light=" + String(this->light_tgt[bed]));
            } else if (!this->light_tgt[bed]) {

                Serial.println("BED" + String(bed) + ": light=ON");
                for(uint led = 0 ; led < this->leds[bed]->numPixels() ; led++) {

                    this->leds[bed]->setPixelColor(led, this->leds[bed]->Color(255, 255, 255));
                }
                this->leds[bed]->show();
                this->command[bed] &= ~CMD_LIGHT_SWITCH_ON;
            }
        }
    }
}

void BedCtrl::loop(void) {

    // wifi_loop() returns true in STA mode (app runs), false in AP mode (paused).
    if (wifi_loop()) {

        for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

            this->do_command(bed);
        }
        this->mqtt->loop();
    }
}
