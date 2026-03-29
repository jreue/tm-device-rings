#pragma once

#define DEBUG

#define NUM_PLAYERS 8
#define NUM_PHASES 8

#define NUM_RING_LEDS 24
#define NUM_LEDS (NUM_PLAYERS * NUM_RING_LEDS)

// ====================
// This Devices Configuration
// ====================
#define DEVICE_ID 104

// ====================
// Button Configuration
// ====================
#define BUTTON_1_PIN GPIO_NUM_18
#define BUTTON_2_PIN GPIO_NUM_5
#define BUTTON_3_PIN GPIO_NUM_17
#define BUTTON_4_PIN GPIO_NUM_32
#define BUTTON_5_PIN GPIO_NUM_33
#define BUTTON_6_PIN GPIO_NUM_25
#define BUTTON_7_PIN GPIO_NUM_26
#define BUTTON_8_PIN GPIO_NUM_27

// ====================
// LED Configuration
// ====================
#define LED_RING_PIN GPIO_NUM_14
#define LED_PHASE_PIN GPIO_NUM_13

#define NUM_PHASE_ROWS 4
#define NUM_PHASE_COLS 12
#define NUM_PHASE_LEDS (NUM_PHASE_ROWS * NUM_PHASE_COLS)

// Ring positions (LED index 0 = 12 o'clock, increases clockwise)
#define LED_12_OCLOCK 0
#define LED_3_OCLOCK (NUM_RING_LEDS / 4)
#define LED_6_OCLOCK (NUM_RING_LEDS / 2)
#define LED_9_OCLOCK (NUM_RING_LEDS * 3 / 4)

// LED colors
#define LED_TARGET_COLOR CRGB::Purple
#define LED_MARKER_COLOR CRGB::Blue
#define LED_HIT_COLOR CRGB::Green
#define LED_MISS_COLOR CRGB::Red