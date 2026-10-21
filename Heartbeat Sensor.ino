/*
 * ======================================================================================
 * PROJECT NEAL DEFENCE WING - TACTICAL HEARTBEAT SENSOR MK-IV
 * Directly Adapted from Verified Source: cfong2001/EBYTE_E54 (E54_Radar.h & UIManager.h)
 * --------------------------------------------------------------------------------------
 * Features:
 *   - Exact State Machine Parser: SYNC_1(0xAA) -> SYNC_2(0xFF) -> SYNC_3(0x03) -> SYNC_4(0x00) -> 24B PAYLOAD -> 0x55 0xCC
 *   - Verified decodeValue() sign extraction from EBYTE_E54
 *   - Automatic target active detection (xRaw != 0 || yRaw != 0)
 *   - Dedicated High-Tech "UPLOADING FIRMWARE" Screen Mode
 *   - Red & White Telemetry Texts, Phosphor Sweep Trail, and ECG Heartbeat
 * ======================================================================================
 */

#include <Adafruit_GFX.h>
#include <MCUFRIEND_kbv.h>
#include "logo.h"

MCUFRIEND_kbv tft;

// --- Palette (RGB565) ---
#define COLOR_WHITE       0xFFFF
#define COLOR_BLACK       0x0000
#define COLOR_DARK_BG     0x0000
#define COLOR_GREEN_BRT   0x07E0   // Tactical Green
#define COLOR_GREEN_MED   0x0480   // Medium Green
#define COLOR_GREEN_DIM   0x0200   // Dim Green
#define COLOR_RED_BRT     0xF800   // Warning / Alert Red
#define COLOR_RED_DIM     0x8000   // Dim Red Frame
#define COLOR_BLUE_HALO   0x02DF   // Blue halo
#define COLOR_BLUE_CORE   0x07FF   // Cyan-blue intense core
#define COLOR_YELLOW_WARN 0xFFE0   // Yellow caution

// --- Hardware Pins ---
const int BUZZER_PIN = 10;

// --- Radar Geometry ---
const int ORIGIN_X = 160;
const int ORIGIN_Y = 210;
const int MAX_RANGE_MM = 6000; // 6.0 Meters
const int RADAR_RADIUS = 175;

// Exact RadarTarget structure from E54_Radar.h
struct E54Target {
  bool active;
  int16_t x;          // mm
  int16_t y;          // mm
  int16_t speed;      // cm/s
  uint16_t resolution;// mm
  uint16_t distance;  // mm
  int16_t prevScrX;
  int16_t prevScrY;
  unsigned long lastSeen;
};

E54Target targets[3];

// Sweep state & trails
float sweepAngle = -55.0;
float sweepStep = 4.2;
float trail1Angle = -55.0;
float trail2Angle = -55.0;
float eraseAngle = -55.0;
unsigned long lastSweepUpdate = 0;

// ECG animation state
int ecgX = 0;
int ecgPrevY = 227;
unsigned long lastEcgUpdate = 0;

// HUD Blink state
unsigned long lastBlinkTime = 0;
bool blinkState = false;

// Audio & Radar timers
unsigned long lastBeepTime = 0;
bool targetNearby = false;
uint8_t activeTargetCount = 0;

// Hardware Diagnostic Counters
uint32_t rxByteCount = 0;
uint8_t lastByteHex = 0x00;
uint32_t validFrameCount = 0;

// Exact State Machine from E54_Radar.h
enum RadarState {
  SYNC_1, SYNC_2, SYNC_3, SYNC_4,
  PAYLOAD,
  TAIL_1, TAIL_2
} radarState = SYNC_1;

uint8_t payloadBuffer[24];
uint8_t payloadIndex = 0;

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // High-Speed UART from E54_Radar.h (256000 baud)
  Serial.begin(256000);

  uint16_t ID = tft.readID();
  if (ID == 0xD3D3 || ID == 0x0000) ID = 0x9341;
  tft.begin(ID);
  tft.setRotation(1); // 320x240 Landscape

  playSmoothLogoIntro();
  playLinuxServerBoot();

  tft.fillScreen(COLOR_DARK_BG);
  drawMultiColorRadarHUD();
}

