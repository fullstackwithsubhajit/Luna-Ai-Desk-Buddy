
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <RTClib.h> 

// ==========================================
// MOCHI DIY DESK COMPANION
// ESP32-C3 + 1.3" SH1106 OLED
// ==========================================

// HARDWARE
#define SDA_PIN 8
#define SCL_PIN 9
#define BUTTON_PIN 3
#define OLED_ADDRESS 0x3C

#define SCREEN_W 128
#define SCREEN_H 64

Adafruit_SH1106G display(128, 64, &Wire, -1);
RTC_DS3231 rtc;
bool rtcReady = false;

// Double-click clock overlay. A single click is delayed briefly so
// a second click can be recognized without triggering the normal action.
bool pendingSingleClick = false;
unsigned long firstClickReleasedAt = 0;
const unsigned long DOUBLE_CLICK_WINDOW = 380;

bool clockOverlayActive = false;
unsigned long clockOverlayStartedAt = 0;
const unsigned long CLOCK_DISPLAY_DURATION = 10000;

// ==========================================
// PERSONALITY STATES
// ==========================================

enum MochiState {
  IDLE,
  SAD,
  SLEEPING,
  WAKING,
  EXCITED
};

MochiState state = IDLE;

unsigned long stateStartedAt = 0;

// Timings
const unsigned long IDLE_TIMEOUT = 120000;
const unsigned long SAD_TIMEOUT = 60000;
const unsigned long EXCITED_DURATION = 3000;
const unsigned long WAKE_DURATION = 1500;

// ==========================================
// BUTTON
// ==========================================

bool lastButtonReading = HIGH;
bool buttonState = HIGH;
unsigned long lastDebounceTime = 0;

const unsigned long DEBOUNCE_MS = 35;

// ==========================================
// EYE MOVEMENT ENGINE
// ==========================================

float gazeX = 0;
float gazeY = 0;

float gazeStartX = 0;
float gazeStartY = 0;

float gazeTargetX = 0;
float gazeTargetY = 0;

unsigned long gazeStartedAt = 0;
unsigned long gazeDuration = 700;
unsigned long nextGazeAt = 0;

bool gazeMoving = false;

// ==========================================
// BLINK ENGINE
// ==========================================

unsigned long nextBlinkAt = 0;
unsigned long blinkStartedAt = 0;

bool blinking = false;

float eyeOpenAmount = 1.0;

// ==========================================
// DROWSY EYE ENGINE
// ==========================================

float drowsyGazeX = 0;
float drowsyGazeY = 0;

float drowsyStartX = 0;
float drowsyStartY = 0;

float drowsyTargetX = 0;
float drowsyTargetY = 0;

unsigned long drowsyGazeStartedAt = 0;
unsigned long drowsyGazeDuration = 1400;
unsigned long nextDrowsyGazeAt = 0;

bool drowsyGazeMoving = false;

float drowsyEyeOpenAmount = 1.0;

bool drowsyBlinking = false;

unsigned long drowsyBlinkStartedAt = 0;
unsigned long nextDrowsyBlinkAt = 0;

// Smooth transition from normal idle eyes into drowsy eyes.
bool drowsyTransitionActive = false;
unsigned long drowsyTransitionStartedAt = 0;
const unsigned long DROWSY_TRANSITION_DURATION = 1000;
float drowsyTransitionFromX = 0;
float drowsyTransitionFromY = 0;

// Sleeping mouth animation: relaxed smile -> tiny open yawn -> smile.
const unsigned long SLEEP_SNORE_DURATION = 850; // slower snore/yawn
const unsigned long SLEEP_MOUTH_CYCLE = 3600; // 850 ms snore + 2750 ms smile

// ==========================================
// ANIMATION FRAME RATE
// ==========================================

unsigned long lastFrameAt = 0;
const unsigned long FRAME_INTERVAL = 30;


// ==========================================
// FIXED IDLE PERSONALITY SCHEDULE
// Challenge face at 30 seconds.
// Cheeky wink face at 70 seconds.
// Upper-left playful face at 100 seconds.
// Drowsy state begins at 120 seconds.
// ==========================================

uint8_t challengePhase = 0;
// 0 = normal roaming
// 1 = return eyes to centre before face 1
// 2 = reveal face 1
// 3 = hold face 1
// 4 = smoothly return to normal after face 1
// 5 = reveal face 2 (cheeky wink)
// 6 = hold face 2
// 7 = smoothly return to normal after face 2
// The same reveal/hold/return phases are reused for face 3.

uint8_t challengeKind = 0;
bool firstFunnyFaceDone = false;
bool secondFunnyFaceDone = false;
bool thirdFunnyFaceDone = false;

float challengeStartX = 0;
float challengeStartY = 0;
unsigned long challengePhaseStartedAt = 0;
unsigned long challengeDuration = 550;

// ==========================================
// SMALL DRAWING HELPERS
// ==========================================

