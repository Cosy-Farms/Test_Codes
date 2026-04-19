// ╔══════════════════════════════════════════════════════════════╗
// ║         CosyBot v3.0 — CosyFarms Desk Robot                 ║
// ║  Board  : Arduino Mega 2560 (ESP32 D1 Mini ready)           ║
// ║  Display: SH1106G 128×64 OLED (I2C)                        ║
// ║  Author : Ishan | Embedded Intern @ CosyFarms               ║
// ╚══════════════════════════════════════════════════════════════╝
//
//  CYCLE (one full loop):
//  BOOT → [IDLE → GREET(0) → GREET(1) → ... → GREET(4) → INFO → FACT(0..4)] → repeat
//
//  FIXES vs v2:
//  • All strings in plain RAM (no PROGMEM pointer mess) — text shows correctly
//  • Cute ROBOT face (big square eyes, blush dots, happy beam mouth)
//  • Multiple expression states: happy, wink, excited, love, sleepy
//  • All 5 members greeted in ONE cycle before looping
//  • "Did You Know" header fits correctly (smaller font on that line)
//  • Greeting layout uses two-line name+role row so nothing clips

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SH110X.h>

// ── Display ────────────────────────────────────────────────
#define SCREEN_W   128
#define SCREEN_H    64
#define OLED_ADDR 0x3C
Adafruit_SH1106G oled(SCREEN_W, SCREEN_H, &Wire, -1);

// ── Timing ─────────────────────────────────────────────────
#define T_BOOT       2800UL
#define T_IDLE       3500UL   // idle face shown between boot and greetings
#define T_GREET      3800UL   // per member
#define T_INFO       3500UL
#define T_FACT       3800UL

// ── State ──────────────────────────────────────────────────
enum State : uint8_t {
  S_BOOT, S_IDLE, S_GREET, S_INFO, S_FACT
};

State    gState   = S_BOOT;
uint32_t gStateMs = 0;
uint8_t  gMember  = 0;   // 0-4, cycles through all
uint8_t  gFact    = 0;   // 0-4

// ── Blink / expression timer ───────────────────────────────
uint32_t gBlinkMs   = 0;
bool     gEyesOpen  = true;
uint8_t  gExpr      = 0;   // expression index for idle variety
uint32_t gExprMs    = 0;

// ── Logo (64×26) ───────────────────────────────────────────
#define LOGO_W 64
#define LOGO_H 26
static const uint8_t LOGO[] PROGMEM = {
  0x00,0x01,0xf8,0x7c,0x1f,0xb8,0x70,0x00,
  0x00,0x03,0xf8,0xfe,0x3f,0xb8,0x70,0x00,
  0x00,0x07,0xd9,0xc7,0x39,0xb8,0x70,0x00,
  0x00,0x07,0x03,0x83,0xb8,0x38,0x70,0x00,
  0x00,0x0e,0x03,0x83,0xb8,0x38,0x60,0x00,
  0x00,0x0e,0x03,0x6d,0x9f,0x38,0x60,0x00,
  0x00,0x0e,0x03,0x39,0x8f,0x9c,0xe0,0x00,
  0x00,0x0e,0x03,0x91,0x83,0x9f,0xe0,0x00,
  0x00,0x0e,0x03,0x93,0x81,0xcf,0xe0,0x00,
  0x00,0x07,0x01,0xd3,0x21,0xc2,0x60,0x00,
  0x00,0x07,0xf9,0xff,0x7f,0x99,0xe0,0x00,
  0x00,0x03,0xf8,0xfe,0x3f,0x9f,0xc0,0x00,
  0x00,0x00,0xf0,0x38,0x0e,0x1f,0x80,0x00,
  0x00,0x0f,0xef,0x1f,0x8e,0x39,0xf0,0x00,
  0x00,0x0f,0xef,0x1f,0xce,0x3b,0xf0,0x00,
  0x00,0x0e,0x0f,0x1c,0xce,0x7b,0x90,0x00,
  0x00,0x0e,0x0f,0x9c,0xef,0x7b,0x80,0x00,
  0x00,0x0e,0x0f,0x9c,0xef,0xfb,0x80,0x00,
  0x00,0x0f,0xdd,0x9d,0xcf,0xf9,0xe0,0x00,
  0x00,0x0f,0xdd,0x9f,0xcd,0xd8,0xf0,0x00,
  0x00,0x0e,0x1f,0x9f,0x8d,0xd8,0x70,0x00,
  0x00,0x0e,0x1f,0xdd,0x8c,0x98,0x38,0x00,
  0x00,0x0e,0x3f,0xdd,0xcc,0x1a,0x38,0x00,
  0x00,0x0e,0x38,0xdd,0xcc,0x1b,0xf0,0x00,
  0x00,0x0e,0x38,0xdc,0xfc,0x1b,0xf0,0x00,
  0x00,0x00,0x30,0xc0,0x00,0x18,0xc0,0x00
};