void loop() {
  processRadarSerial();
  updatePhosphorSweep();
  processRadarSerial();
  renderTargetsWithAlerts();
  updateBottomLeftHUD();
  updateECGHeartbeat();
  handleTacticalAudio();
}

// =====================================================================================
// AUTHENTIC GEIGER-MULLER MECHANICAL CLICK (No long beeps, no harsh pitch)
// =====================================================================================
void geigerClick() {
  digitalWrite(BUZZER_PIN, HIGH);
  delayMicroseconds(500); // 0.5ms pulse produces a crisp mechanical pop/click
  digitalWrite(BUZZER_PIN, LOW);
}

// =====================================================================================
// DEDICATED FIRMWARE UPLOAD SCREEN (Triggered during upload)
// =====================================================================================
void showFirmwareUploadScreen() {
  tft.fillScreen(COLOR_DARK_BG);
  tft.drawRect(6, 6, 308, 228, COLOR_RED_BRT);
  tft.drawRect(8, 8, 304, 224, COLOR_RED_BRT);

  tft.fillRect(10, 10, 300, 24, COLOR_RED_BRT);
  tft.setTextColor(COLOR_WHITE, COLOR_RED_BRT);
  tft.setTextSize(1);
  tft.setCursor(20, 18);
  tft.print(F(">>> FIRMWARE UPGRADE IN PROGRESS <<<"));

  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.setCursor(20, 48);
  tft.print(F("PROJECT NEAL DEFENCE WING // FLASHER"));

  tft.setTextColor(COLOR_GREEN_BRT, COLOR_DARK_BG);
  tft.setCursor(20, 70);
  tft.print(F("TARGET: ATmega328P [COM5]"));

  tft.setTextColor(COLOR_YELLOW_WARN, COLOR_DARK_BG);
  tft.setCursor(20, 92);
  tft.print(F("STATUS: FLASHING VIA AVRDUDE..."));

  // Progress Bar
  tft.drawRect(20, 118, 280, 18, COLOR_GREEN_DIM);
  tft.fillRect(22, 120, 276, 14, COLOR_GREEN_BRT);

  tft.setTextColor(COLOR_RED_BRT, COLOR_DARK_BG);
  tft.setCursor(22, 155);
  tft.print(F("CRITICAL: DO NOT CUT POWER"));

  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.setCursor(22, 178);
  tft.print(F("BOOTLOADER ACTIVE // VERIFYING FLASH"));

  geigerClick();
  delay(60);
  geigerClick();
}

// =====================================================================================
// STEP 1: SMOOTH ANIMATED LOGO INTRO
// =====================================================================================
void playSmoothLogoIntro() {
  tft.fillScreen(COLOR_BLACK);

  int logoX = (320 - LOGO_WIDTH) / 2;
  int logoY = 70;

  for (int h = 0; h <= 85; h += 5) {
    tft.fillRect(0, 120 - h, 320, h * 2, COLOR_WHITE);
    delay(10);
  }

  tft.drawBitmap(logoX, logoY, logo_black_bits, LOGO_WIDTH, LOGO_HEIGHT, COLOR_BLACK);
  tft.drawBitmap(logoX, logoY, logo_red_bits, LOGO_WIDTH, LOGO_HEIGHT, COLOR_RED_BRT);

  tft.setTextColor(COLOR_BLACK, COLOR_WHITE);
  tft.setTextSize(1);
  tft.setCursor(54, logoY + LOGO_HEIGHT + 12);
  tft.print(F("PROJECT NEAL DEFENCE WING"));

  tft.setTextColor(COLOR_RED_BRT, COLOR_WHITE);
  tft.setCursor(68, logoY + LOGO_HEIGHT + 24);
  tft.print(F("TACTICAL AVIONICS & RECON"));

  for (int x = logoX - 4; x <= logoX + LOGO_WIDTH + 4; x += 10) {
    tft.drawFastVLine(x, logoY - 6, LOGO_HEIGHT + 12, COLOR_RED_BRT);
    if (x % 40 == 0) geigerClick();
    delay(12);
    tft.drawFastVLine(x, logoY - 6, LOGO_HEIGHT + 12, COLOR_WHITE);
  }
  tft.drawBitmap(logoX, logoY, logo_black_bits, LOGO_WIDTH, LOGO_HEIGHT, COLOR_BLACK);
  tft.drawBitmap(logoX, logoY, logo_red_bits, LOGO_WIDTH, LOGO_HEIGHT, COLOR_RED_BRT);

  int bx = logoX - 6;
  int by = logoY - 6;
  int bw = LOGO_WIDTH + 12;
  int bh = LOGO_HEIGHT + 12;

  for (int flash = 0; flash < 2; flash++) {
    drawCornerBrackets(bx, by, bw, bh, 12, COLOR_RED_BRT);
    geigerClick();
    delay(90);
    drawCornerBrackets(bx, by, bw, bh, 12, COLOR_WHITE);
    delay(60);
  }
  drawCornerBrackets(bx, by, bw, bh, 12, COLOR_RED_BRT);

  geigerClick();
  delay(40);
  geigerClick();
  delay(600);
}

