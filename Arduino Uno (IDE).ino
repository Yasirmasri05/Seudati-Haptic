#include "I2Cdev.h"
#include "MPU6050.h"
#include "Wire.h"
#include <LiquidCrystal_I2C.h>

// ================= KONFIGURASI PENGGUA =================
const int motorPin  = 3;
const int buttonPin = 4;
const int ledPin    = 13;

// ================= SENSITIVITAS (Threshold) =================
const long impactThreshold = 8000;
const int  armThreshold    = 10000;
const int  targetInterval  = 3000;
const int  tolerance       = 1000;

// ================= COACH SEQUENCE =================
const uint8_t PHRASES_PER_LEVEL = 1;

enum HapticType : uint8_t {
  HAPTIC_NONE = 0,
  HAPTIC_PULSE,
  HAPTIC_LONG,
  HAPTIC_DOUBLE
};

enum HapticState : uint8_t {
  HS_IDLE = 0,
  HS_ON1,
  HS_GAP,
  HS_ON2,
  HS_DONE
};

// ================= LEVEL CONFIGURATIONS =================
const uint8_t PHRASE_LEN = 8;

struct CoachLevelConfig {
  uint8_t  level;
  uint16_t bpm;
  HapticType pattern[PHRASE_LEN];
};

const CoachLevelConfig LEVEL1 = {
  1, 90,
  { HAPTIC_PULSE, HAPTIC_PULSE, HAPTIC_PULSE, HAPTIC_PULSE,
    HAPTIC_PULSE, HAPTIC_PULSE, HAPTIC_PULSE, HAPTIC_PULSE }
};

const CoachLevelConfig LEVEL2 = {
  2, 105,
  { HAPTIC_LONG,  HAPTIC_PULSE, HAPTIC_PULSE, HAPTIC_LONG,
    HAPTIC_PULSE, HAPTIC_PULSE, HAPTIC_LONG,  HAPTIC_PULSE }
};

const CoachLevelConfig LEVEL3 = {
  3, 125,
  { HAPTIC_LONG,  HAPTIC_PULSE, HAPTIC_PULSE, HAPTIC_LONG,
    HAPTIC_PULSE, HAPTIC_PULSE, HAPTIC_LONG,  HAPTIC_PULSE }
};

// ================= GLOBAL =================
uint8_t  g_level = 1;
uint16_t g_bpm = 90;
uint16_t g_beatIntervalMs = 60000UL / 90;
HapticType g_pattern[PHRASE_LEN];

uint8_t  g_seqIndex = 0;
uint8_t  g_phraseCountThisLevel = 0;
bool     g_running = true;
uint32_t g_nextBeatAt = 0;

struct {
  HapticType  type  = HAPTIC_NONE;
  HapticState state = HS_IDLE;
  uint32_t    tMark = 0;
} haptic;

// Sensor
MPU6050 mpu;
int16_t ax, ay, az;

// Latih timing
unsigned long nextBeatTime = 0;
long lastMag = 0;
unsigned long lastDetectionTime = 0;

// Mode
bool modeCoach = false;

// LED Control
unsigned long ledOnUntil = 0;
uint8_t previousLevel = 0;

// Tombol debounce (non-blocking)
bool lastButtonReading = false;
bool stableButtonState = false;
uint32_t lastDebounceAt = 0;
const uint16_t debounceMs = 50;

// ================= LCD Setup =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= LCD Coach UI (IMPROVED) =================
// Target: tidak ada output haptic yang hilang
String coachLine2 = "Haptic....";
uint32_t coachLine2Until = 0;     // pesan baris 2 ditahan sampai waktu ini
bool coachStoppedLock = false;    // jika stop, jangan berubah lagi

unsigned long lastCoachRender = 0;
const unsigned long coachRenderInterval = 200; // lebih sering biar ga miss

// Durasi tampil pesan (nyaman dibaca)
const uint16_t LEVEL_SHOW_MS  = 550;  // sebelumnya 1000 -> kepanjangan
const uint16_t HAPTIC_HOLD_MS = 450;  // latch tampilan haptic supaya ga hilang
const uint16_t START_SHOW_MS  = 450;

// ================= HELPER =================
inline void motorWrite(bool on) {
  digitalWrite(motorPin, on ? HIGH : LOW);
}

void lcdPrintLine(uint8_t row, const String &text) {
  String s = text;
  if (s.length() > 16) s = s.substring(0, 16);
  while (s.length() < 16) s += " ";
  lcd.setCursor(0, row);
  lcd.print(s);
}

void setCoachLine2(const String &msg, uint16_t holdMs) {
  if (!modeCoach) return;
  if (coachStoppedLock) return;

  coachLine2 = msg;
  coachLine2Until = millis() + holdMs;
}