void drawSmile() {
  // Original smile geometry, increased to 3 pixels thick.
  display.drawLine(55, 49, 59, 53, SH110X_WHITE);
  display.drawLine(55, 50, 59, 54, SH110X_WHITE);
  display.drawLine(55, 51, 59, 55, SH110X_WHITE);

  display.drawLine(59, 53, 64, 54, SH110X_WHITE);
  display.drawLine(59, 54, 64, 55, SH110X_WHITE);
  display.drawLine(59, 55, 64, 56, SH110X_WHITE);

  display.drawLine(64, 54, 69, 53, SH110X_WHITE);
  display.drawLine(64, 55, 69, 54, SH110X_WHITE);
  display.drawLine(64, 56, 69, 55, SH110X_WHITE);

  display.drawLine(69, 53, 73, 49, SH110X_WHITE);
  display.drawLine(69, 54, 73, 50, SH110X_WHITE);
  display.drawLine(69, 55, 73, 51, SH110X_WHITE);
}

void drawFrown() {
  display.drawLine(55, 55, 59, 51, SH110X_WHITE);
  display.drawLine(59, 51, 64, 50, SH110X_WHITE);
  display.drawLine(64, 50, 69, 51, SH110X_WHITE);
  display.drawLine(69, 51, 73, 55, SH110X_WHITE);
}

void drawThickLine(int x1, int y1, int x2, int y2) {
  display.drawLine(x1, y1, x2, y2, SH110X_WHITE);
  display.drawLine(x1, y1 + 1, x2, y2 + 1, SH110X_WHITE);
  display.drawLine(x1, y1 - 1, x2, y2 - 1, SH110X_WHITE);
}

void updateDrowsyGaze(unsigned long now) {

  if (!drowsyGazeMoving) {

    if (now >= nextDrowsyGazeAt) {

      drowsyStartX = drowsyGazeX;
      drowsyStartY = drowsyGazeY;

      int direction = random(0, 5);

      switch (direction) {

        case 0:
          drowsyTargetX = -4;
          drowsyTargetY = 0;
          break;

        case 1:
          drowsyTargetX = 4;
          drowsyTargetY = 0;
          break;

        case 2:
          drowsyTargetX = -2;
          drowsyTargetY = 1;
          break;

        case 3:
          drowsyTargetX = 2;
          drowsyTargetY = 1;
          break;

        default:
          drowsyTargetX = 0;
          drowsyTargetY = 0;
          break;
      }

      drowsyGazeStartedAt = now;
      drowsyGazeDuration = random(1100, 1800);

      drowsyGazeMoving = true;
    }

    return;
  }

  float t = (float)(now - drowsyGazeStartedAt)
            / drowsyGazeDuration;

  if (t >= 1.0) {

    t = 1.0;

    drowsyGazeX = drowsyTargetX;
    drowsyGazeY = drowsyTargetY;

    drowsyGazeMoving = false;

    nextDrowsyGazeAt = now + random(1000, 2300);

    return;
  }

  float smooth = t * t * (3.0 - 2.0 * t);

  drowsyGazeX = drowsyStartX +
    (drowsyTargetX - drowsyStartX) * smooth;

  drowsyGazeY = drowsyStartY +
    (drowsyTargetY - drowsyStartY) * smooth;
}

void updateDrowsyBlink(unsigned long now) {

  if (!drowsyBlinking && now >= nextDrowsyBlinkAt) {

    drowsyBlinking = true;
    drowsyBlinkStartedAt = now;
  }

  if (!drowsyBlinking) {

    drowsyEyeOpenAmount = 1.0;
    return;
  }

  unsigned long elapsed = now - drowsyBlinkStartedAt;

  if (elapsed < 180) {

    drowsyEyeOpenAmount = 1.0 - elapsed / 180.0;

  } else if (elapsed < 260) {

    drowsyEyeOpenAmount = 0.0;

  } else if (elapsed < 500) {

    drowsyEyeOpenAmount = (elapsed - 260) / 240.0;

  } else {

    drowsyEyeOpenAmount = 1.0;

    drowsyBlinking = false;

    nextDrowsyBlinkAt = now + random(2500, 5000);
  }
}


// ==========================================
// SMOOTH EYE MOVEMENT
// ==========================================

void updateGaze(unsigned long now) {

  if (!gazeMoving) {

    if (now >= nextGazeAt) {

      gazeStartX = gazeX;
      gazeStartY = gazeY;

      // Choose a natural looking direction
      int direction = random(0, 7);

      switch (direction) {

        case 0:
          gazeTargetX = -7;
          gazeTargetY = 0;
          break;

        case 1:
          gazeTargetX = 7;
          gazeTargetY = 0;
          break;

        case 2:
          gazeTargetX = -4;
          gazeTargetY = -2;
          break;

        case 3:
          gazeTargetX = 4;
          gazeTargetY = -2;
          break;

        case 4:
          gazeTargetX = -4;
          gazeTargetY = 2;
          break;

        case 5:
          gazeTargetX = 4;
          gazeTargetY = 2;
          break;

        default:
          gazeTargetX = 0;
          gazeTargetY = 0;
          break;
      }

      gazeStartedAt = now;
      gazeDuration = random(400, 850);

      gazeMoving = true;
    }

    return;
  }

  unsigned long elapsed = now - gazeStartedAt;

  float t = (float)elapsed / gazeDuration;

  if (t >= 1.0) {

    t = 1.0;

    gazeX = gazeTargetX;
    gazeY = gazeTargetY;

    gazeMoving = false;

    nextGazeAt = now + random(500, 1600);

    return;
  }

  // Smoothstep interpolation
  float smooth = t * t * (3.0 - 2.0 * t);

  gazeX = gazeStartX +
          (gazeTargetX - gazeStartX) * smooth;

  gazeY = gazeStartY +
          (gazeTargetY - gazeStartY) * smooth;
}