// ══════════════════════════════════════════════════════════
//  TEAM DATA  — plain RAM strings, no PROGMEM tricks
//  greeting / role / hype / tagline
// ══════════════════════════════════════════════════════════
struct Member {
  const char* greet;
  const char* role;
  const char* hype;
  const char* tagline;
};

static const Member TEAM[5] = {
  { "Hey Krishang!",   "CEO & Founder",    "You run the whole show",  "Dream. Lead. Grow!"  },
  { "Hey Pavitra!",    "Sales Lead",       "Hotels say YES to you!",  "Seal those greens!"  },
  { "Hey Vaibhav!",    "Sr. Agronomist",   "Plants luv u literally!", "Roots run deep!"     },
  { "Hey Saurabh!",    "Design Engineer",  "Designs hit different!",  "Make it beautiful!"  },
  { "Hey Ishan!",      "Embedded Intern",  "Circuits bow to you!",    "Keep building! :)"   },
};

// ══════════════════════════════════════════════════════════
//  FACTS — plain RAM, 3 lines each
// ══════════════════════════════════════════════════════════
struct Fact { const char* a; const char* b; const char* c; };

static const Fact FACTS[5] = {
  { "NFT = Nutrient",     "Film Technique",    "No soil needed!"        },
  { "Hydro grows 3x",     "faster than soil",  "More yield per sqft!"   },
  { "90% less water",     "than field farms",  "Every drop counts!"     },
  { "Lettuce & Basil",    "love NFT channels", "5-star chef approved!"  },
  { "Ebb & Flow floods",  "roots, then drains","Timed to perfection!"   },
};

// ══════════════════════════════════════════════════════════
//  FORWARD DECLARATIONS
// ══════════════════════════════════════════════════════════
void toState(State s);
void screenBoot();
void screenIdle(uint32_t now);
void screenGreet(uint8_t idx);
void screenInfo();
void screenFact(uint8_t idx);

// Robot face expressions
void faceHappy(bool eyesOpen);
void faceExcited();
void faceWink();
void faceLove();
void faceSleepy();
void faceGreeting();   // sparkle eyes for greeting

// Face primitives
void drawEyeOpen(int cx, int cy, int w, int h);
void drawEyeClosed(int cx, int cy, int w);
void drawEyeHalf(int cx, int cy, int w, int h);     // sleepy
void drawEyeHeart(int cx, int cy);                  // love
void drawEyeStars(int cx, int cy);                  // excited / sparkle
void drawMouthHappy(int cx, int cy);
void drawMouthBeam(int cx, int cy);                 // wide beam grin
void drawMouthSmall(int cx, int cy);                // tiny o
void drawBlush(int lx, int rx, int y);
void drawAntenna(int cx, int y);

// Utility
void printC(const char* s, int y);           // size-1 centred
void printC2(const char* s, int y);          // size-2 centred (for big welcome line)

// ══════════════════════════════════════════════════════════
//  SETUP
// ══════════════════════════════════════════════════════════
void setup() {
  oled.begin(OLED_ADDR, true);
  oled.setTextColor(SH110X_WHITE);
  oled.setTextWrap(false);

  screenBoot();
  toState(S_IDLE);
  gBlinkMs = millis();
  gExprMs  = millis();
}

// ══════════════════════════════════════════════════════════
//  MAIN LOOP
// ══════════════════════════════════════════════════════════
void loop() {
  uint32_t now = millis();
  uint32_t dt  = now - gStateMs;

  switch (gState) {

    case S_IDLE:
      screenIdle(now);
      if (dt >= T_IDLE) {
        gMember = 0;          // always start from member 0
        toState(S_GREET);
      }
      break;

    case S_GREET:
      screenGreet(gMember);
      if (dt >= T_GREET) {
        gMember++;
        if (gMember >= 5) {
          toState(S_INFO);    // all 5 greeted → move on
        } else {
          toState(S_GREET);   // next member
        }
      }
      break;

    case S_INFO:
      screenInfo();
      if (dt >= T_INFO) toState(S_FACT);
      break;

    case S_FACT:
      screenFact(gFact);
      if (dt >= T_FACT) {
        gFact = (gFact + 1) % 5;
        if (gFact == 0) toState(S_IDLE);   // all facts shown → restart
        else            toState(S_FACT);
      }
      break;

    default: break;
  }
}

