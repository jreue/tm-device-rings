#include <Arduino.h>
#include <FastLED.h>
#include <shared_hardware_config.h>

#include "Button.h"
#include "EspNowHelper.h"
#include "hardware_config.h"

uint8_t hubAddress[] = HUB_MAC_ADDRESS;

EspNowHelper espNowHelper;

CRGB leds[NUM_LEDS];
CRGB phaseLeds[NUM_PHASE_LEDS];

int currentPhase = 0;
bool phaseCompleted[NUM_PHASES] = {};

// Phase types
enum MarkerType { SOLO, COLLECTIVE, SIMON };
enum SimonState { SIMON_SHOW, SIMON_PAUSE, SIMON_INPUT, SIMON_MISTAKE };

#if NUM_PLAYERS == 2
#include "patterns_2p.h"
#elif NUM_PLAYERS == 3
#include "patterns_3p.h"
#elif NUM_PLAYERS == 4
#include "patterns_4p.h"
#elif NUM_PLAYERS == 5
#include "patterns_5p.h"
#elif NUM_PLAYERS == 6
#include "patterns_6p.h"
#elif NUM_PLAYERS == 7
#include "patterns_7p.h"
#elif NUM_PLAYERS == 8
#include "patterns_8p.h"
#endif

struct PhaseConfig {
    MarkerType type;
    bool clockwise;

    const MarkerPattern* pattern;  // nullptr = default sequential
    unsigned long speed;           // marker move interval in ms
    bool moveTargetOnMiss;         // if true, target moves to random pos on miss (SOLO only)
};

// Game configuration
const unsigned long MISS_HOLD_MS = 1500;
const unsigned long SIMON_PAUSE_MS = 300;
const unsigned long SIMON_MISTAKE_MS = 1500;
const unsigned long SIMON_HIT_MS = 400;
const unsigned long HIT_FLASH_ON_MS = 200;
const unsigned long HIT_FLASH_OFF_MS = 150;
const int HIT_FLASH_COUNT = 3;
const unsigned long HIT_EFFECT_TOTAL_MS =
    (unsigned long)HIT_FLASH_COUNT * (HIT_FLASH_ON_MS + HIT_FLASH_OFF_MS);

// Patterns defined in patterns.h

// Per-player ring colors — matches physical button colors where possible.
// Black buttons have no LED equivalent; Purple is used instead.
const CRGB PLAYER_COLORS[8] = {
    CRGB::Purple,  // P1
    CRGB::Blue,    // P2
    CRGB::Red,     // P3
    CRGB::Yellow,  // P4
    CRGB::Purple,  // P5
    CRGB::Blue,    // P6
    CRGB::Green,   // P7
    CRGB::White,   // P8
};

const PhaseConfig phases[NUM_PHASES] = {
    {SOLO, false, nullptr, 30, true},

    //{COLLECTIVE, true, &PATTERN_WEAVE, 80, true},
    // {SOLO, true, nullptr, 70,true},
    // {COLLECTIVE, true, &PATTERN_WAVES_LOOP, 70,false},
    // {SOLO, false, nullptr, 60,true},
    // {COLLECTIVE, true, &PATTERN_EVEN_ODD, 60,false},
    // {SOLO, true, nullptr, 50,true},
    // {COLLECTIVE, true, &PATTERN_HALVES, 50,false},
    //{SIMON, false, &PATTERN_SIMON_1, 500, false},
    //{SIMON, false, &PATTERN_SIMON_2, 400, false},
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
int playerTargetPos[NUM_PLAYERS] = {};
bool playerTargetMovePending[NUM_PLAYERS] = {};
int collectiveMarkerPos = 0;
int collectivePatternIndex = 0;

unsigned long playerEffectStartTime[NUM_PLAYERS] = {};

unsigned long lastMarkerMoveTime = 0;

SimonState simonState = SIMON_SHOW;
int simonPlayStep = 0;
int simonInputStep = 0;
unsigned long simonStepTime = 0;
int simonLastPressedRing = -1;
unsigned long simonPressTime = 0;

int phasePlayerCompletedCount[NUM_PHASE_COLS] = {};

void setupESPNow();
void setupLEDS();
void setupButtons();
void initGame();
void runGameLoop();
void renderGameFrame();

void advanceMarkers();
void runSimonPhase();

void logPhase(int phase);
void startPhase(int phase);
void advancePhase();

bool isSoloMarker();
bool isCollectiveMarker();
bool isSimonMarker();
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
void handleDebugButtonPress();
void updatePhaseStatusLEDs();

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
  FastLED.addLeds<WS2812, LED_PHASE_PIN, GRB>(phaseLeds, NUM_PHASE_LEDS);
  FastLED.setBrightness(20);
  FastLED.clear(true);
  FastLED.show();
}