// ==========================================
// BLINK ENGINE
// ==========================================

void updateBlink(unsigned long now) {

  if (!blinking && now >= nextBlinkAt) {

    blinking = true;
    blinkStartedAt = now;
  }

  if (!blinking) {
    eyeOpenAmount = 1.0;
    return;
  }

  unsigned long elapsed = now - blinkStartedAt;

  if (elapsed < 90) {

    eyeOpenAmount = 1.0 - (float)elapsed / 90.0;

  } else if (elapsed < 130) {

    eyeOpenAmount = 0.06;

  } else if (elapsed < 220) {

    eyeOpenAmount = (float)(elapsed - 130) / 90.0;

  } else {

    eyeOpenAmount = 1.0;

    blinking = false;

    nextBlinkAt = now + random(2500, 6000);
  }
}


// ==========================================
// FIXED-TIME PLAYFUL EXPRESSIONS
// ==========================================

void startChallenge(unsigned long now, uint8_t kind) {
  challengeKind = kind;
  challengePhase = 1;

  challengeStartX = gazeX;
  challengeStartY = gazeY;

  challengePhaseStartedAt = now;
  challengeDuration = 550;

  gazeMoving = false;
  blinking = false;
  eyeOpenAmount = 1.0;
}

void updateChallenge(unsigned long now) {

  unsigned long idleElapsed = now - stateStartedAt;

  // Trigger each funny expression only once per idle cycle.
  if (challengePhase == 0) {

    if (!firstFunnyFaceDone && idleElapsed >= 30000) {
      firstFunnyFaceDone = true;
      startChallenge(now, 1);
      return;
    }

    if (!secondFunnyFaceDone && idleElapsed >= 70000) {
      secondFunnyFaceDone = true;
      startChallenge(now, 2);
      return;
    }

    if (!thirdFunnyFaceDone && idleElapsed >= 100000) {
      thirdFunnyFaceDone = true;
      startChallenge(now, 3);
      return;
    }

    return;
  }

  unsigned long elapsed = now - challengePhaseStartedAt;
  float t = (float)elapsed / challengeDuration;
  if (t > 1.0) t = 1.0;

  float smooth = t * t * (3.0 - 2.0 * t);

  // Phase 1: return the normal eyes to centre.
  if (challengePhase == 1) {

    gazeX = challengeStartX * (1.0 - smooth);
    gazeY = challengeStartY * (1.0 - smooth);

    if (t >= 1.0) {
      gazeX = 0;
      gazeY = 0;
      challengePhase = 2;
      challengePhaseStartedAt = now;
      challengeDuration = 350;
    }
  }

  // Phase 2: reveal the selected funny expression.
  else if (challengePhase == 2) {

    if (t >= 1.0) {
      challengePhase = 3;
      challengePhaseStartedAt = now;
      challengeDuration = 1500;
    }
  }

  // Phase 3: hold the expression.
  else if (challengePhase == 3) {

    if (t >= 1.0) {
      challengePhase = 4;
      challengePhaseStartedAt = now;
      challengeDuration = 450;
    }
  }

  // Phase 4: smoothly return to the regular face.
  else if (challengePhase == 4) {

    if (t >= 1.0) {
      challengePhase = 0;
      challengeKind = 0;

      gazeX = 0;
      gazeY = 0;
      gazeMoving = false;

      nextGazeAt = now + 250;
      nextBlinkAt = now + random(1200, 2500);
    }
  }
}

// Filled quadrilateral helper for the first playful face.
void drawFilledQuad(
  int x1, int y1, int x2, int y2,
  int x3, int y3, int x4, int y4
) {
  display.fillTriangle(x1, y1, x2, y2, x3, y3, SH110X_WHITE);
  display.fillTriangle(x1, y1, x3, y3, x4, y4, SH110X_WHITE);
}

// First funny face: tilted/asymmetrical eyes and smile.
void drawChallengeFace(float reveal) {

  int leftTop = 30 - (int)(14 * reveal);
  int leftBottom = 30 + (int)(12 * reveal);
  int rightTop = 30 - (int)(18 * reveal);
  int rightBottom = 30 + (int)(14 * reveal);

  drawFilledQuad(
    24, leftTop - 1,
    43, leftTop + 4,
    41, leftBottom,
    25, leftBottom - 3
  );

  drawFilledQuad(
    85, rightTop + 5,
    105, rightTop - 5,
    109, rightBottom - 1,
    91, rightBottom + 3
  );

  drawSmile();
}