void drawCornerBrackets(int x, int y, int w, int h, int len, uint16_t color) {
  tft.drawFastHLine(x, y, len, color);
  tft.drawFastVLine(x, y, len, color);
  tft.drawFastHLine(x + w - len, y, len, color);
  tft.drawFastVLine(x + w - 1, y, len, color);
  tft.drawFastHLine(x, y + h - 1, len, color);
  tft.drawFastVLine(x, y + h - len, len, color);
  tft.drawFastHLine(x + w - len, y + h - 1, len, color);
  tft.drawFastVLine(x + w - 1, y + h - len, len, color);
}

// =====================================================================================
// STEP 2: FUTURISTIC LINUX SERVER KERNEL BOOT
// =====================================================================================
void printBootLine(int y, uint8_t cs, const __FlashStringHelper* msg, bool isRedTag = false) {
  tft.setCursor(6, y);
  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.print(F("[ 0."));
  if (cs < 10) tft.print('0');
  tft.print(cs);
  tft.print(F(" ] "));
  tft.print(msg);

  if (isRedTag) {
    tft.setTextColor(COLOR_RED_BRT, COLOR_DARK_BG);
    tft.print(F(" [ SECURE ]"));
  } else {
    tft.setTextColor(COLOR_GREEN_BRT, COLOR_DARK_BG);
    tft.print(F(" [ OK ]"));
  }

  geigerClick();
  delay(65);
}

void playLinuxServerBoot() {
  tft.fillScreen(COLOR_DARK_BG);
  tft.setTextSize(1);

  tft.setTextColor(COLOR_RED_BRT, COLOR_DARK_BG);
  tft.setCursor(6, 4);
  tft.print(F("DEFENCE WING // "));
  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.println(F("KERNEL v6.8.0-TVC"));
  tft.drawFastHLine(6, 14, 308, COLOR_RED_DIM);

  printBootLine(22,  12, F("Mounting /dev/radar0 [24GHz FMCW]"));
  printBootLine(40,  28, F("SECURITY: DEFENCE WING AUTHORIZED"), true);
  printBootLine(58,  50, F("UART0 Active: 256000 8N1 [Pin 0]"));
  printBootLine(76,  72, F("Azimuth Beamforming: +/-55' FOV"));
  printBootLine(94,  88, F("Tactical Recon Daemon [OK]"));
  printBootLine(112, 99, F("Vector HUD Graphics Engine Ready"));

  int barY = 175;
  tft.drawRect(20, barY, 280, 14, COLOR_RED_BRT);
  for (int p = 0; p <= 276; p += 8) {
    tft.fillRect(22, barY + 2, p, 10, COLOR_GREEN_BRT);
    if (p % 48 == 0) geigerClick();
    delay(8);
  }

  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.setCursor(62, 202);
  tft.print(F("DEFENCE WING: "));
  tft.setTextColor(COLOR_GREEN_BRT, COLOR_DARK_BG);
  tft.print(F("100% OPERATIONAL."));

  geigerClick();
  delay(70);
  geigerClick();
  delay(200);
}

