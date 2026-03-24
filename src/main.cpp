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
enum MarkerType { INDEPENDENT, COLLECTIVE };

#include "patterns.h"

struct PhaseConfig {
    bool clockwise;
    MarkerType type;
    const MarkerPattern* pattern;  // nullptr = default sequential
};

// Game configuration
const unsigned long MOVE_INTERVAL_MS = 80;
const unsigned long MISS_HOLD_MS = 1500;
const unsigned long HIT_FLASH_ON_MS = 200;
const unsigned long HIT_FLASH_OFF_MS = 150;
const int HIT_FLASH_COUNT = 3;
const unsigned long HIT_EFFECT_TOTAL_MS =
    (unsigned long)HIT_FLASH_COUNT * (HIT_FLASH_ON_MS + HIT_FLASH_OFF_MS);

// Patterns defined in patterns.h

const PhaseConfig phases[NUM_PHASES] = {
    {true, INDEPENDENT, nullptr},
    {false, COLLECTIVE, &PATTERN_WAVES_LOOP},
    {true, INDEPENDENT, nullptr},
    {false, COLLECTIVE, &PATTERN_INFINITY},
};

const int STATE_NORMAL = 0;
const int STATE_HIT = 1;
const int STATE_MISS = 2;
const int STATE_DONE = 3;

// Game state
bool gameActive = false;

volatile bool playerButtonPressed[NUM_PLAYERS] = {};
int playerCurrentState[NUM_PLAYERS] = {};
bool playerPhaseCompleted[NUM_PLAYERS] = {};

int playerMarkerPos[NUM_PLAYERS] = {};
int playerMissedPos[NUM_PLAYERS] = {};
int collectiveMarkerPos = 0;
int collectivePatternIndex = 0;

unsigned long playerEffectStartTime[NUM_PLAYERS] = {};

unsigned long lastMarkerMoveTime = 0;

void setupESPNow();
void setupLEDS();
void setupButtons();
void initGame();
void runGameLoop();
void renderGameFrame();

void advanceMarkers();

void logPhase(int phase);
void startPhase(int phase);
void advancePhase();

bool isIndependentMarker();
bool isCollectiveMarker();
bool isHit(int player);
bool isPhaseComplete();
bool isCalibrated();

int getMarkerPos(int player);
int getTargetPos(int player);
int getNextMarkerPos(int player);
int getNextMarkerPos();

void advanceMarkers();
void playCalibratedEffects();

void handleButtonPressed(void* button_handle, void* usr_data);

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
  Serial.printf("Button pressed for player %d\n", (int)(intptr_t)usr_data);
  int player = (int)(intptr_t)usr_data;
  playerButtonPressed[player] = true;
}

void initGame() {
  currentPhase = 0;
  memset(phaseCompleted, false, sizeof(phaseCompleted));
  startPhase(0);
}

void runGameLoop() {
  if (!gameActive)
    return;

  // Iterate over players to transition into hit/miss states based on button presses
  for (int p = 0; p < NUM_PLAYERS; p++) {
    if (playerButtonPressed[p]) {
      playerButtonPressed[p] = false;
      if (playerCurrentState[p] == STATE_NORMAL) {
        if (isHit(p)) {
          playerPhaseCompleted[p] = true;
          playerCurrentState[p] = STATE_HIT;
          playerEffectStartTime[p] = millis();
        } else {
          playerMissedPos[p] = (isIndependentMarker()) ? p * NUM_RING_LEDS + playerMarkerPos[p]
                                                       : collectiveMarkerPos;
          playerCurrentState[p] = STATE_MISS;
          playerEffectStartTime[p] = millis();
        }
      }
    }
  }

  // Iterate over players to transition out of hit/miss states based on timers
  for (int p = 0; p < NUM_PLAYERS; p++) {
    unsigned long elapsed = millis() - playerEffectStartTime[p];
    if (playerCurrentState[p] == STATE_HIT && elapsed >= HIT_EFFECT_TOTAL_MS) {
      playerCurrentState[p] = STATE_DONE;
    } else if (playerCurrentState[p] == STATE_MISS && elapsed >= MISS_HOLD_MS) {
      playerCurrentState[p] = STATE_NORMAL;
    }
  }

  // Advance phase once all players have hit and hit effects have finished
  if (isPhaseComplete()) {
    bool allEffectsDone = true;
    for (int p = 0; p < NUM_PLAYERS; p++) {
      if (playerCurrentState[p] == STATE_HIT) {
        allEffectsDone = false;
        break;
      }
    }
    if (allEffectsDone) {
      advancePhase();
      return;
    }
  }

  // Advance Marker(s) on timer
  unsigned long now = millis();
  if (now - lastMarkerMoveTime >= MOVE_INTERVAL_MS) {
    lastMarkerMoveTime = now;
    advanceMarkers();
  }

  renderGameFrame();
}