// Second funny face: cheeky asymmetric wink inspired by
// the bottom-left reference. Monochrome cheeks are white.
void drawWinkFace(float reveal) {

  int lift = (int)(reveal * 5);
  int cheekRadius = max(1, (int)(5 * reveal));

  // Left eye: thicker closed/winking curve.
  drawThickLine(17, 25 - lift, 27, 22 - lift);
  drawThickLine(27, 22 - lift, 39, 23 - lift);
  drawThickLine(39, 23 - lift, 48, 27 - lift);

  // Right eye: thicker slanted playful eye.
  drawThickLine(80, 29 - lift, 91, 24 - lift);
  drawThickLine(91, 24 - lift, 103, 20 - lift);
  drawThickLine(103, 20 - lift, 114, 18 - lift);

  // Small expressive accent marks above the eyes.
  display.drawLine(53, 13, 59, 11, SH110X_WHITE);
  display.drawLine(68, 15, 72, 18, SH110X_WHITE);

  // Cheek marks.
  display.fillCircle(20, 43, cheekRadius, SH110X_WHITE);
  display.fillCircle(108, 48, cheekRadius, SH110X_WHITE);

  // Playful little "3"-shaped mouth, now 3 pixels thick
  // using the same stroke thickness as the wink eyes.
  int mouthX = 56;
  int mouthY = 35;

  drawThickLine(mouthX, mouthY, mouthX + 7, mouthY - 2);
  drawThickLine(mouthX + 7, mouthY - 2, mouthX + 10, mouthY + 2);
  drawThickLine(mouthX + 10, mouthY + 2, mouthX + 6, mouthY + 6);
  drawThickLine(mouthX + 6, mouthY + 6, mouthX + 10, mouthY + 10);
  drawThickLine(mouthX + 10, mouthY + 10, mouthX + 6, mouthY + 14);
  drawThickLine(mouthX + 6, mouthY + 14, mouthX, mouthY + 13);
}

// Third funny face: animated LOVE EYES.
// Heart-shaped eyes gently pop into view, hold, then shrink
// smoothly back into the regular idle face.
void drawHeart(int cx, int cy, int radius) {
  if (radius < 2) return;

  int lobe = max(1, radius / 2);

  // Two rounded upper lobes and a tapered lower point.
  display.fillCircle(cx - lobe, cy - lobe / 2, lobe, SH110X_WHITE);
  display.fillCircle(cx + lobe, cy - lobe / 2, lobe, SH110X_WHITE);

  display.fillTriangle(
    cx - radius, cy - 1,
    cx + radius, cy - 1,
    cx, cy + radius + 2,
    SH110X_WHITE
  );

  // Tiny dark glints make the hearts feel glossy.
  if (radius >= 8) {
    display.drawPixel(cx - lobe, cy - lobe, SH110X_BLACK);
    display.drawPixel(cx + lobe, cy - lobe, SH110X_BLACK);
  }
}

void drawLoveEyesFace(float reveal) {

  // reveal = 0: normal idle eyes
  // reveal = 1: large love hearts
  // This lets the hearts smoothly grow out of the idle eyes,
  // then morph back into the normal rounded eyes.

  reveal = constrain(reveal, 0.0, 1.0);

  // A small overshoot makes the hearts feel like they pop up,
  // but they remain large throughout the hold phase.
  float pop = reveal + 0.10 * sin(reveal * 3.14159);
  pop = constrain(pop, 0.0, 1.10);

  // Normal idle eyes gradually disappear as the hearts take over.
  float idleOpen = 1.0 - reveal;
  int idleHeight = max(1, (int)(33.0 * idleOpen));
  int idleY = 12 + (33 - idleHeight) / 2;

  display.fillRoundRect(25, idleY, 25, idleHeight, 9, SH110X_WHITE);
  display.fillRoundRect(78, idleY, 25, idleHeight, 9, SH110X_WHITE);

  // Hearts grow to a larger, steady size while the expression is shown.
  int radius = (int)(15.0 * pop);
  if (radius >= 2) {
    drawHeart(37, 29, radius);
    drawHeart(91, 29, radius);
  }

  // Keep the familiar smile visible throughout the transition.
  drawSmile();
}

// ==========================================
// IDLE FACE
// LARGE ROUNDED RECTANGULAR EYES
// ==========================================

void drawIdleFace(float openness) {

  int eyeWidth = 25;
  int eyeHeight = max(2, (int)(33 * openness));

  // Keep eye centres stable while blinking
  int eyeY = 12 + (33 - eyeHeight) / 2;

  int leftX = 25 + (int)gazeX;
  int rightX = 78 + (int)gazeX;

  int verticalOffset = (int)gazeY;

  // Left eye
  display.fillRoundRect(
    leftX,
    eyeY + verticalOffset,
    eyeWidth,
    eyeHeight,
    9,
    SH110X_WHITE
  );

  // Right eye
  display.fillRoundRect(
    rightX,
    eyeY + verticalOffset,
    eyeWidth,
    eyeHeight,
    9,
    SH110X_WHITE
  );

  // Original small curved smile
  drawSmile();
}

