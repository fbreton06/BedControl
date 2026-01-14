#include "bedctrl.h"

#define CMD_NONE            0
#define CMD_START_HEAD_UP   BIT(0)
#define CMD_STOP_HEAD_UP    BIT(1)
#define CMD_START_HEAD_DOWN BIT(2)
#define CMD_STOP_HEAD_DOWN  BIT(3)
#define CMD_START_FOOT_UP   BIT(4)
#define CMD_STOP_FOOT_UP    BIT(5)
#define CMD_START_FOOT_DOWN BIT(6)
#define CMD_STOP_FOOT_DOWN  BIT(7)
#define CMD_SYNCH_TO_TARGET BIT(8)
#define CMD_LIGHT_SWITCH_ON BIT(9)


static void on_message(void* context, String& sub_topic, String& message) {

    BedCtrl* bedctrl = (BedCtrl *)context;
    int value = message.toInt();
    if (sub_topic == "bed_pos_1") {

        bedctrl->set_target(BED1, (value >> 16) & 0xFFFF, value & 0xFFFF);
    } else if (sub_topic == "bed_pos_2") {

        bedctrl->set_target(BED2, (value >> 16) & 0xFFFF, value & 0xFFFF);
    } else {

        uint state = value % 10;
        if (sub_topic == "synch") {

            if (state) {

                bedctrl->synch();
            }
        } else {

            byte bed_mask = (value >> 1) & (BIT(NB_BEDS) - 1);
            if (sub_topic == "reset") {

                if (state) {

                    bedctrl->reset(bed_mask);
                }            
            } else if (sub_topic == "light") {

                bedctrl->set_light(bed_mask, state != 0);
            } else {

                for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

                    if (bed_mask & BIT(bed)) {

                        if (sub_topic == "memory1") {

                            bedctrl->memory(bed, 1, state);
                        } else if (sub_topic == "memory2") {

                            bedctrl->memory(bed, 2, state);
                        } else if (sub_topic == "headup") {

                            if (state) {

                                Serial.println("BED" + String(bed) + ": HEAD UP started...");
                                bedctrl->add_cmd(bed, CMD_START_HEAD_UP, CMD_START_HEAD_DOWN);
                            } else {

                                bedctrl->add_cmd(bed, CMD_STOP_HEAD_UP, CMD_START_HEAD_UP);
                            }
                        } else if (sub_topic == "footup") {

                            if (state) {

                                Serial.println("BED" + String(bed) + ": FOOT UP started...");
                                bedctrl->add_cmd(bed, CMD_START_FOOT_UP, CMD_START_FOOT_DOWN);
                            } else {

                                bedctrl->add_cmd(bed, CMD_STOP_FOOT_UP, CMD_START_FOOT_UP);
                            }
                        } else if (sub_topic == "headdown") {

                            if (state) {

                                Serial.println("BED" + String(bed) + ": HEAD DOWN started...");
                                bedctrl->add_cmd(bed, CMD_START_HEAD_DOWN, CMD_START_HEAD_UP);
                            } else {

                                bedctrl->add_cmd(bed, CMD_STOP_HEAD_DOWN, CMD_START_HEAD_DOWN);
                            }
                        } else if (sub_topic == "footdown") {

                            if (state) {

                                Serial.println("BED" + String(bed) + ": FOOT DOWN started...");
                                bedctrl->add_cmd(bed, CMD_START_FOOT_DOWN, CMD_START_FOOT_UP);
                            } else {

                                bedctrl->add_cmd(bed, CMD_STOP_FOOT_DOWN, CMD_START_FOOT_DOWN);
                            }
                        } else {

                            Serial.println("MQTT unsupported sub topic: " + sub_topic);
                        }
                    }
                }
            }
        }
    }
}

BedCtrl::BedCtrl(uint loop_time_ms) {

    this->loop_time_ms = loop_time_ms;
    for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

        // Memory buttonS
        this->prev_state[bed] = 0;
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
    this->mqtt = mqtt_create(on_message, this);
}

void BedCtrl::set_light(byte bed_mask, bool state)  {

    this->bed_mask = bed_mask;
    for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

        if (this->bed_mask & BIT(bed) && state) {

            if (this->light_tgt[bed] < 0) {

                digitalWrite(io_lights[bed][LIGHT_PWR], LOW);
                this->light_tgt[bed] = 5;
            }
            this->command[bed] |= CMD_LIGHT_SWITCH_ON;
            Serial.println("BED" + String(bed) + ": light=" + String(this->light_tgt[bed]));
        } else {

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

void BedCtrl::add_cmd(byte bed, uint append, uint clear) {

    this->bed_mask |= BIT(bed);
    this->command[bed] = (this->command[bed] & ~clear) | append;
}

void BedCtrl::reset(byte bed_mask) {

    this->bed_mask = bed_mask;
    for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

        if (this->bed_mask & BIT(bed)) {

            this->head_pos[bed] += 20;
            this->foot_pos[bed] += 20;
            this->set_target(bed, 0, 0);
        }
    }
}

void BedCtrl::synch(void) {

    int head_tgt[NB_BEDS];
    int foot_tgt[NB_BEDS];
    for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

        head_tgt[bed] = -1;
        foot_tgt[bed] = -1;
    }
    if (this->head_pos[BED1] > this->head_pos[BED2]) {

        head_tgt[BED2] = head_tgt[BED1];
    } else if (this->head_pos[BED1] < this->head_pos[BED2]) {

        head_tgt[BED1] = head_tgt[BED2];
    }
    if (this->foot_pos[BED1] > this->foot_pos[BED2]) {

        foot_tgt[BED2] = foot_tgt[BED1];
    } else if (this->foot_pos[BED1] < this->foot_pos[BED2]) {

        foot_tgt[BED1] = foot_tgt[BED2];
    }
    for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

        this->set_target(bed, head_tgt[bed], foot_tgt[bed]);
    }
}

void BedCtrl::memory(byte bed, uint unit, uint state) {

    String sub_topic;
    String message = "";
    switch (state) {

        case 0:
            switch (this->prev_state[bed]) {

                case 1: // Memory button short pressed
                    sub_topic = "restore" + String(unit) + String(bed);
                    Serial.println("BED" + String(bed) + ": short press M" + String(unit));
                    this->mqtt->publish(message, &sub_topic);
                    break;

                case 2: // Memory button long pressed
                    sub_topic = "store" + String(unit) + String(bed);
                    message += String(this->head_pos[bed]) + ' ' + String(this->foot_pos[bed]);
                    Serial.println("BED" + String(bed) + ": long press M" + String(unit) + " => store: " + message);
                    this->mqtt->publish(message, &sub_topic);
                    break;
            }
            this->prev_state[bed] = 0;
            break;

        case 1:
            this->prev_state[bed] = 1;
            break;

        case 2:
            if (this->prev_state[bed] == 1) {

                this->prev_state[bed] = 2;
            }
            break;
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

    this->mqtt->loop();
    for (uint bed = 0 ; bed < NB_BEDS ; bed++) {

        this->do_command(bed);
    }
}
