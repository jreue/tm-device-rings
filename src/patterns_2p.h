#pragma once
#include <Arduino.h>

#include "hardware_config.h"

// Patterns for a 2-ring setup (48 LEDs total).
// Ring 1 = LEDs 0-23
// Ring 2 = LEDs 24-47

// Clock positions per ring
// 12 o'clock = 0
// 3 o'clock = 6
// 6 o'clock = 12
// 9 o'clock = 18

struct MarkerPattern {
    const uint8_t* leds;
    uint16_t length;
};

const uint8_t PATTERN_WEAVE_LEDS[] PROGMEM = {
    // R1-3 -> R1-3 CW
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
    18,
    // R2-3 -> R2-3 CW
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
    30,
    // R1-9 -> R1-3 CCW
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
};

const uint8_t PATTERN_WAVES_LOOP_LEDS[] PROGMEM = {
    // R1-3 -> R1-9 CW
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
    18,
    // R2-3 -> R2-3 CW
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    // R1-9 -> R1-3 CW
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
};

const MarkerPattern PATTERN_WEAVE = {PATTERN_WEAVE_LEDS, sizeof(PATTERN_WEAVE_LEDS)};
const MarkerPattern PATTERN_WAVES_LOOP = {PATTERN_WAVES_LOOP_LEDS, sizeof(PATTERN_WAVES_LOOP_LEDS)};