// ==========================================
// EXCITED FACE
// ANGULAR INWARD-FACING EYES
// ==========================================

// Draw a deliberately bold stroke without changing other expressions.
void drawExcitedEyeLine(int x1, int y1, int x2, int y2) {
  // Five-pixel visual weight, using parallel strokes.
  display.drawLine(x1, y1, x2, y2, SH110X_WHITE);
  display.drawLine(x1, y1 - 1, x2, y2 - 1, SH110X_WHITE);
  display.drawLine(x1, y1 + 1, x2, y2 + 1, SH110X_WHITE);
  display.drawLine(x1 - 1, y1, x2 - 1, y2, SH110X_WHITE);
  display.drawLine(x1 + 1, y1, x2 + 1, y2, SH110X_WHITE);
}

void drawExcitedFace() {

  unsigned long elapsed = millis() - stateStartedAt;

  // Start with a small, already U-shaped eye rather than a flat line.
  // A gentle overshoot gives the eyes a soft, adorable pop.
  float t = constrain((float)elapsed / 520.0f, 0.0f, 1.0f);
  float ease = 1.0f - pow(1.0f - t, 3.0f);
  float bounce = sin(t * 3.1415926f) * 0.055f;
  float eyeScale = 0.68f + 0.32f * ease + bounce;

  // Transform coordinates around each eye's centre.
  int lx1 = 37 + (int)((20 - 37) * eyeScale);
  int lx3 = 37 + (int)((27 - 37) * eyeScale);
  int lx4 = 37 + (int)((36 - 37) * eyeScale);
  int lx5 = 37 + (int)((46 - 37) * eyeScale);
  int lx6 = 37 + (int)((51 - 37) * eyeScale);

  int rx1 = 91 + (int)((77 - 91) * eyeScale);
  int rx2 = 91 + (int)((82 - 91) * eyeScale);
  int rx3 = 91 + (int)((93 - 91) * eyeScale);
  int rx4 = 91 + (int)((104 - 91) * eyeScale);
  int rx5 = 91 + (int)((109 - 91) * eyeScale);

  int topY = 27 + (int)((13 - 27) * eyeScale);
  int shoulderY = 27;
  int curveY = 27 + (int)(7 * eyeScale);
  int bottomY = 27 + (int)(10 * eyeScale);

  // Left thick U-shaped eye.
  drawExcitedEyeLine(lx1, topY, lx1, shoulderY);
  drawExcitedEyeLine(lx1, shoulderY, lx3, curveY);
  drawExcitedEyeLine(lx3, curveY, lx4, bottomY);
  drawExcitedEyeLine(lx4, bottomY, lx5, curveY);
  drawExcitedEyeLine(lx5, curveY, lx6, shoulderY);
  drawExcitedEyeLine(lx6, shoulderY, lx6, topY);

  // Right thick U-shaped eye.
  drawExcitedEyeLine(rx1, topY, rx1, shoulderY);
  drawExcitedEyeLine(rx1, shoulderY, rx2, curveY);
  drawExcitedEyeLine(rx2, curveY, rx3, bottomY);
  drawExcitedEyeLine(rx3, bottomY, rx4, curveY);
  drawExcitedEyeLine(rx4, curveY, rx5, shoulderY);
  drawExcitedEyeLine(rx5, shoulderY, rx5, topY);

  // W-shaped delighted mouth gently scales in with the eyes.
  int mouthScale = (int)(eyeScale * 100);
  int mouthY = 46;
  int mouthHalfWidth = 10 * mouthScale / 100;
  int mouthDepth = 5 * mouthScale / 100;
  drawThickLine(64 - mouthHalfWidth, mouthY - 3,
                59, mouthY + mouthDepth);
  drawThickLine(59, mouthY + mouthDepth,
                64, mouthY - 1);
  drawThickLine(64, mouthY - 1,
                69, mouthY + mouthDepth);
  drawThickLine(69, mouthY + mouthDepth,
                64 + mouthHalfWidth, mouthY - 3);

  // Blushes pop in AFTER the eyes: squash small, then spring into
  // soft oval cheeks. On a monochrome OLED they render white.
  const unsigned long BLUSH_DELAY = 150;
  if (elapsed > BLUSH_DELAY) {
    float bt = constrain((float)(elapsed - BLUSH_DELAY) / 360.0f,
                         0.0f, 1.0f);
    float bEase = 1.0f - pow(1.0f - bt, 3.0f);
    float bBounce = sin(bt * 3.1415926f) * 0.16f;
    float blushScale = constrain(bEase + bBounce, 0.0f, 1.12f);

    int blushW = (int)(15.0f * blushScale);
    int blushH = (int)(8.0f * blushScale);

    if (blushW > 1 && blushH > 1) {
      int cheekY = 43 - (int)(2.0f * blushScale);
      display.fillRoundRect(12, cheekY, blushW, blushH,
                            blushH / 2, SH110X_WHITE);
      display.fillRoundRect(101, cheekY, blushW, blushH,
                            blushH / 2, SH110X_WHITE);

    }
  }
}