// =====================================================================================
// STEP 3: MULTI-COLOR RADAR HUD
// =====================================================================================
void drawMultiColorRadarHUD() {
  tft.fillRect(4, 4, 48, 12, COLOR_RED_BRT);
  tft.setTextColor(COLOR_WHITE, COLOR_RED_BRT);
  tft.setTextSize(1);
  tft.setCursor(8, 6);
  tft.print(F("NEAL"));

  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.setCursor(56, 6);
  tft.print(F("DEFENCE WING"));

  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.setCursor(185, 6);
  tft.print(F("FREQ:24G"));
  tft.setTextColor(COLOR_RED_BRT, COLOR_DARK_BG);
  tft.setCursor(245, 6);
  tft.print(F("REC [ACT]"));

  tft.drawFastHLine(4, 18, 312, COLOR_GREEN_DIM);

  int r2m = (2000L * RADAR_RADIUS) / MAX_RANGE_MM;
  int r4m = (4000L * RADAR_RADIUS) / MAX_RANGE_MM;
  int r6m = RADAR_RADIUS;

  tft.drawCircle(ORIGIN_X, ORIGIN_Y, r2m, COLOR_GREEN_DIM);
  tft.drawCircle(ORIGIN_X, ORIGIN_Y, r4m, COLOR_GREEN_DIM);
  tft.drawCircle(ORIGIN_X, ORIGIN_Y, r6m, COLOR_GREEN_BRT);
  tft.drawCircle(ORIGIN_X, ORIGIN_Y, r6m + 1, COLOR_GREEN_BRT);

  drawRadialSectorLine(-55.0, COLOR_GREEN_BRT);
  drawRadialSectorLine(-30.0, COLOR_GREEN_DIM);
  drawRadialSectorLine(0.0,   COLOR_GREEN_DIM);
  drawRadialSectorLine(30.0,  COLOR_GREEN_DIM);
  drawRadialSectorLine(55.0,  COLOR_GREEN_BRT);

  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.setCursor(ORIGIN_X - 165, ORIGIN_Y - 116); tft.print(F("-55'"));
  tft.setCursor(ORIGIN_X - 100, ORIGIN_Y - 160); tft.print(F("-30'"));
  tft.setCursor(ORIGIN_X - 9,   ORIGIN_Y - r6m - 10); tft.print(F("000'"));
  tft.setCursor(ORIGIN_X + 80,  ORIGIN_Y - 160); tft.print(F("+30'"));
  tft.setCursor(ORIGIN_X + 144, ORIGIN_Y - 116); tft.print(F("+55'"));

  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.setCursor(ORIGIN_X + 5, ORIGIN_Y - r2m + 2); tft.print(F("2M"));
  tft.setCursor(ORIGIN_X + 5, ORIGIN_Y - r4m + 2); tft.print(F("4M"));
  tft.setCursor(ORIGIN_X + 5, ORIGIN_Y - r6m + 2); tft.print(F("6M"));

  tft.fillCircle(ORIGIN_X, ORIGIN_Y, 5, COLOR_GREEN_BRT);
  tft.fillCircle(ORIGIN_X, ORIGIN_Y, 2, COLOR_WHITE);
  tft.drawLine(ORIGIN_X - 3, ORIGIN_Y + 5, ORIGIN_X, ORIGIN_Y + 2, COLOR_RED_BRT);
  tft.drawLine(ORIGIN_X + 3, ORIGIN_Y + 5, ORIGIN_X, ORIGIN_Y + 2, COLOR_RED_BRT);

  tft.drawRect(2, 2, 316, 236, COLOR_GREEN_DIM);
  tft.drawFastHLine(2, 18, 10, COLOR_RED_BRT);
  tft.drawFastVLine(18, 2, 10, COLOR_RED_BRT);
  tft.drawFastHLine(308, 18, 10, COLOR_RED_BRT);
  tft.drawFastVLine(302, 2, 10, COLOR_RED_BRT);

  tft.drawRect(4, 182, 126, 52, COLOR_RED_DIM);
  tft.drawFastHLine(4, 182, 14, COLOR_RED_BRT);
  tft.drawFastVLine(4, 182, 14, COLOR_RED_BRT);

  tft.drawRect(206, 218, 110, 18, COLOR_GREEN_DIM);
  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.setCursor(180, 222);
  tft.print(F("HRT:"));
}

