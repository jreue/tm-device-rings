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

// Phase types
enum PhaseType { INDEPENDENT, SEQUENTIAL };

struct PhaseConfig {
    bool clockwise;
    PhaseType type;
};

// Game configuration
const unsigned long MOVE_INTERVAL_MS = 80;
const unsigned long MISS_HOLD_MS = 1500;
const unsigned long HIT_FLASH_ON_MS = 200;
const unsigned long HIT_FLASH_OFF_MS = 150;
const int HIT_FLASH_COUNT = 3;
const unsigned long HIT_EFFECT_TOTAL_MS =
    (unsigned long)HIT_FLASH_COUNT * (HIT_FLASH_ON_MS + HIT_FLASH_OFF_MS);
const PhaseConfig phases[NUM_PHASES] = {
    {true, INDEPENDENT},
    {false, SEQUENTIAL},
    {true, INDEPENDENT},
    {false, SEQUENTIAL},
};

enum PlayerEffectState { EFFECT_NONE, EFFECT_HIT, EFFECT_MISS, EFFECT_DONE };

// Game state
volatile bool buttonPressed[NUM_PLAYERS] = {};
int playerMovingLed[NUM_PLAYERS] = {};
int sequentialLedPos = 0;
bool playerPhaseComplete[NUM_PLAYERS] = {};
PlayerEffectState playerEffect[NUM_PLAYERS] = {};
unsigned long playerEffectStart[NUM_PLAYERS] = {};
int playerMissedLed[NUM_PLAYERS] = {};
unsigned long lastMoveTime = 0;
bool gameActive = false;

bool isCalibrated();
void setupESPNow();
void setupLEDS();
void setupButtons();
void initGame();
void runGameLoop();
void renderGameFrame();
void startPhase(int phase);
void advancePhase();
bool isHit(int player);
bool isPhaseComplete();
void advanceLEDs();
void playCalibratedEffects();

void handleButtonPressed(void* button_handle, void* usr_data);

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
  const gpio_num_t buttonPins[NUM_PLAYERS] = {BUTTON_1_PIN, BUTTON_2_PIN};
  for (int i = 0; i < NUM_PLAYERS; i++) {
    Button* btn = new Button(buttonPins[i], false);
    btn->attachPressDownEventCb(&handleButtonPressed, (void*)(intptr_t)i);
  }
}

void handleButtonPressed(void* button_handle, void* usr_data) {
  int player = (int)(intptr_t)usr_data;
  buttonPressed[player] = true;
}

void initGame() {
  currentPhase = 0;
  memset(phaseCompleted, false, sizeof(phaseCompleted));
  startPhase(0);
}

void runGameLoop() {
  if (!gameActive)
    return;

  // Handle button presses
  for (int p = 0; p < NUM_PLAYERS; p++) {
    if (buttonPressed[p]) {
      buttonPressed[p] = false;
      if (playerEffect[p] == EFFECT_NONE) {
        if (isHit(p)) {
          playerPhaseComplete[p] = true;
          playerEffect[p] = EFFECT_HIT;
          playerEffectStart[p] = millis();
        } else {
          playerMissedLed[p] = (phases[currentPhase].type == INDEPENDENT)
                                   ? p * NUM_RING_LEDS + playerMovingLed[p]
                                   : sequentialLedPos;
          playerEffect[p] = EFFECT_MISS;
          playerEffectStart[p] = millis();
        }
      }
    }
  }

  // Advance effect state transitions
  for (int p = 0; p < NUM_PLAYERS; p++) {
    unsigned long elapsed = millis() - playerEffectStart[p];
    if (playerEffect[p] == EFFECT_HIT && elapsed >= HIT_EFFECT_TOTAL_MS) {
      playerEffect[p] = EFFECT_DONE;
    } else if (playerEffect[p] == EFFECT_MISS && elapsed >= MISS_HOLD_MS) {
      playerEffect[p] = EFFECT_NONE;
    }
  }

  // Advance phase once all players have hit and hit effects have finished
  if (isPhaseComplete()) {
    bool allEffectsDone = true;
    for (int p = 0; p < NUM_PLAYERS; p++) {
      if (playerEffect[p] == EFFECT_HIT) {
        allEffectsDone = false;
        break;
      }
    }
    if (allEffectsDone) {
      advancePhase();
      return;
    }
  }

  // Advance LEDs on timer
  unsigned long now = millis();
  if (now - lastMoveTime >= MOVE_INTERVAL_MS) {
    lastMoveTime = now;
    advanceLEDs();
  }

  renderGameFrame();
}