// ──────────────────────────────────────────────────────────
void toState(State s) { gState = s; gStateMs = millis(); }

// ══════════════════════════════════════════════════════════
//  BOOT SCREEN
// ══════════════════════════════════════════════════════════
void screenBoot() {
  // Frame 1 — logo + company name
  oled.clearDisplay();
  oled.drawBitmap(32, 0, LOGO, LOGO_W, LOGO_H, SH110X_WHITE);
  oled.setTextSize(1);
  printC("CosyFarms",            30);
  printC("Automated Hydroponics", 41);
  printC("NFT  |  Ebb & Flow",   52);
  oled.display();
  delay(T_BOOT);

  // Frame 2 — robot wakes up
  oled.clearDisplay();
  faceHappy(true);
  oled.setTextSize(1);
  printC("Hello! I am CosyBot!", 57);
  oled.display();
  delay(1400);
}

// ══════════════════════════════════════════════════════════
//  IDLE — cycles through expressions
// ══════════════════════════════════════════════════════════
void screenIdle(uint32_t now) {
  // Blink every 3.5 s, stays closed 130 ms
  if ( gEyesOpen && (now - gBlinkMs) >= 3500) { gEyesOpen = false; gBlinkMs = now; }
  if (!gEyesOpen && (now - gBlinkMs) >=  130) { gEyesOpen = true;  gBlinkMs = now; }

  // Swap expression every 4 s
  if ((now - gExprMs) >= 4000) {
    gExpr = (gExpr + 1) % 3;
    gExprMs = now;
  }

  oled.clearDisplay();
  if (!gEyesOpen) {
    faceSleepy();   // use sleepy face for blink
  } else {
    switch (gExpr) {
      case 0: faceHappy(true); break;
      case 1: faceWink();      break;
      case 2: faceLove();      break;
    }
  }
  oled.setTextSize(1);
  printC("CosyBot  (^o^)", 57);
  oled.display();
}

// ══════════════════════════════════════════════════════════
//  GREETING  — text only, big and clear
// ══════════════════════════════════════════════════════════
void screenGreet(uint8_t idx) {
  const Member& m = TEAM[idx];
  oled.clearDisplay();

  // Small robot face top-right as decoration
  // Draw mini face: two small squares
  oled.drawRect(96,  2, 8, 7, SH110X_WHITE);
  oled.drawRect(108, 2, 8, 7, SH110X_WHITE);
  oled.fillRect(99,  4, 2, 3, SH110X_WHITE);
  oled.fillRect(111, 4, 2, 3, SH110X_WHITE);
  oled.drawFastHLine(98, 10, 16, SH110X_WHITE);   // mouth

  // Greeting name — size 2 if short enough, else size 1
  oled.setTextSize(1);
  printC(m.greet,   3);

  // Separator
  oled.drawFastHLine(4, 13, 120, SH110X_WHITE);

  // Role
  oled.setTextSize(1);
  printC(m.role,   20);

  // Another thin line
  oled.drawFastHLine(20, 30, 88, SH110X_WHITE);

  // Hype line
  printC(m.hype,   39);

  // Tagline — inverted pill at bottom
  int16_t x1, y1; uint16_t tw, th;
  oled.getTextBounds(m.tagline, 0, 0, &x1, &y1, &tw, &th);
  int px = (SCREEN_W - tw) / 2 - 4;
  oled.fillRoundRect(px, 50, tw + 8, 12, 3, SH110X_WHITE);
  oled.setTextColor(SH110X_BLACK);
  oled.setCursor((SCREEN_W - tw) / 2, 52);
  oled.print(m.tagline);
  oled.setTextColor(SH110X_WHITE);

  oled.display();
}

// ══════════════════════════════════════════════════════════
//  INFO SCREEN
// ══════════════════════════════════════════════════════════
void screenInfo() {
  oled.clearDisplay();
  oled.drawBitmap(32, 0, LOGO, LOGO_W, LOGO_H, SH110X_WHITE);
  oled.setTextSize(1);
  printC("CosyFarms",            30);
  printC("Hotel-Grade Greens",   41);
  printC("NFT | Ebb&Flow | IoT", 53);
  oled.display();
}