void setupButtons() {
#if NUM_PLAYERS == 2
  Serial.println("Setting up for 2 players");
  const gpio_num_t buttonPins[NUM_PLAYERS] = {BUTTON_1_PIN, BUTTON_2_PIN};
#elif NUM_PLAYERS == 3
  Serial.println("Setting up for 3 players");
  const gpio_num_t buttonPins[NUM_PLAYERS] = {BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN};
#elif NUM_PLAYERS == 4
  Serial.println("Setting up for 4 players");
  const gpio_num_t buttonPins[NUM_PLAYERS] = {BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN,
                                              BUTTON_4_PIN};
#elif NUM_PLAYERS == 5
  Serial.println("Setting up for 5 players");
  const gpio_num_t buttonPins[NUM_PLAYERS] = {BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN,
                                              BUTTON_4_PIN, BUTTON_5_PIN};
#elif NUM_PLAYERS == 6
  Serial.println("Setting up for 6 players");
  const gpio_num_t buttonPins[NUM_PLAYERS] = {BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN,
                                              BUTTON_4_PIN, BUTTON_5_PIN, BUTTON_6_PIN};
#elif NUM_PLAYERS == 7
  Serial.println("Setting up for 7 players");
  const gpio_num_t buttonPins[NUM_PLAYERS] = {BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN,
                                              BUTTON_4_PIN, BUTTON_5_PIN, BUTTON_6_PIN,
                                              BUTTON_7_PIN};
#elif NUM_PLAYERS == 8
  Serial.println("Setting up for 8 players");
  const gpio_num_t buttonPins[NUM_PLAYERS] = {BUTTON_1_PIN, BUTTON_2_PIN, BUTTON_3_PIN,
                                              BUTTON_4_PIN, BUTTON_5_PIN, BUTTON_6_PIN,
                                              BUTTON_7_PIN, BUTTON_8_PIN};
#endif

  for (int i = 0; i < NUM_PLAYERS; i++) {
    Button* btn = new Button(buttonPins[i], false);
    btn->attachPressDownEventCb(&handleButtonPressed, (void*)(intptr_t)i);
  }
}

void handleButtonPressed(void* button_handle, void* usr_data) {
  Serial.printf("Button pressed for player %d\n", (int)(intptr_t)usr_data);
  int player = (int)(intptr_t)usr_data;
#ifdef DEBUG
  if (player == 0) {
    handleDebugButtonPress();
    return;
  }
#endif
  playerButtonPressed[player] = true;
}

void handleDebugButtonPress() {
  Serial.println("[DEBUG] Skipping phase — marking all players complete");
  if (isSimonMarker()) {
    const MarkerPattern* pattern = phases[currentPhase].pattern;
    phasePlayerCompletedCount[currentPhase] = pattern->length;
    updatePhaseStatusLEDs();
    for (int p = 0; p < NUM_PLAYERS; p++) playerPhaseCompleted[p] = true;
    advancePhase();
  } else {
    for (int p = 0; p < NUM_PLAYERS; p++) {
      playerPhaseCompleted[p] = true;
      playerCurrentState[p] = STATE_DONE;
    }
    phasePlayerCompletedCount[currentPhase] = NUM_PLAYERS;
    updatePhaseStatusLEDs();
  }
}

void initGame() {
  currentPhase = 0;
  memset(phaseCompleted, false, sizeof(phaseCompleted));
  memset(phasePlayerCompletedCount, 0, sizeof(phasePlayerCompletedCount));
  memset(phaseLeds, 0, sizeof(phaseLeds));
  startPhase(0);
}