void drawRadialSectorLine(float angleDeg, uint16_t color) {
  float rad = (angleDeg - 90.0) * 0.0174533;
  int xEnd = ORIGIN_X + cos(rad) * RADAR_RADIUS;
  int yEnd = ORIGIN_Y + sin(rad) * RADAR_RADIUS;
  tft.drawLine(ORIGIN_X, ORIGIN_Y, xEnd, yEnd, color);
}

// =====================================================================================
// STEP 4: BOTTOM-LEFT HUD TELEMETRY & LIVE DIAGNOSTICS
// =====================================================================================
void updateBottomLeftHUD() {
  unsigned long now = millis();
  static unsigned long lastHudTime = 0;
  if (now - lastHudTime < 140) return;
  lastHudTime = now;

  if (now - lastBlinkTime > 400) {
    blinkState = !blinkState;
    lastBlinkTime = now;
  }

  tft.fillRect(6, 184, 122, 48, COLOR_DARK_BG);
  tft.setTextSize(1);

  // Line 1: Target Count / Status
  tft.setCursor(8, 186);
  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.print(F("TGT:"));
  if (activeTargetCount > 0) {
    tft.setTextColor(COLOR_RED_BRT, COLOR_DARK_BG);
    tft.print(activeTargetCount);
    tft.print(F(" LOCK [HOSTILE]"));
  } else {
    tft.setTextColor(COLOR_GREEN_BRT, COLOR_DARK_BG);
    tft.print(F("0 [SEARCHING]"));
  }

  // Line 2: RX Byte Counter & Distance
  tft.setCursor(8, 196);
  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.print(F("RX:"));
  tft.setTextColor(COLOR_GREEN_BRT, COLOR_DARK_BG);
  tft.print(rxByteCount);
  if (targets[0].active) {
    tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
    tft.print(F(" D:"));
    tft.setTextColor(COLOR_RED_BRT, COLOR_DARK_BG);
    tft.print((float)targets[0].distance / 1000.0, 1);
    tft.print(F("m"));
  }

  // Line 3: Last Hex Byte & Valid Packet Counter
  tft.setCursor(8, 206);
  tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
  tft.print(F("H:0x"));
  if (lastByteHex < 0x10) tft.print('0');
  tft.print(lastByteHex, HEX);
  tft.print(F(" PKT:"));
  tft.setTextColor(validFrameCount > 0 ? COLOR_GREEN_BRT : COLOR_WHITE, COLOR_DARK_BG);
  tft.print(validFrameCount);

  // Line 4: Threat Warning or Active Status
  tft.setCursor(8, 218);
  if (targetNearby) {
    if (blinkState) {
      tft.setTextColor(COLOR_WHITE, COLOR_RED_BRT);
      tft.print(F(" ! HOSTILE NEAR ! "));
    } else {
      tft.setTextColor(COLOR_RED_BRT, COLOR_DARK_BG);
      tft.print(F(" ! HOSTILE NEAR ! "));
    }
  } else {
    tft.setTextColor(COLOR_GREEN_BRT, COLOR_DARK_BG);
    tft.print(F("RADAR ACTIVE 24G"));
  }
}

