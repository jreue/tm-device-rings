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