void renderCoachLcd() {
  if (!modeCoach) return;
  if (millis() - lastCoachRender < coachRenderInterval) return;
  lastCoachRender = millis();

  // Baris 1 FIXED
  lcdPrintLine(0, "Mode Coach");

  // Baris 2 FIXED
  lcdPrintLine(1, coachLine2);
}

// Fungsi untuk memberikan feedback getaran (MODE LATIH)
void feedbackPattern(int type) {
  if (type == 1) {
    motorWrite(true); delay(300); motorWrite(false);
  }
  else if (type == 2) {
    motorWrite(true); delay(400); motorWrite(false); delay(200);
    motorWrite(true); delay(400); motorWrite(false);
  }
  else if (type == 3) {
    for (int i = 0; i < 3; i++) {
      motorWrite(true); delay(150); motorWrite(false); delay(100);
    }
  }
}

void loadCoachLevel(uint8_t level) {
  const CoachLevelConfig* cfg = &LEVEL1;
  if (level == 2) cfg = &LEVEL2;
  else if (level == 3) cfg = &LEVEL3;

  g_level = cfg->level;
  g_bpm = cfg->bpm;
  g_beatIntervalMs = (uint16_t)(60000UL / (uint32_t)g_bpm);

  for (uint8_t i = 0; i < PHRASE_LEN; i++) {
    g_pattern[i] = cfg->pattern[i];
  }

  g_seqIndex = 0;
  g_phraseCountThisLevel = 0;

  // LED: nyaman dibaca
  if (g_level > previousLevel) {
    ledOnUntil = millis() + 900;
    Serial.println("Level Up! LED on.");
  }
  previousLevel = g_level;

  Serial.print("Level ");
  Serial.println(g_level);
  Serial.print("BPM: ");
  Serial.println(g_bpm);

  // ===== LCD: tampil Level X sebentar, lalu baru mulai beat =====
  if (modeCoach && !coachStoppedLock) {
    setCoachLine2("Level " + String(g_level), LEVEL_SHOW_MS);

    // tunda beat pertama sampai tulisan level selesai
    g_nextBeatAt = millis() + LEVEL_SHOW_MS;
  }
}

void stopCoach() {
  g_running = false;
  motorWrite(false);
  haptic.state = HS_IDLE;
  haptic.type  = HAPTIC_NONE;
  Serial.println("Coach stopped.");

  // Kunci LCD baris 2
  coachStoppedLock = true;
  coachLine2 = "Coach Stopped";
  coachLine2Until = 0;
}

void startCoach() {
  coachStoppedLock = false;

  // Pesan awal sebentar (biar jelas)
  setCoachLine2("Coach started", START_SHOW_MS);

  loadCoachLevel(1);

  g_running = true;
  haptic.state = HS_IDLE;
  haptic.type  = HAPTIC_NONE;
  motorWrite(false);

  Serial.println("Coach started.");
}

void startHaptic(HapticType type) {
  if (type == HAPTIC_NONE) return;
  if (coachStoppedLock) return;

  haptic.type = type;
  haptic.state = HS_ON1;
  haptic.tMark = millis();
  motorWrite(true);

  // ===== LATCH tampilan haptic supaya tidak hilang =====
  if (type == HAPTIC_PULSE) {
    Serial.println("Haptic: Pulse");
    setCoachLine2("Haptic: Pulse", HAPTIC_HOLD_MS);
  }
  else if (type == HAPTIC_LONG) {
    Serial.println("Haptic: Long");
    setCoachLine2("Haptic: Long", HAPTIC_HOLD_MS);
  }
  else if (type == HAPTIC_DOUBLE) {
    Serial.println("Haptic: Double");
    setCoachLine2("Haptic: Double", HAPTIC_HOLD_MS);
  }
}

void updateHaptic() {
  const uint32_t now = millis();

  switch (haptic.state) {
    case HS_IDLE:
      break;

    case HS_ON1: {
      uint16_t dur =
        (haptic.type == HAPTIC_PULSE)  ? 120 :
        (haptic.type == HAPTIC_LONG)   ? 300 :
        (haptic.type == HAPTIC_DOUBLE) ? 120 : 0;

      if ((uint32_t)(now - haptic.tMark) >= dur) {
        motorWrite(false);
        if (haptic.type == HAPTIC_DOUBLE) {
          haptic.state = HS_GAP;
          haptic.tMark = now;
        } else {
          haptic.state = HS_DONE;
        }
      }
    } break;

    case HS_GAP:
      if ((uint32_t)(now - haptic.tMark) >= 60) {
        haptic.state = HS_ON2;
        haptic.tMark = now;
        motorWrite(true);
      }
      break;

    case HS_ON2:
      if ((uint32_t)(now - haptic.tMark) >= 120) {
        motorWrite(false);
        haptic.state = HS_DONE;
      }
      break;

    case HS_DONE:
      haptic.state = HS_IDLE;
      haptic.type  = HAPTIC_NONE;
      break;
  }
}