// =====================================================================================
// STEP 5: PHOSPHOR CRT SWEEP TRAIL
// =====================================================================================
void updatePhosphorSweep() {
  unsigned long now = millis();
  if (now - lastSweepUpdate < 22) return;
  lastSweepUpdate = now;

  if (eraseAngle >= -55.0 && eraseAngle <= 55.0) {
    drawSweepLine(eraseAngle, COLOR_DARK_BG);
    restoreGridPixels(eraseAngle);
  }

  if (trail2Angle >= -55.0 && trail2Angle <= 55.0) {
    drawSweepLine(trail2Angle, COLOR_GREEN_DIM);
  }

  if (trail1Angle >= -55.0 && trail1Angle <= 55.0) {
    drawSweepLine(trail1Angle, COLOR_GREEN_MED);
  }

  drawSweepLine(sweepAngle, COLOR_GREEN_BRT);

  tft.fillCircle(ORIGIN_X, ORIGIN_Y, 5, COLOR_GREEN_BRT);
  tft.fillCircle(ORIGIN_X, ORIGIN_Y, 2, COLOR_WHITE);

  eraseAngle = trail2Angle;
  trail2Angle = trail1Angle;
  trail1Angle = sweepAngle;

  sweepAngle += sweepStep;
  if (sweepAngle >= 55.0 || sweepAngle <= -55.0) {
    sweepStep = -sweepStep;
  }
}

void drawSweepLine(float deg, uint16_t color) {
  float rad = (deg - 90.0) * 0.0174533;
  int xEnd = ORIGIN_X + cos(rad) * (RADAR_RADIUS - 2);
  int yEnd = ORIGIN_Y + sin(rad) * (RADAR_RADIUS - 2);
  tft.drawLine(ORIGIN_X, ORIGIN_Y, xEnd, yEnd, color);
}

void restoreGridPixels(float deg) {
  float rad = (deg - 90.0) * 0.0174533;
  int r2m = (2000L * RADAR_RADIUS) / MAX_RANGE_MM;
  int r4m = (4000L * RADAR_RADIUS) / MAX_RANGE_MM;
  tft.drawPixel(ORIGIN_X + cos(rad) * r2m, ORIGIN_Y + sin(rad) * r2m, COLOR_GREEN_DIM);
  tft.drawPixel(ORIGIN_X + cos(rad) * r4m, ORIGIN_Y + sin(rad) * r4m, COLOR_GREEN_DIM);
  tft.drawPixel(ORIGIN_X + cos(rad) * RADAR_RADIUS, ORIGIN_Y + sin(rad) * RADAR_RADIUS, COLOR_GREEN_BRT);
}

// =====================================================================================
// STEP 6: RENDER TARGETS (From EBYTE_E54 / UIManager.h)
// =====================================================================================
void renderTargetsWithAlerts() {
  unsigned long now = millis();
  static unsigned long lastTargetTime = 0;
  if (now - lastTargetTime < 45) return;
  lastTargetTime = now;

  targetNearby = false;
  activeTargetCount = 0;

  for (int i = 0; i < 3; i++) {
    bool isVisible = targets[i].active && (now - targets[i].lastSeen < 1500);
    int16_t absY = abs(targets[i].y);

    if (isVisible && absY > 50 && absY <= MAX_RANGE_MM) {
      targetNearby = true;
      activeTargetCount++;

      // Coordinate mapping from UIManager.h:
      // screenX = originX + (x * scale)
      // screenY = originY - (y * scale)
      int scrX = ORIGIN_X + (int)((long)targets[i].x * RADAR_RADIUS / MAX_RANGE_MM);
      int scrY = ORIGIN_Y - (int)((long)absY * RADAR_RADIUS / MAX_RANGE_MM);

      scrX = constrain(scrX, 20, 300);
      scrY = constrain(scrY, 26, 205);

      if (targets[i].prevScrX != scrX || targets[i].prevScrY != scrY) {
        eraseTargetLock(targets[i].prevScrX, targets[i].prevScrY);
      }

      uint16_t bracketColor = (targets[i].distance < 2500) ? COLOR_RED_BRT : COLOR_GREEN_BRT;
      drawLockBrackets(scrX, scrY, bracketColor);

      tft.fillCircle(scrX, scrY, 4, COLOR_BLUE_HALO);
      tft.fillCircle(scrX, scrY, 2, COLOR_BLUE_CORE);

      // Distance tag in meters
      tft.setTextSize(1);
      tft.setCursor(scrX + 10, scrY - 7);
      if (targets[i].distance < 2500) {
        tft.setTextColor(COLOR_RED_BRT, COLOR_DARK_BG);
        tft.print(F("WARN!"));
      } else {
        tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
        tft.print(F("TGT#")); tft.print(i + 1);
      }

      tft.setTextColor(COLOR_WHITE, COLOR_DARK_BG);
      tft.setCursor(scrX + 10, scrY + 3);
      tft.print((float)targets[i].distance / 1000.0, 1);
      tft.print(F("m"));

      targets[i].prevScrX = scrX;
      targets[i].prevScrY = scrY;
    } else {
      if (targets[i].prevScrX > 0) {
        eraseTargetLock(targets[i].prevScrX, targets[i].prevScrY);
        targets[i].prevScrX = 0;
        targets[i].prevScrY = 0;
      }
    }
  }
}

