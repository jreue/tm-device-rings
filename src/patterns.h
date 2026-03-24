#pragma once
#include <Arduino.h>

#include "hardware_config.h"

// Patterns are ordered arrays of absolute LED indices stored in flash.
// Ring 1 = LEDs 0-23, Ring 2 = LEDs 24-47.
// Clock positions per ring: 12=0, 3=6, 6=12, 9=18 (with NUM_RING_LEDS=24)

struct MarkerPattern {
    const uint8_t* leds;
    uint16_t length;
};

// Figure-8 / infinity: Ring1 is the right lobe (CW), Ring2 is the left lobe (CCW).
const uint8_t PATTERN_INFINITY_LEDS[] PROGMEM = {
    // Ring 1: CW from 9 o'clock through 12, 3, 6, back to 9 (right lobe)
    18,
    19,
    20,
    21,
    22,
    23,
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    // Ring 2: CCW from 3 o'clock through 12, 9, 6, back to 3 (left lobe)
    30,
    29,
    28,
    27,
    26,
    25,
    24,
    47,
    46,
    45,
    44,
    43,
    42,
    41,
    40,
    39,
    38,
    37,
    36,
    35,
    34,
    33,
    32,
    31,
};
const MarkerPattern PATTERN_INFINITY = {PATTERN_INFINITY_LEDS, sizeof(PATTERN_INFINITY_LEDS)};