// ══════════════════════════════════════════════════════════
//  FACT SCREEN  — fixed header + 3 body lines + dots
// ══════════════════════════════════════════════════════════
void screenFact(uint8_t idx) {
  const Fact& f = FACTS[idx];
  oled.clearDisplay();

  // ── Header: inverted bar, size-1 text (fits "Did You Know?" fine)
  oled.fillRect(0, 0, 128, 12, SH110X_WHITE);
  oled.setTextColor(SH110X_BLACK);
  oled.setTextSize(1);
  printC("Did You Know?", 2);
  oled.setTextColor(SH110X_WHITE);

  // ── Three fact lines
  oled.setTextSize(1);
  printC(f.a, 18);
  printC(f.b, 30);
  printC(f.c, 42);

  // ── Progress dots (5 facts)
  const int dotR = 3, dotGap = 12;
  int x0 = (SCREEN_W - 5 * dotGap + dotGap - 2) / 2;
  for (uint8_t i = 0; i < 5; i++) {
    int cx = x0 + i * dotGap + dotR;
    if (i == idx) oled.fillCircle(cx, 59, dotR, SH110X_WHITE);
    else          oled.drawCircle(cx, 59, dotR, SH110X_WHITE);
  }

  oled.display();
}

// ══════════════════════════════════════════════════════════
//  ROBOT FACE EXPRESSIONS
//  Robot head centred at (64, 26)
//  Eyes are rounded rectangles, not circles
// ══════════════════════════════════════════════════════════

// Shared positions
#define FACE_CX   64
#define FACE_CY   26
#define EYE_LX    38    // left eye centre x
#define EYE_RX    90    // right eye centre x
#define EYE_Y     22    // eye centre y
#define EYE_W     16    // eye box width
#define EYE_H     13    // eye box height
#define MOUTH_Y   37    // mouth y

void drawAntenna(int cx, int topY) {
  oled.drawFastVLine(cx, topY,      5, SH110X_WHITE);
  oled.fillCircle   (cx, topY - 1, 2, SH110X_WHITE);
}

// Big open robot eye (rounded rect + pupil dot)
void drawEyeOpen(int cx, int cy, int w, int h) {
  oled.drawRoundRect(cx - w/2, cy - h/2, w, h, 3, SH110X_WHITE);
  oled.fillCircle(cx, cy + 1, 2, SH110X_WHITE);  // pupil
}

// Closed eye — just a horizontal line
void drawEyeClosed(int cx, int cy, int w) {
  oled.drawFastHLine(cx - w/2 + 1, cy, w - 2, SH110X_WHITE);
  oled.drawFastHLine(cx - w/2 + 2, cy+1, w - 4, SH110X_WHITE);
}

// Sleepy eye — filled bottom half of rect (droopy lid)
void drawEyeHalf(int cx, int cy, int w, int h) {
  oled.drawRoundRect(cx - w/2, cy - h/2, w, h, 3, SH110X_WHITE);
  // lid covers top half
  oled.fillRect(cx - w/2 + 1, cy - h/2 + 1, w - 2, h/2, SH110X_BLACK);
  oled.drawFastHLine(cx - w/2, cy, w, SH110X_WHITE);  // lid edge
}

// Heart eye  ♥
void drawEyeHeart(int cx, int cy) {
  // Two small circles + triangle = rough heart
  oled.fillCircle(cx - 3, cy - 2, 3, SH110X_WHITE);
  oled.fillCircle(cx + 3, cy - 2, 3, SH110X_WHITE);
  oled.fillTriangle(cx - 5, cy, cx + 5, cy, cx, cy + 5, SH110X_WHITE);
}

// Star / sparkle eye  *
void drawEyeStars(int cx, int cy) {
  // cross + diagonals
  oled.drawFastHLine(cx - 4, cy,     9, SH110X_WHITE);
  oled.drawFastVLine(cx,     cy - 4, 9, SH110X_WHITE);
  oled.drawLine(cx-3, cy-3, cx+3, cy+3, SH110X_WHITE);
  oled.drawLine(cx+3, cy-3, cx-3, cy+3, SH110X_WHITE);
}

// Blush dots (2 dots each cheek)
void drawBlush(int lx, int rx, int y) {
  oled.drawPixel(lx,   y,   SH110X_WHITE);
  oled.drawPixel(lx+2, y,   SH110X_WHITE);
  oled.drawPixel(lx+1, y+1, SH110X_WHITE);
  oled.drawPixel(rx,   y,   SH110X_WHITE);
  oled.drawPixel(rx+2, y,   SH110X_WHITE);
  oled.drawPixel(rx+1, y+1, SH110X_WHITE);
}

// Wide beam grin ———
void drawMouthBeam(int cx, int cy) {
  oled.drawFastHLine(cx - 11, cy,     23, SH110X_WHITE);
  oled.drawFastHLine(cx - 10, cy + 1, 21, SH110X_WHITE);
  oled.drawPixel    (cx - 11, cy + 1,     SH110X_WHITE);  // left corner curve
  oled.drawPixel    (cx + 11, cy + 1,     SH110X_WHITE);  // right corner curve
}

