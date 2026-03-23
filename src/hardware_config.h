#pragma once

#define NUM_PLAYERS 1
#define NUM_PHASES 4

#define NUM_RING_LEDS 24
#define NUM_LEDS (NUM_PLAYERS * NUM_RING_LEDS)

// ====================
// This Devices Configuration
// ====================
#define DEVICE_ID 104

// ====================
// Button Configuration
// ====================
#define BUTTON_1_PIN GPIO_NUM_5

// ====================
// LED Configuration
// ====================
#define LED_RING_PIN GPIO_NUM_14

// Ring positions (LED index 0 = 12 o'clock, increases clockwise)
#define LED_12_OCLOCK 0
#define LED_3_OCLOCK (NUM_RING_LEDS / 4)
#define LED_6_OCLOCK (NUM_RING_LEDS / 2)
#define LED_9_OCLOCK (NUM_RING_LEDS * 3 / 4)

// LED colors
#define LED_TARGET_COLOR CRGB::Yellow
#define LED_CURRENT_COLOR CRGB::Blue
#define LED_HIT_COLOR CRGB::Green
#define LED_MISS_COLOR CRGB::Red