void coachTick() {
  if (!g_running) return;

  const uint32_t now = millis();

  if ((int32_t)(now - g_nextBeatAt) >= 0) {
    if (g_level == 3 && g_seqIndex == 0) startHaptic(HAPTIC_DOUBLE);
    else startHaptic(g_pattern[g_seqIndex]);

    g_seqIndex++;

    if (g_seqIndex >= PHRASE_LEN) {
      g_seqIndex = 0;
      g_phraseCountThisLevel++;

      if (g_phraseCountThisLevel >= PHRASES_PER_LEVEL) {
        if (g_level < 3) {
          loadCoachLevel(g_level + 1);
        } else {
          stopCoach();
          return;
        }
      }
    }

    g_nextBeatAt += (uint32_t)g_beatIntervalMs;
  }
}

// Saat masuk Latih
void startLatih() {
  stopCoach();
  motorWrite(false);
  haptic.state = HS_IDLE;
  haptic.type  = HAPTIC_NONE;

  nextBeatTime = millis() + targetInterval;

  lastMag = 0;
  lastDetectionTime = 0;

  Serial.println("Latih started.");
}

bool readButtonPressedEvent() {
  bool reading = (digitalRead(buttonPin) == LOW);
  uint32_t now = millis();

  if (reading != lastButtonReading) {
    lastDebounceAt = now;
    lastButtonReading = reading;
  }

  if ((uint32_t)(now - lastDebounceAt) > debounceMs) {
    if (stableButtonState != reading) {
      stableButtonState = reading;
      if (stableButtonState == true) return true;
    }
  }
  return false;
}

void setup() {
  Serial.begin(9600);
  pinMode(motorPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  motorWrite(false);
  digitalWrite(ledPin, LOW);

  Wire.begin();
  mpu.initialize();

  lcd.begin(16, 2);
  lcd.backlight();

  lcd.clear();
  lcdPrintLine(0, "Mode Latih");
  lcdPrintLine(1, "Ready");
  Serial.println("Mode Latih");

  startLatih();
}

void loop() {
  // Toggle mode
  if (readButtonPressedEvent()) {
    modeCoach = !modeCoach;

    ledOnUntil = millis() + 1200;
    Serial.println("Mode Changed! LED on.");

    if (modeCoach) {
      lcd.clear();
      lcdPrintLine(0, "Mode Coach");
      lcdPrintLine(1, "Coach...");
      Serial.println("Mode Coach");

      // reset state LCD coach
      coachStoppedLock = false;
      coachLine2 = "Coach...";
      coachLine2Until = 0;

      startCoach();
    } else {
      lcd.clear();
      lcdPrintLine(0, "Mode Latih");
      lcdPrintLine(1, "Ready");
      Serial.println("Mode Latih");
      startLatih();
    }
  }

  // LED
  digitalWrite(ledPin, (millis() < ledOnUntil) ? HIGH : LOW);

  // ================= MODE COACH =================
  if (modeCoach) {
    updateHaptic();
    coachTick();

    // Kalau tidak stop: setelah hold berakhir, kembali ke Haptic....
    if (!coachStoppedLock) {
      if (coachLine2Until != 0 && (int32_t)(millis() - coachLine2Until) >= 0) {
        coachLine2Until = 0;
        coachLine2 = "Haptic....";
      }
      // Jika belum pernah diset, tetap default
      if (coachLine2.length() == 0) coachLine2 = "Haptic....";
    }

    renderCoachLcd();
    return;
  }

  // ================= MODE LATIH =================
  unsigned long currentTime = millis();
  if (nextBeatTime == 0) nextBeatTime = currentTime + targetInterval;

  mpu.getAcceleration(&ax, &ay, &az);

  long currentMag = sqrt((long)ax * ax + (long)ay * ay + (long)az * az);
  long jerk = labs(currentMag - lastMag);
  lastMag = currentMag;

  if (jerk > impactThreshold && (currentTime - lastDetectionTime > 1000)) {
    long timeDiff = (long)(currentTime - nextBeatTime);

    bool isHandMovementCorrect =
      (abs(ax) > armThreshold || abs(ay) > armThreshold || abs(az) > armThreshold);

    if (abs(timeDiff) <= tolerance && isHandMovementCorrect) {
      Serial.println("SEMPURNA! (1 Getar)");
      lcd.setCursor(0, 1);
      lcd.print("SEMPURNA!      ");
      Serial.println("SEMPURNA!");
      feedbackPattern(1);
    } else {
      Serial.println("POSISI / TIMING SALAH (2 Getar)");
      lcd.setCursor(0, 1);
      lcd.print("SALAH!         ");
      Serial.println("SALAH!");
      feedbackPattern(2);
    }

    nextBeatTime = currentTime + targetInterval;
    lastDetectionTime = currentTime;
  }

  delay(10);
}