// ==========================================
// SAD FACE
// ==========================================

// ==========================================
// DROWSY FACE
// FILLED, PARTIALLY OPEN EYES
// ==========================================

void drawSadFace() {

  // Reference: bottom-right sleepy face.
  // Filled eyes with a flat upper lid and softly rounded lower edge.
  // The existing drowsy blink controls the eye height.
  int eyeHeight = 2 + (int)(13 * drowsyEyeOpenAmount);
  int eyeY = 25 + (15 - eyeHeight) / 2;

  int leftX = 25 + (int)drowsyGazeX;
  int rightX = 78 + (int)drowsyGazeX;
  int verticalOffset = (int)drowsyGazeY;

  // Left filled sleepy eye
  display.fillRoundRect(
    leftX,
    eyeY + verticalOffset,
    25,
    eyeHeight,
    5,
    SH110X_WHITE
  );
  // Square off the top edge to match the reference eyelid
  display.fillRect(
    leftX + 5,
    eyeY + verticalOffset,
    15,
    min(4, eyeHeight),
    SH110X_WHITE
  );

  // Right filled sleepy eye
  display.fillRoundRect(
    rightX,
    eyeY + verticalOffset,
    25,
    eyeHeight,
    5,
    SH110X_WHITE
  );
  display.fillRect(
    rightX + 5,
    eyeY + verticalOffset,
    15,
    min(4, eyeHeight),
    SH110X_WHITE
  );

  // Tiny filled sleepy mouth from the bottom-right reference
  display.fillRoundRect(59, 48, 10, 5, 3, SH110X_WHITE);
}

// ==========================================
// SMOOTH IDLE -> DROWSY FACE TRANSITION
// ==========================================

void drawDrowsyTransitionFace(unsigned long now) {

  float t = (float)(now - drowsyTransitionStartedAt)
            / DROWSY_TRANSITION_DURATION;
  t = constrain(t, 0.0, 1.0);

  // Smoothstep keeps the morph gentle at both ends.
  float eased = t * t * (3.0 - 2.0 * t);

  float currentX = drowsyTransitionFromX * (1.0 - eased);
  float currentY = drowsyTransitionFromY * (1.0 - eased);

  int eyeHeight = 33 - (int)(18 * eased);
  int eyeY = 12 + (33 - eyeHeight) / 2;

  int leftX = 25 + (int)currentX;
  int rightX = 78 + (int)currentX;
  int yOffset = (int)currentY;

  // Morph from the full idle rounded eyes into the flatter,
  // filled half-closed sleepy eyes.
  display.fillRoundRect(
    leftX, eyeY + yOffset, 25, eyeHeight,
    7, SH110X_WHITE
  );
  display.fillRoundRect(
    rightX, eyeY + yOffset, 25, eyeHeight,
    7, SH110X_WHITE
  );

  // Gradually flatten the upper eyelids as the eyes get sleepy.
  int flatHeight = (int)(4 * eased);
  if (flatHeight > 0) {
    display.fillRect(
      leftX + 5, eyeY + yOffset, 15, flatHeight, SH110X_WHITE
    );
    display.fillRect(
      rightX + 5, eyeY + yOffset, 15, flatHeight, SH110X_WHITE
    );
  }

  if (t < 0.65) {
    drawSmile();
  } else {
    // Crossfade isn't possible on a monochrome OLED; progressively
    // replace the broad smile with the small sleepy mouth.
    display.fillRoundRect(59, 48, 10, 5, 3, SH110X_WHITE);
  }
}

// ==========================================
// SLEEPING FACE
// ==========================================

void drawSleepingFace(unsigned long now) {

  // Closed curved eyes — thick, Dasai-inspired strokes.
  drawThickLine(25, 33, 30, 28);
  drawThickLine(30, 28, 38, 26);
  drawThickLine(38, 26, 46, 28);
  drawThickLine(46, 28, 52, 33);

  drawThickLine(76, 33, 82, 28);
  drawThickLine(82, 28, 90, 26);
  drawThickLine(90, 26, 98, 28);
  drawThickLine(98, 28, 104, 33);

  // Sleep loop: snore with Zzz and a filled open mouth,
  // then relax into a smile for 2.5 seconds, and repeat.
  // Timing starts when Mochi enters the sleeping state.
  unsigned long phase = (now - stateStartedAt) % SLEEP_MOUTH_CYCLE;

  if (phase < SLEEP_SNORE_DURATION) {

    // Fully filled open mouth — no dark centre/hole.
    float pulse = sin((float)phase / (float)SLEEP_SNORE_DURATION * 3.14159);
    int mouthW = 10 + (int)(3 * pulse);
    int mouthH = 8 + (int)(4 * pulse);

    display.fillRoundRect(
      64 - mouthW / 2, 44,
      mouthW, mouthH, 5, SH110X_WHITE
    );

    // Zzz floats while Mochi is snoring.
    int zBob = (now / 260) % 3;

    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);

    display.setCursor(91, 10 - zBob);
    display.print("z");

    display.setCursor(101, 4 + zBob);
    display.print("z");

    display.setCursor(111, 0);
    display.print("z");

  } else {

    // Quiet sleeping: thick relaxed smile for 2.5 seconds.
    drawThickLine(57, 47, 62, 49);
    drawThickLine(62, 49, 67, 47);
  }
}
// ==========================================
// WAKE-UP ANIMATION
// ==========================================