void renderGameFrame() {
  FastLED.clear();

  // Iterate over players to render target, marker, and hit/miss effects
  for (int p = 0; p < NUM_PLAYERS; p++) {
    int offset = p * NUM_RING_LEDS;
    int targetPos = offset + LED_6_OCLOCK;
    unsigned long elapsed = millis() - playerEffectStartTime[p];

    switch (playerCurrentState[p]) {
      case STATE_NORMAL:
        leds[targetPos] = LED_TARGET_COLOR;
        if (isIndependentMarker()) {
          leds[offset + playerMarkerPos[p]] = LED_MARKER_COLOR;
        }
        break;
      case STATE_HIT: {
        unsigned long cycle = HIT_FLASH_ON_MS + HIT_FLASH_OFF_MS;
        leds[targetPos] = (elapsed % cycle < HIT_FLASH_ON_MS) ? LED_HIT_COLOR : CRGB::Black;
        break;
      }
      case STATE_MISS:
        leds[targetPos] = LED_TARGET_COLOR;
        leds[playerMissedPos[p]] = LED_MISS_COLOR;
        break;
      case STATE_DONE:
        leds[targetPos] = LED_HIT_COLOR;
        break;
    }
  }

  // Collective moving LED overlaid on top
  if (isCollectiveMarker()) {
    leds[collectiveMarkerPos] = LED_MARKER_COLOR;
  }

  FastLED.show();
}

void advanceMarkers() {
  if (isIndependentMarker()) {
    for (int p = 0; p < NUM_PLAYERS; p++) {
      if (playerCurrentState[p] == STATE_NORMAL) {
        playerMarkerPos[p] = getNextMarkerPos(p);
      }
    }
  } else {
    const MarkerPattern* pattern = phases[currentPhase].pattern;
    if (pattern) {
      collectivePatternIndex =
          phases[currentPhase].clockwise
              ? (collectivePatternIndex + 1) % pattern->length
              : (collectivePatternIndex - 1 + pattern->length) % pattern->length;
      collectiveMarkerPos = pgm_read_byte(&pattern->leds[collectivePatternIndex]);
    } else {
      collectiveMarkerPos = getNextMarkerPos();
    }
  }
}

void logPhase(int phase) {
  Serial.printf("→ Phase %d: %s, %s\n", phase,
                phases[phase].clockwise ? "clockwise" : "counter-clockwise",
                phases[phase].type == INDEPENDENT ? "independent" : "collective");
}

void startPhase(int phase) {
  logPhase(phase);

  for (int p = 0; p < NUM_PLAYERS; p++) {
    playerMarkerPos[p] = LED_12_OCLOCK;
    playerPhaseCompleted[p] = false;
    playerCurrentState[p] = STATE_NORMAL;
    playerEffectStartTime[p] = 0;
    playerButtonPressed[p] = false;
  }
  collectivePatternIndex = 0;
  const MarkerPattern* pat = phases[phase].pattern;
  collectiveMarkerPos = pat ? pgm_read_byte(&pat->leds[0]) : LED_12_OCLOCK;
  lastMarkerMoveTime = millis();
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

bool isIndependentMarker() {
  return phases[currentPhase].type == INDEPENDENT;
}

bool isCollectiveMarker() {
  return phases[currentPhase].type == COLLECTIVE;
}

int getMarkerPos(int player) {
  return isIndependentMarker() ? playerMarkerPos[player] : collectiveMarkerPos;
}

int getTargetPos(int player) {
  return isIndependentMarker() ? LED_6_OCLOCK : player * NUM_RING_LEDS + LED_6_OCLOCK;
}

int getNextMarkerPos(int p) {
  return phases[currentPhase].clockwise ? (playerMarkerPos[p] + 1) % NUM_RING_LEDS
                                        : (playerMarkerPos[p] - 1 + NUM_RING_LEDS) % NUM_RING_LEDS;
}

int getNextMarkerPos() {
  return phases[currentPhase].clockwise ? (collectiveMarkerPos + 1) % NUM_LEDS
                                        : (collectiveMarkerPos - 1 + NUM_LEDS) % NUM_LEDS;
}

bool isHit(int player) {
  return getMarkerPos(player) == getTargetPos(player);
}

// Returns true if all players have completed their current phase
bool isPhaseComplete() {
  for (int p = 0; p < NUM_PLAYERS; p++) {
    if (!playerPhaseCompleted[p])
      return false;
  }
  return true;
}

// Returns true if all phases are completed
bool isCalibrated() {
  for (int i = 0; i < NUM_PHASES; i++) {
    if (!phaseCompleted[i]) {
      return false;
    }
  }
  return true;
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