void renderGameFrame() {
  FastLED.clear();
  for (int p = 0; p < NUM_PLAYERS; p++) {
    int offset = p * NUM_RING_LEDS;
    int targetPos = offset + LED_6_OCLOCK;
    unsigned long elapsed = millis() - playerEffectStart[p];
    switch (playerEffect[p]) {
      case EFFECT_NONE:
        leds[targetPos] = LED_TARGET_COLOR;
        leds[offset + playerMovingLed[p]] = LED_CURRENT_COLOR;
        break;
      case EFFECT_HIT: {
        unsigned long cycle = HIT_FLASH_ON_MS + HIT_FLASH_OFF_MS;
        leds[targetPos] = (elapsed % cycle < HIT_FLASH_ON_MS) ? LED_HIT_COLOR : CRGB::Black;
        break;
      }
      case EFFECT_MISS:
        leds[targetPos] = LED_TARGET_COLOR;
        leds[playerMissedLed[p]] = LED_MISS_COLOR;
        break;
      case EFFECT_DONE:
        leds[targetPos] = LED_HIT_COLOR;
        break;
    }
  }
  // Sequential moving LED overlaid on top
  if (phases[currentPhase].type == SEQUENTIAL) {
    leds[sequentialLedPos] = LED_CURRENT_COLOR;
  }
  FastLED.show();
}

void startPhase(int phase) {
  Serial.printf("Starting phase %d (%s, %s)\n", phase,
                phases[phase].clockwise ? "clockwise" : "counter-clockwise",
                phases[phase].type == INDEPENDENT ? "independent" : "sequential");
  for (int p = 0; p < NUM_PLAYERS; p++) {
    playerMovingLed[p] = LED_12_OCLOCK;
    playerPhaseComplete[p] = false;
    playerEffect[p] = EFFECT_NONE;
    playerEffectStart[p] = 0;
    buttonPressed[p] = false;
  }
  sequentialLedPos = LED_12_OCLOCK;
  lastMoveTime = millis();
  gameActive = true;
  renderGameFrame();
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

bool isHit(int player) {
  if (phases[currentPhase].type == INDEPENDENT) {
    return playerMovingLed[player] == LED_6_OCLOCK;
  } else {
    return sequentialLedPos == player * NUM_RING_LEDS + LED_6_OCLOCK;
  }
}

bool isPhaseComplete() {
  for (int p = 0; p < NUM_PLAYERS; p++) {
    if (!playerPhaseComplete[p])
      return false;
  }
  return true;
}

void advanceLEDs() {
  if (phases[currentPhase].type == INDEPENDENT) {
    for (int p = 0; p < NUM_PLAYERS; p++) {
      if (playerEffect[p] == EFFECT_NONE) {
        playerMovingLed[p] = phases[currentPhase].clockwise
                                 ? (playerMovingLed[p] + 1) % NUM_RING_LEDS
                                 : (playerMovingLed[p] - 1 + NUM_RING_LEDS) % NUM_RING_LEDS;
      }
    }
  } else {
    sequentialLedPos = phases[currentPhase].clockwise
                           ? (sequentialLedPos + 1) % NUM_LEDS
                           : (sequentialLedPos - 1 + NUM_LEDS) % NUM_LEDS;
  }
}

void playCalibratedEffects() {
  gameActive = false;
  Serial.println("Game complete!");
  unsigned long start = millis();
  while (millis() - start < 3000) {
    for (int i = 0; i < 5; i++) {
      leds[random(NUM_LEDS)] = CHSV(random(256), 255, 255);
    }
    FastLED.show();
    delay(50);
    fadeToBlackBy(leds, NUM_LEDS, 80);
  }
  FastLED.clear(true);
}