void drawWakeupFace(unsigned long now) {

  unsigned long elapsed = now - stateStartedAt;

  float progress = (float)elapsed / WAKE_DURATION;

  if (progress > 1.0) {
    progress = 1.0;
  }

  // Gradually open the eyes
  float openness = progress * progress *
                   (3.0 - 2.0 * progress);

  openness = 0.06 + openness * 0.94;

  // Look straight ahead during wake-up
  gazeX = 0;
  gazeY = 0;

  drawIdleFace(openness);
}

// ==========================================
// STATE MANAGEMENT
// ==========================================

void setState(MochiState newState) {

  state = newState;
  stateStartedAt = millis();

  if (newState != SAD) {
    drowsyTransitionActive = false;
  }

  if (newState == SLEEPING) {

    display.setContrast(12);

    gazeX = 0;
    gazeY = 0;

    blinking = false;

  } else {

    display.setContrast(207);
  }

  if (newState == IDLE) {

    challengePhase = 0;
    challengeKind = 0;
    firstFunnyFaceDone = false;
    secondFunnyFaceDone = false;
    thirdFunnyFaceDone = false;

    gazeX = 0;
    gazeY = 0;

    gazeMoving = false;

    nextGazeAt = millis() + 500;

    blinking = false;
    eyeOpenAmount = 1.0;

    nextBlinkAt = millis() + random(1500, 4000);
  }

if (newState == SAD) {

  drowsyTransitionActive = true;
  drowsyTransitionStartedAt = millis();
  drowsyTransitionFromX = gazeX;
  drowsyTransitionFromY = gazeY;

  drowsyGazeX = gazeX;
  drowsyGazeY = gazeY;
  drowsyGazeMoving = false;

  nextDrowsyGazeAt = millis() + DROWSY_TRANSITION_DURATION + 300;

  drowsyBlinking = false;
  drowsyEyeOpenAmount = 1.0;

  nextDrowsyBlinkAt = millis() + DROWSY_TRANSITION_DURATION + 1200;
}

}

// ==========================================
// BUTTON HANDLER
// ==========================================

void handleButtonPress() {

  switch (state) {

    case IDLE:
    case SAD:

      // Button press interrupts idle/drowsy and triggers excited face
      setState(EXCITED);
      break;

    case SLEEPING:

      setState(WAKING);
      break;

    case WAKING:
    case EXCITED:

      // Ignore additional presses during animations
      break;
  }
}

void updateButton() {

  unsigned long now = millis();
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastDebounceTime = now;
    lastButtonReading = reading;
  }

  if ((now - lastDebounceTime) > DEBOUNCE_MS && reading != buttonState) {
    buttonState = reading;

    // Count a click on release, after a complete press/release cycle.
    if (buttonState == HIGH) {
      if (clockOverlayActive) {
        // Ignore clicks while the clock is already being shown.
      } else if (pendingSingleClick &&
                 now - firstClickReleasedAt <= DOUBLE_CLICK_WINDOW) {
        // Second click: show the clock instead of firing two single clicks.
        pendingSingleClick = false;
        clockOverlayActive = true;
        clockOverlayStartedAt = now;
      } else {
        pendingSingleClick = true;
        firstClickReleasedAt = now;
      }
    }
  }

  // If no second click arrives, perform the original single-press action.
  if (pendingSingleClick &&
      now - firstClickReleasedAt > DOUBLE_CLICK_WINDOW) {
    pendingSingleClick = false;
    handleButtonPress();
  }
}