void drawLockBrackets(int x, int y, uint16_t color) {
  tft.drawFastVLine(x - 7, y - 5, 11, color);
  tft.drawFastHLine(x - 7, y - 5, 3, color);
  tft.drawFastHLine(x - 7, y + 5, 3, color);

  tft.drawFastVLine(x + 7, y - 5, 11, color);
  tft.drawFastHLine(x + 5, y - 5, 3, color);
  tft.drawFastHLine(x + 5, y + 5, 3, color);
}

void eraseTargetLock(int x, int y) {
  if (x <= 0 || y <= 0) return;
  tft.fillRect(x - 8, y - 11, 17, 24, COLOR_DARK_BG);
  tft.fillRect(x + 9, y - 8, 42, 20, COLOR_DARK_BG);
}

// =====================================================================================
// STEP 7: DYNAMIC ECG WAVEFORM
// =====================================================================================
void updateECGHeartbeat() {
  unsigned long now = millis();
  if (now - lastEcgUpdate < 38) return;
  lastEcgUpdate = now;

  int baseY = 227;
  int targetY = baseY;

  if (targetNearby) {
    if (ecgX == 28 || ecgX == 34) targetY = baseY - 6;
    else if (ecgX == 31) targetY = baseY + 5;
    else targetY = baseY;
  } else {
    if (ecgX == 42) targetY = baseY - 4;
    else if (ecgX == 45) targetY = baseY + 3;
    else targetY = baseY;
  }

  int scrX = 208 + ecgX;
  tft.drawFastVLine(scrX + 1, 219, 16, COLOR_DARK_BG);
  tft.drawLine(scrX, ecgPrevY, scrX + 1, targetY, targetNearby ? COLOR_RED_BRT : COLOR_GREEN_BRT);
  ecgPrevY = targetY;

  ecgX++;
  if (ecgX >= 104) {
    ecgX = 0;
    ecgPrevY = baseY;
  }
}

// --- Audio: Authentic Geiger-Muller Mechanical Clicker ---
void handleTacticalAudio() {
  unsigned long now = millis();

  if (targetNearby && activeTargetCount > 0) {
    uint16_t closestDist = 6000;
    for (int i = 0; i < 3; i++) {
      if (targets[i].active && targets[i].distance > 50 && targets[i].distance < closestDist) {
        closestDist = targets[i].distance;
      }
    }

    // Dynamic Geiger click rate based on proximity:
    // 300mm (0.3m): ~60ms (intense Geiger chatter)
    // 1000mm (1.0m): ~180ms (rapid clicks)
    // 2500mm (2.5m): ~430ms (moderate clicks)
    // 5000mm (5.0m): ~950ms (slow clicks)
    unsigned long interval = 45 + (closestDist / 6);
    if (interval > 1050) interval = 1050;

    // Authentic Geiger randomness (Poisson jitter +/- 15%)
    int jitter = (int)(interval / 7);
    interval += random(-jitter, jitter + 1);

    if (now - lastBeepTime >= interval) {
      geigerClick();
      lastBeepTime = now;
    }
  }
  // When no target detected: completely silent (zero annoying beeps!)
}

// =====================================================================================
// EXACT DECODE FUNCTION FROM E54_Radar.h
// =====================================================================================
int16_t decodeValue(uint16_t raw) {
  if (raw == 0) return 0;
  bool isPositive = (raw & 0x8000) != 0;
  int16_t value = raw & 0x7FFF;
  if (!isPositive) {
    value = -value;
  }
  return value;
}

