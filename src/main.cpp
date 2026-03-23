#include <Arduino.h>
#include <FastLED.h>
#include <shared_hardware_config.h>

#include "Button.h"
#include "EspNowHelper.h"
#include "hardware_config.h"

uint8_t hubAddress[] = HUB_MAC_ADDRESS;

EspNowHelper espNowHelper;

CRGB leds[NUM_LEDS];

int currentPhase = 0;
bool phaseCompleted[NUM_PHASES] = {};

bool isCalibrated();
void setupESPNow();
void setupLEDS();
void setupButtons();
void testRings();

void handlePlayer1ButtonPressed(void* button_handle, void* usr_data);

// Returns true if all phases are completed
bool isCalibrated() {
  for (int i = 0; i < NUM_PHASES; i++) {
    if (!phaseCompleted[i]) {
      return false;
    }
  }
  return true;
}

void setup() {
  Serial.begin(115200);

  setupESPNow();
  setupLEDS();
  setupButtons();
  testRings();
}

void loop() {
  // put your main code here, to run repeatedly:
}

void setupESPNow() {
  espNowHelper.begin(DEVICE_ID);
  espNowHelper.addPeer(hubAddress);
  espNowHelper.sendModuleConnected(hubAddress);
}

void setupLEDS() {
  FastLED.addLeds<WS2812, LED_RING_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(25);
  FastLED.clear(true);
  FastLED.show();
}

void setupButtons() {
  Button* player1Button = new Button(BUTTON_1_PIN, false);
  player1Button->attachPressDownEventCb(&handlePlayer1ButtonPressed, NULL);
}

void handlePlayer1ButtonPressed(void* button_handle, void* usr_data) {
  Serial.println("Player 1 Button Pressed");
}

void testRings() {
  for (int i = 0; i < NUM_RING_LEDS; i++) {
    leds[i] = CRGB::Purple;
    FastLED.show();
    delay(40);
    leds[i] = CRGB::Black;
  }
  FastLED.show();
}