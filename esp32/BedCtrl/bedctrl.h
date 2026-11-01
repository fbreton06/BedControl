#ifndef __RADIAT__
#define __RADIAT__

#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include "globals.h"

// Do not use GPIO 1&3 (used bu serial monitor: trace debug)
#define IO_PIN_BED1_LIGHT_CMD       25
#define IO_PIN_BED1_LIGHT_PWR       32
#define IO_PIN_BED1_HEAD_UP         18
#define IO_PIN_BED1_HEAD_DOWN       19
#define IO_PIN_BED1_FOOT_UP         23
#define IO_PIN_BED1_FOOT_DOWN        5

#define IO_PIN_BED2_LIGHT_CMD       0
#define IO_PIN_BED2_LIGHT_PWR       4
#define IO_PIN_BED2_HEAD_UP         22
#define IO_PIN_BED2_HEAD_DOWN       21
#define IO_PIN_BED2_FOOT_UP         17
#define IO_PIN_BED2_FOOT_DOWN       16

#define HEAD_UP     0
#define HEAD_DOWN   1
#define FOOT_UP     2
#define FOOT_DOWN   3
#define NB_IO_PINS  4

#define BED1        0 // Right
#define BED2        1 // Left
// NB_BEDS is defined in globals.h (shared with DeviceConfig)

#define LIGHT_PWR    0
#define LIGHT_CMD    1
#define NB_IO_LIGHTS 2

#define BED1_MASK   BIT(BED1)
#define BED2_MASK   BIT(BED2)


const uint io_pins[NB_BEDS][NB_IO_PINS] = {

    { IO_PIN_BED1_HEAD_UP, IO_PIN_BED1_HEAD_DOWN, IO_PIN_BED1_FOOT_UP, IO_PIN_BED1_FOOT_DOWN },
    { IO_PIN_BED2_HEAD_UP, IO_PIN_BED2_HEAD_DOWN, IO_PIN_BED2_FOOT_UP, IO_PIN_BED2_FOOT_DOWN }
};

const uint io_lights[NB_BEDS][NB_IO_LIGHTS] = {

    { IO_PIN_BED1_LIGHT_PWR, IO_PIN_BED1_LIGHT_CMD },
    { IO_PIN_BED2_LIGHT_PWR, IO_PIN_BED2_LIGHT_CMD },    
};


class BedCtrl {

public:
    BedCtrl(uint loop_time_ms);
    virtual ~BedCtrl(void) {};

private:
    Adafruit_NeoPixel* leds[NB_BEDS];
    uint loop_time_ms;
    uint command[NB_BEDS];      // Bit array of current commands ongoing: default is CMD_NONE
    uint head_pos[NB_BEDS];     // Current head position: counter of "loop_time_ms"
    uint foot_pos[NB_BEDS];     // Current head position: counter of "loop_time_ms"
    int head_tgt[NB_BEDS];      // -1: means target reached
    int foot_tgt[NB_BEDS];      // -1: means target reached
    int light_tgt[NB_BEDS];     // -1: means target reached

private:
    void do_command(uint bed);

public:
    void set_light(byte bed_mask); // toggle light on/off per bed
    void add_cmd(byte byte, uint append, uint clear);
    void reset(byte bed_mask);
    void synch(void);
    bool stop_synch(byte bed_mask); // abort an in-progress reset/synch; true if anything stopped
    void stop_all(void);            // stop every motor (BLE client lost)
    void memory(byte bed, uint unit, bool is_long);
    void set_target(byte bed, int head, int foot);
    void loop(void);
};
#endif // __RADIAT__