// =====================================================================================
// EXACT STATE MACHINE PARSER FROM E54_Radar.h
// =====================================================================================
void processRadarSerial() {
  while (Serial.available()) {
    uint8_t b = Serial.read();

    rxByteCount++;
    lastByteHex = b;

    // Check for Flash/Upload command ONLY when idle at SYNC_1
    if (radarState == SYNC_1) {
      static char cmdBuf[8];
      static uint8_t cmdIdx = 0;
      if (b >= 'A' && b <= 'Z') {
        if (cmdIdx < 6) {
          cmdBuf[cmdIdx++] = b;
          cmdBuf[cmdIdx] = '\0';
        }
        if (strcmp(cmdBuf, "FLASH") == 0 || strcmp(cmdBuf, "UPLOAD") == 0) {
          showFirmwareUploadScreen();
          while (1) { /* Halt and display upload screen */ }
        }
      } else {
        cmdIdx = 0;
      }
    }

    switch (radarState) {
      case SYNC_1:
        if (b == 0xAA) radarState = SYNC_2;
        break;
      case SYNC_2:
        if (b == 0xFF) radarState = SYNC_3;
        else if (b == 0xAA) radarState = SYNC_2;
        else radarState = SYNC_1;
        break;
      case SYNC_3:
        if (b == 0x03) radarState = SYNC_4;
        else if (b == 0xAA) radarState = SYNC_2;
        else radarState = SYNC_1;
        break;
      case SYNC_4:
        if (b == 0x00) {
          radarState = PAYLOAD;
          payloadIndex = 0;
        } else if (b == 0xAA) {
          radarState = SYNC_2;
        } else {
          radarState = SYNC_1;
        }
        break;
      case PAYLOAD:
        if (payloadIndex < sizeof(payloadBuffer)) {
          payloadBuffer[payloadIndex++] = b;
        }
        // Exactly 24 bytes payload (3 targets * 8 bytes)
        if (payloadIndex == 24) {
          radarState = TAIL_1;
        }
        break;
      case TAIL_1:
        if (b == 0x55) radarState = TAIL_2;
        else if (b == 0xAA) radarState = SYNC_2;
        else radarState = SYNC_1;
        break;
      case TAIL_2:
        if (b == 0xCC) {
          validFrameCount++;
          parsePayloadE54();
          radarState = SYNC_1;
        } else if (b == 0xAA) {
          radarState = SYNC_2;
        } else {
          radarState = SYNC_1;
        }
        break;
    }
  }
}

// Exact parsePayload() from E54_Radar.h
void parsePayloadE54() {
  unsigned long now = millis();

  for (int i = 0; i < 3; i++) {
    int offset = i * 8;
    uint16_t xRaw = (uint16_t)payloadBuffer[offset] | ((uint16_t)payloadBuffer[offset + 1] << 8);
    uint16_t yRaw = (uint16_t)payloadBuffer[offset + 2] | ((uint16_t)payloadBuffer[offset + 3] << 8);
    uint16_t sRaw = (uint16_t)payloadBuffer[offset + 4] | ((uint16_t)payloadBuffer[offset + 5] << 8);
    uint16_t rRaw = (uint16_t)payloadBuffer[offset + 6] | ((uint16_t)payloadBuffer[offset + 7] << 8);

    if (xRaw == 0 && yRaw == 0 && sRaw == 0 && rRaw == 0) {
      targets[i].active = false;
      continue;
    }

    targets[i].active = true;
    targets[i].x = decodeValue(xRaw);
    targets[i].y = decodeValue(yRaw);
    targets[i].speed = decodeValue(sRaw);
    targets[i].resolution = rRaw;
    targets[i].distance = (uint16_t)sqrt((long)targets[i].x * targets[i].x + (long)targets[i].y * targets[i].y);
    if (targets[i].distance == 0) {
      targets[i].distance = abs(targets[i].y);
    }
    targets[i].lastSeen = now;
  }
}