void runGameLoop() {
  if (!gameActive)
    return;

  if (isSimonMarker()) {
    runSimonPhase();
    return;
  }

  // Iterate over players to transition into hit/miss states based on button presses
  for (int p = 0; p < NUM_PLAYERS; p++) {
    if (playerButtonPressed[p]) {
      playerButtonPressed[p] = false;
      if (playerCurrentState[p] == STATE_NORMAL) {
        if (isHit(p)) {
          playerPhaseCompleted[p] = true;
          playerCurrentState[p] = STATE_HIT;
          playerEffectStartTime[p] = millis();
          phasePlayerCompletedCount[currentPhase]++;
          updatePhaseStatusLEDs();
        } else {
          playerMissedPos[p] =
              (isSoloMarker()) ? p * NUM_RING_LEDS + playerMarkerPos[p] : collectiveMarkerPos;
          if (phases[currentPhase].moveTargetOnMiss) {
            playerTargetMovePending[p] = true;
          }
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
      if (playerTargetMovePending[p]) {
        playerTargetMovePending[p] = false;
        int newTarget;
        do {
          newTarget = random(NUM_RING_LEDS);
        } while (newTarget == playerTargetPos[p] ||
                 (isSoloMarker() && newTarget == playerMarkerPos[p]));
        playerTargetPos[p] = newTarget;
      }
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
  if (now - lastMarkerMoveTime >= phases[currentPhase].speed) {
    lastMarkerMoveTime = now;
    advanceMarkers();
  }

  renderGameFrame();
}

void renderGameFrame() {
  memset(leds, 0, sizeof(leds));

  if (phases[currentPhase].type == SIMON) {
    const MarkerPattern* pattern = phases[currentPhase].pattern;
    if (simonState == SIMON_SHOW) {
      uint8_t ringIdx = pgm_read_byte(&pattern->leds[simonPlayStep]);
      fill_solid(&leds[ringIdx * NUM_RING_LEDS], NUM_RING_LEDS, PLAYER_COLORS[ringIdx]);
    } else if (simonState == SIMON_INPUT) {
      for (int p = 0; p < NUM_PLAYERS; p++) {
        int offset = p * NUM_RING_LEDS;
        leds[offset + LED_12_OCLOCK] = PLAYER_COLORS[p];
        leds[offset + LED_3_OCLOCK] = PLAYER_COLORS[p];
        leds[offset + LED_6_OCLOCK] = PLAYER_COLORS[p];
        leds[offset + LED_9_OCLOCK] = PLAYER_COLORS[p];
      }
      if (simonLastPressedRing >= 0 && millis() - simonPressTime < SIMON_HIT_MS) {
        fill_solid(&leds[simonLastPressedRing * NUM_RING_LEDS], NUM_RING_LEDS,
                   PLAYER_COLORS[simonLastPressedRing]);
      }
    } else if (simonState == SIMON_MISTAKE) {
      fill_solid(leds, NUM_LEDS, LED_MISS_COLOR);
    }
    // SIMON_PAUSE: all dark, already cleared by memset
    FastLED.show();
    return;
  }

  // Iterate over players to render target, marker, and hit/miss effects
  for (int p = 0; p < NUM_PLAYERS; p++) {
    int offset = p * NUM_RING_LEDS;
    int targetPos = offset + playerTargetPos[p];
    unsigned long elapsed = millis() - playerEffectStartTime[p];

    switch (playerCurrentState[p]) {
      case STATE_NORMAL:
        leds[targetPos] = LED_TARGET_COLOR;
        if (isSoloMarker()) {
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
  if (isSoloMarker()) {
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
                phases[phase].type == SOLO ? "solo" : "collective");
}

void startPhase(int phase) {
  logPhase(phase);

  for (int p = 0; p < NUM_PLAYERS; p++) {
    playerMarkerPos[p] = LED_12_OCLOCK;
    playerTargetPos[p] = LED_6_OCLOCK;
    playerTargetMovePending[p] = false;
    playerPhaseCompleted[p] = false;
    playerCurrentState[p] = STATE_NORMAL;
    playerEffectStartTime[p] = 0;
    playerButtonPressed[p] = false;
  }
  collectivePatternIndex = 0;
  const MarkerPattern* pat = phases[phase].pattern;
  collectiveMarkerPos = pat ? pgm_read_byte(&pat->leds[0]) : LED_12_OCLOCK;
  if (phases[phase].type == SIMON) {
    simonState = SIMON_SHOW;
    simonPlayStep = 0;
    simonInputStep = 0;
    simonLastPressedRing = -1;
    simonPressTime = 0;
    simonStepTime = millis();
  }
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

bool isSoloMarker() {
  return phases[currentPhase].type == SOLO;
}

bool isCollectiveMarker() {
  return phases[currentPhase].type == COLLECTIVE;
}

bool isSimonMarker() {
  return phases[currentPhase].type == SIMON;
}

void runSimonPhase() {
  unsigned long now = millis();
  const MarkerPattern* pattern = phases[currentPhase].pattern;
  unsigned long speed = phases[currentPhase].speed;

  switch (simonState) {
    case SIMON_SHOW:
      if (now - simonStepTime >= speed) {
        simonStepTime = now;
        simonState = SIMON_PAUSE;
      }
      break;

    case SIMON_PAUSE:
      if (now - simonStepTime >= SIMON_PAUSE_MS) {
        simonPlayStep++;
        if (simonPlayStep >= (int)pattern->length) {
          simonInputStep = 0;
          simonState = SIMON_INPUT;
        } else {
          simonStepTime = now;
          simonState = SIMON_SHOW;
        }
      }
      break;

    case SIMON_INPUT:
      // If the last step was just completed, wait for the hit flash to finish before advancing
      if (simonInputStep >= (int)pattern->length) {
        if (now - simonPressTime >= SIMON_HIT_MS) {
          for (int pp = 0; pp < NUM_PLAYERS; pp++) {
            playerPhaseCompleted[pp] = true;
          }
          advancePhase();
          return;
        }
        break;
      }
      for (int p = 0; p < NUM_PLAYERS; p++) {
        if (playerButtonPressed[p]) {
          playerButtonPressed[p] = false;
          uint8_t expectedRing = pgm_read_byte(&pattern->leds[simonInputStep]);
          if (p == (int)expectedRing) {
            simonLastPressedRing = p;
            simonPressTime = now;
            simonInputStep++;
            phasePlayerCompletedCount[currentPhase] = simonInputStep;
            updatePhaseStatusLEDs();
          } else {
            simonStepTime = now;
            simonState = SIMON_MISTAKE;
          }
          break;
        }
      }
      break;

    case SIMON_MISTAKE:
      if (now - simonStepTime >= SIMON_MISTAKE_MS) {
        simonPlayStep = 0;
        simonInputStep = 0;
        simonLastPressedRing = -1;
        phasePlayerCompletedCount[currentPhase] = 0;
        updatePhaseStatusLEDs();
        simonStepTime = now;
        simonState = SIMON_SHOW;
      }
      break;
  }

  renderGameFrame();
}

int getMarkerPos(int player) {
  return isSoloMarker() ? playerMarkerPos[player] : collectiveMarkerPos;
}

int getTargetPos(int player) {
  return isSoloMarker() ? playerTargetPos[player]
                        : player * NUM_RING_LEDS + playerTargetPos[player];
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

void updatePhaseStatusLEDs() {
  for (int col = 0; col < NUM_PHASE_COLS; col++) {
    int count = phasePlayerCompletedCount[col];
    bool colIsSimon = (col < NUM_PHASES && phases[col].type == SIMON);
    for (int row = 0; row < NUM_PHASE_ROWS; row++) {
      int ledIndex = (col % 2 == 0) ? col * NUM_PHASE_ROWS + row
                                    : col * NUM_PHASE_ROWS + (NUM_PHASE_ROWS - 1 - row);
      CRGB color = CRGB::Black;
      if (colIsSimon) {
        int thresholdOrange = (NUM_PHASE_ROWS - 1 - row) * 3 + 1;
        int thresholdYellow = (NUM_PHASE_ROWS - 1 - row) * 3 + 2;
        int thresholdGreen = (NUM_PHASE_ROWS - 1 - row) * 3 + 3;
        if (count >= thresholdGreen)
          color = CRGB::Green;
        else if (count >= thresholdYellow)
          color = CRGB::Yellow;
        else if (count >= thresholdOrange)
          color = CRGB::Orange;
      } else {
        int thresholdYellow = (NUM_PHASE_ROWS - 1 - row) * 2 + 1;
        int thresholdGreen = (NUM_PHASE_ROWS - 1 - row) * 2 + 2;
        if (count >= thresholdGreen)
          color = CRGB::Green;
        else if (count >= thresholdYellow)
          color = CRGB::Yellow;
      }
      phaseLeds[ledIndex] = color;
    }
  }
  FastLED.show();
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