// Small happy curve
void drawMouthHappy(int cx, int cy) {
  oled.drawLine(cx-8, cy,   cx-4, cy+3, SH110X_WHITE);
  oled.drawLine(cx-4, cy+3, cx+4, cy+3, SH110X_WHITE);
  oled.drawLine(cx+4, cy+3, cx+8, cy,   SH110X_WHITE);
}

// tiny mouth
void drawMouthSmall(int cx, int cy) {
  oled.drawFastHLine(cx - 3, cy, 7, SH110X_WHITE);
}

// ── EXPRESSION: HAPPY (default) ───────────────────────────
void faceHappy(bool eyesOpen) {
  drawAntenna(FACE_CX, FACE_CY - 23);
  oled.drawRoundRect(FACE_CX - 30, FACE_CY - 18, 60, 38, 5, SH110X_WHITE);

  if (eyesOpen) {
    drawEyeOpen(EYE_LX, EYE_Y, EYE_W, EYE_H);
    drawEyeOpen(EYE_RX, EYE_Y, EYE_W, EYE_H);
  } else {
    drawEyeClosed(EYE_LX, EYE_Y, EYE_W);
    drawEyeClosed(EYE_RX, EYE_Y, EYE_W);
  }
  drawMouthBeam(FACE_CX, MOUTH_Y);
  drawBlush(EYE_LX - 11, EYE_RX + 8, EYE_Y + 9);
}

// ── EXPRESSION: WINK ──────────────────────────────────────
void faceWink() {
  drawAntenna(FACE_CX, FACE_CY - 23);
  oled.drawRoundRect(FACE_CX - 30, FACE_CY - 18, 60, 38, 5, SH110X_WHITE);
  drawEyeOpen  (EYE_LX, EYE_Y, EYE_W, EYE_H);
  drawEyeClosed(EYE_RX, EYE_Y, EYE_W);
  drawMouthHappy(FACE_CX, MOUTH_Y);
  drawBlush(EYE_LX - 11, EYE_RX + 8, EYE_Y + 9);
}

// ── EXPRESSION: LOVE ──────────────────────────────────────
void faceLove() {
  drawAntenna(FACE_CX, FACE_CY - 23);
  oled.drawRoundRect(FACE_CX - 30, FACE_CY - 18, 60, 38, 5, SH110X_WHITE);
  drawEyeHeart(EYE_LX, EYE_Y);
  drawEyeHeart(EYE_RX, EYE_Y);
  drawMouthBeam(FACE_CX, MOUTH_Y);
  drawBlush(EYE_LX - 11, EYE_RX + 8, EYE_Y + 9);
}

// ── EXPRESSION: SLEEPY (also used for blink) ──────────────
void faceSleepy() {
  drawAntenna(FACE_CX, FACE_CY - 23);
  oled.drawRoundRect(FACE_CX - 30, FACE_CY - 18, 60, 38, 5, SH110X_WHITE);
  drawEyeHalf(EYE_LX, EYE_Y, EYE_W, EYE_H);
  drawEyeHalf(EYE_RX, EYE_Y, EYE_W, EYE_H);
  drawMouthSmall(FACE_CX, MOUTH_Y + 1);
}

// ── EXPRESSION: EXCITED (star eyes, used for greeting) ────
void faceExcited() {
  drawAntenna(FACE_CX, FACE_CY - 23);
  oled.drawRoundRect(FACE_CX - 30, FACE_CY - 18, 60, 38, 5, SH110X_WHITE);
  drawEyeStars(EYE_LX, EYE_Y);
  drawEyeStars(EYE_RX, EYE_Y);
  drawMouthBeam(FACE_CX, MOUTH_Y);
  drawBlush(EYE_LX - 11, EYE_RX + 8, EYE_Y + 9);
}

// ══════════════════════════════════════════════════════════
//  UTILITY
// ══════════════════════════════════════════════════════════
void printC(const char* s, int y) {
  int16_t x1, y1; uint16_t w, h;
  oled.setTextSize(1);
  oled.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((SCREEN_W - (int16_t)w) / 2, y);
  oled.print(s);
}

void printC2(const char* s, int y) {
  int16_t x1, y1; uint16_t w, h;
  oled.setTextSize(2);
  oled.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((SCREEN_W - (int16_t)w) / 2, y);
  oled.print(s);
  oled.setTextSize(1);
}