void drawClockScreen() {
  if (!rtcReady) {
    display.setTextColor(SH110X_WHITE);
    display.setTextSize(1);
    display.setCursor(20, 25);
    display.print("RTC NOT FOUND");
    display.setCursor(16, 39);
    display.print("Check DS3231 wire");
    return;
  }

  DateTime now = rtc.now();
  char timeBuffer[8];
  char dateBuffer[18];

  // 12-hour clock: hours and minutes only (no seconds).
  int hour12 = now.hour() % 12;
  if (hour12 == 0) hour12 = 12;
  snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d", hour12, now.minute());

  const char* weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  snprintf(dateBuffer, sizeof(dateBuffer), "%s %02d %s %04d",
           weekdays[now.dayOfTheWeek()], now.day(), months[now.month() - 1], now.year());

  // Greeting changes with the RTC's local hour.
  const char* greeting;
  if (now.hour() >= 5 && now.hour() < 12) {
    greeting = "GOOD MORNING";
  } else if (now.hour() >= 12 && now.hour() < 17) {
    greeting = "GOOD AFTERNOON";
  } else if (now.hour() >= 17 && now.hour() < 21) {
    greeting = "GOOD EVENING";
  } else {
    greeting = "GOOD NIGHT";
  }

  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  int16_t x1, y1;
  uint16_t w, h;

  display.getTextBounds(greeting, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - w) / 2, 5);
  display.print(greeting);

  // Large, clean HH:MM with a small AM/PM label alongside it.
  display.setTextSize(2);
  display.getTextBounds(timeBuffer, 0, 0, &x1, &y1, &w, &h);
  int timeX = (SCREEN_W - (w + 20)) / 2;
  display.setCursor(timeX, 23);
  display.print(timeBuffer);

  display.setTextSize(1);
  display.setCursor(timeX + w + 4, 29);
  display.print(now.hour() < 12 ? "AM" : "PM");

  display.getTextBounds(dateBuffer, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - w) / 2, 50);
  display.print(dateBuffer);
}

// ==========================================
// PERSONALITY TIMER
// ==========================================

void updatePersonality(unsigned long now) {

  unsigned long elapsed = now - stateStartedAt;

  switch (state) {

    case IDLE:

      if (elapsed >= IDLE_TIMEOUT) {
        setState(SAD);
      }

      break;

    case SAD:

      if (elapsed >= SAD_TIMEOUT) {
        setState(SLEEPING);
      }

      break;

    case WAKING:

      if (elapsed >= WAKE_DURATION) {
        setState(EXCITED);
      }

      break;

    case EXCITED:

      if (elapsed >= EXCITED_DURATION) {
        setState(IDLE);
      }

      break;

    case SLEEPING:
      break;
  }
}

// ==========================================
// MAIN RENDERER
// ==========================================

void renderMochi(unsigned long now) {

  display.clearDisplay();

  // Clock is a temporary overlay: personality timers keep running underneath.
  if (clockOverlayActive) {
    drawClockScreen();
    display.display();
    return;
  }

  switch (state) {

    case IDLE:

      if (challengePhase == 0) {
        updateGaze(now);
        updateBlink(now);
      }

      updateChallenge(now);

      if (challengePhase == 2 || challengePhase == 3 ||
          challengePhase == 4) {

        float reveal = 1.0;

        if (challengePhase == 2) {
          reveal = (float)(now - challengePhaseStartedAt)
                   / challengeDuration;
        }
        else if (challengePhase == 4) {
          reveal = 1.0 - (float)(now - challengePhaseStartedAt)
                   / challengeDuration;
        }

        reveal = constrain(reveal, 0.0, 1.0);

        if (challengeKind == 1) {
          drawChallengeFace(reveal);
        } else if (challengeKind == 2) {
          drawWinkFace(reveal);
        } else {
          drawLoveEyesFace(reveal);
        }
      }
      else {
        drawIdleFace(eyeOpenAmount);
      }

      break;

    case SAD:

      if (drowsyTransitionActive) {
        drawDrowsyTransitionFace(now);

        if (now - drowsyTransitionStartedAt >=
            DROWSY_TRANSITION_DURATION) {
          drowsyTransitionActive = false;
          drowsyGazeX = 0;
          drowsyGazeY = 0;
          nextDrowsyGazeAt = now + 300;
        }
      } else {
        updateDrowsyGaze(now);
        updateDrowsyBlink(now);
        drawSadFace();
      }

      break;

    case SLEEPING:

      drawSleepingFace(now);

      break;

    case WAKING:

      drawWakeupFace(now);

      break;

    case EXCITED:

      drawExcitedFace();

      break;
  }

  display.display();
}

// ==========================================
// SETUP
// ==========================================

void setup() {

  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  if (!display.begin(OLED_ADDRESS, true)) {

    Serial.println("OLED initialization failed!");

    while (true) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setContrast(207);

  // Initialize DS3231. If its backup power was lost, set it to
  // the date/time at which this sketch was compiled (one-time fallback).
  
// Initialize DS3231
if (rtc.begin()) {
    rtcReady = true;

    if (rtc.lostPower()) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    Serial.println("DS3231 RTC ready.");
}
else {
    rtcReady = false;
    Serial.println("DS3231 not detected.");
}


  randomSeed(esp_random());

  state = IDLE;
  stateStartedAt = millis();

  nextGazeAt = millis() + 1000;
  nextBlinkAt = millis() + 2500;

  Serial.println("MOCHI ONLINE!");
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {

  unsigned long now = millis();

  // Always check the button
  updateButton();

  if (clockOverlayActive &&
      now - clockOverlayStartedAt >= CLOCK_DISPLAY_DURATION) {
    clockOverlayActive = false;
  }

  // Update personality
  updatePersonality(now);

  // Render at approximately 33 FPS
  if (now - lastFrameAt >= FRAME_INTERVAL) {

    lastFrameAt = now;

    renderMochi(now);
  }
}
