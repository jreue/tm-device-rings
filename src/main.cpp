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

// Game configuration
const unsigned long MOVE_INTERVAL_MS = 80;
const bool phaseClockwise[NUM_PHASES] = {true, false, true, false};

// Game state
volatile bool buttonPressedFlag = false;
int movingLedPos = 0;
unsigned long lastMoveTime = 0;
bool gameActive = false;
bool showingMiss = false;
unsigned long missTime = 0;
const unsigned long MISS_HOLD_MS = 1500;

bool isCalibrated();
void setupESPNow();
void setupLEDS();
void setupButtons();
void initGame();
void startPhase(int phase);
void advancePhase();
void runGameLoop();
void renderGame();
bool isHit();
void showHit();
void playCalibratedEffects();

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
  initGame();
}

void loop() {
  runGameLoop();
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
  buttonPressedFlag = true;
}

void initGame() {
  currentPhase = 0;
  memset(phaseCompleted, false, sizeof(phaseCompleted));
  startPhase(0);
}

void startPhase(int phase) {
  Serial.printf("Starting phase %d (%s)\n", phase,
                phaseClockwise[phase] ? "clockwise" : "counter-clockwise");
  movingLedPos = LED_12_OCLOCK;
  buttonPressedFlag = false;
  showingMiss = false;
  lastMoveTime = millis();
  gameActive = true;
  renderGame();
}

void renderGame() {
  FastLED.clear();
  leds[LED_6_OCLOCK] = CRGB::Yellow;
  leds[movingLedPos] = CRGB::Blue;
  FastLED.show();
}

void showHit() {
  for (int flash = 0; flash < 3; flash++) {
    FastLED.clear();
    leds[LED_6_OCLOCK] = CRGB::Green;
    FastLED.show();
    delay(200);
    FastLED.clear(true);
    delay(150);
  }
}

bool isHit() {
  return movingLedPos == LED_6_OCLOCK;
}

void advancePhase() {
  phaseCompleted[currentPhase] = true;
  currentPhase++;
  if (isCalibrated()) {
    playCalibratedEffects();
  } else {
    startPhase(currentPhase);
  }
}

void runGameLoop() {
  if (!gameActive)
    return;

  if (showingMiss) {
    if (millis() - missTime >= MISS_HOLD_MS) {
      showingMiss = false;
      lastMoveTime = millis();
      buttonPressedFlag = false;
      renderGame();
    }
    return;
  }

  if (buttonPressedFlag) {
    buttonPressedFlag = false;
    if (isHit()) {
      showHit();
      advancePhase();
      return;
    } else {
      showingMiss = true;
      missTime = millis();
      FastLED.clear();
      leds[LED_6_OCLOCK] = CRGB::Yellow;
      leds[movingLedPos] = CRGB::Red;
      FastLED.show();
      return;
    }
  }

  unsigned long now = millis();
  if (now - lastMoveTime >= MOVE_INTERVAL_MS) {
    lastMoveTime = now;
    if (phaseClockwise[currentPhase]) {
      movingLedPos = (movingLedPos + 1) % NUM_RING_LEDS;
    } else {
      movingLedPos = (movingLedPos - 1 + NUM_RING_LEDS) % NUM_RING_LEDS;
    }
    renderGame();
  }
}

void playCalibratedEffects() {
  gameActive = false;
  Serial.println("Game complete!");
  for (int flash = 0; flash < 3; flash++) {
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.show();
    delay(300);
    FastLED.clear(true);
    delay(200);
  }
}