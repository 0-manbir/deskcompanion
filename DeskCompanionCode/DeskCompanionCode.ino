#include <SPI.h>
#include <U8g2lib.h>
#include <ESP32Encoder.h>
#include <Wire.h>
#include <NimBLEDevice.h>
#include <DHT.h>
#include <Adafruit_ADXL345_U.h>

#include <ble.h>
#include <encoders.h>
#include <eye_manager.h>
#include <icons.h>
#include <rotating_music_note_16_frames.h>

#include <face_music.h>
#include <face_clock.h>
#include <face_timer.h>
#include <face_games.h>
#include <face_events.h>
#include <face_notifications.h>

// === Display ===
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
static const int SCREEN_WIDTH = 128;
static const int SCREEN_HEIGHT = 64;

// === States ===
enum AppState {
  IDLE,
  CLOCK,
  MUSIC,
  NOTIFICATIONS,
  TIMER,
  EVENTS,
  MENU,
  NOTIFICATION_POPUP,
  SLEEP,
  GAMES
};
AppState currentState = IDLE;
AppState previousState = IDLE;

// === Sensors ===
#define DHT_PIN 4
DHT dht(DHT_PIN, DHT11);
Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);
#define BUZZER_PIN 23
#define LDR_PIN 36     // ADC pin

// === PIR Sensor ===
#define PIR_PIN 35
bool userPresent = false;
unsigned long lastMotionTime = 0;
const unsigned long SLEEP_DELAY = 30000;  // 30 seconds
bool isAsleep = false;

// === Sensor Data ===
float temperature = 0;
float humidity = 0;
float tiltX = 0, tiltY = 0;

// === Notification Popup ===
unsigned long notificationPopupStart = 0;
const unsigned long NOTIFICATION_POPUP_DURATION = 5000;
bool showNotificationPopup = false;
String currentNotification = "";

// === Menu System ===
const String faceNames[] = { "IDLE", "CLOCK", "MUSIC", "NOTIFS", "TIMER", "EVENTS", "SLEEP", "GAMES" };
const AppState faceStates[] = { IDLE, CLOCK, MUSIC, NOTIFICATIONS, TIMER, EVENTS, SLEEP, GAMES };
const int numFaces = 8;
int menuSelectionIndex = 0;

EyeManager eyes(48, 48, 12, 12);

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.setContrast(255);  // Full brightness initially

  eyes.reset();

  setupNimBLE();
  setupSensors();

  pinMode(ENCODER1_BTN, INPUT_PULLUP);
  pinMode(ENCODER2_BTN, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Encoder setup
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder2.attachFullQuad(ENCODER2_A, ENCODER2_B);
  encoder2.setCount(0);
  encoder1.attachFullQuad(ENCODER1_A, ENCODER1_B);
  encoder1.setCount(0);

  lastMotionTime = millis();
}

void setupSensors() {
  dht.begin();

  if (!adxl.begin()) {
    Serial.println("ADXL345 initialization failed");
  } else {
    adxl.setRange(ADXL345_RANGE_2_G);
    Serial.println("ADXL345 initialized at I2C address 0x53");
  }
}

void loop() {
  checkEncoders();
  readSensors();
  handleSleepMode();
  handleOverlays();
  handleState();
  updateSeek();

  if (deviceConnected) {
    static unsigned long lastSent = 0;
    if (millis() - lastSent > 10000) {  // Every 10 seconds
      sendStatusToBLE();
      lastSent = millis();
    }
  }
}

void readSensors() {
  // PIR sensor
  userPresent = digitalRead(PIR_PIN);
  if (userPresent) {
    lastMotionTime = millis();
    if (isAsleep) {
      wakeUp();
    }
  }

  // Temperature and humidity
  if (millis() - lastTempRead > tempReadInterval) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    lastTempRead = millis();
  }

  // Accelerometer
  if (millis() - lastAccelRead > accelReadInterval) {
    sensors_event_t event;
    adxl.getEvent(&event);

    tiltX = event.acceleration.x / 9.8;  // Normalize to g-force
    tiltY = event.acceleration.y / 9.8;

    lastAccelRead = millis();
  }
}

void handleSleepMode() {
  if (!userPresent && millis() - lastMotionTime > SLEEP_DELAY && !isAsleep) {
    goToSleep();
  }
}

void goToSleep() {
  isAsleep = true;
  u8g2.setContrast(25);  // Dim the display
  eyes.sleep();
  Serial.println("Going to sleep mode");
  u8g2.drawXBMP(SCREEN_WIDTH - 26, 4, 24, 24, sleepFace);
}

void wakeUp() {
  isAsleep = false;
  u8g2.setContrast(255);  // Full brightness
  eyes.wakeup();
  Serial.println("Waking up");
}

void handleOverlays() {
  // Notification popup timeout
  if (showNotificationPopup && millis() - notificationPopupStart > NOTIFICATION_POPUP_DURATION) {
    showNotificationPopup = false;
    currentState = previousState;
  }
}

void handleState() {
  if (isAsleep && currentState != NOTIFICATION_POPUP) {
    return;  // Don't update display when asleep except for overlays
  }

  switch (currentState) {
    case IDLE:
      handleIdleState();
      break;
    case CLOCK:
      displayClockFace();
      break;
    case MUSIC:
      displayMusicFace();
      break;
    case NOTIFICATIONS:
      displayNotificationsFace();
      break;
    case TIMER:
      handleTimerState();
      break;
    case EVENTS:
      displayEventsFace();
      break;
    case MENU:
      displayMenu();
      break;
    case NOTIFICATION_POPUP:
      displayNotificationPopup();
      break;
    case SLEEP:
      // TODO go to sleep, turn off all sensors
      break;
    case GAMES:
      // TODO displaygames
      break;
  }
}

void handleIdleState() {
  // Apply tilt to eyes for fluid animation
  eyes.applyTilt(tiltX, tiltY);
  eyes.draw();

  // Periodic blinking
  if (millis() - lastEyeAnim > eyeAnimInterval) {
    eyes.blink();
    lastEyeAnim = millis();
  }
}

void displayMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawStr(5, 12, "Select Face:");
  displayMenuItemIcon(menuSelectionIndex);

  String menuItemText = faceNames[menuSelectionIndex];
  int leftSpacing = (SCREEN_WIDTH - menuItemText.length() * 6) / 2;
  u8g2.drawStr(leftSpacing, SCREEN_HEIGHT - 4, menuItemText.c_str());

  u8g2.sendBuffer();
}

int faceIconSize = 24;

void displayMenuItemIcon(int index) {
  int centerX = SCREEN_WIDTH / 2;
  int centerY = 34;

  u8g2.setFontMode(1);

  int startX = centerX - faceIconSize / 2;
  int startY = centerY - faceIconSize / 2;

  switch (index) {
    case 0: u8g2.drawXBM(startX, startY, faceIconSize, faceIconSize, idleFace); break;
    case 1: u8g2.drawXBM(startX, startY, faceIconSize, faceIconSize, clockFace); break;
    case 2: u8g2.drawXBM(startX, startY, faceIconSize, faceIconSize, musicFace); break;
    case 3: u8g2.drawXBM(startX, startY, faceIconSize, faceIconSize, notificationsFace); break;
    case 4: u8g2.drawXBM(startX, startY, faceIconSize, faceIconSize, timerFace); break;
    case 5: u8g2.drawXBM(startX, startY, faceIconSize, faceIconSize, eventsFace); break;
    case 6: u8g2.drawXBM(startX, startY, faceIconSize, faceIconSize, sleepFace); break;
    case 7: u8g2.drawXBM(startX, startY, faceIconSize, faceIconSize, gamesFace); break;
  }
}

void displayNotificationPopup() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.drawStr(5, 15, "New Notification:");

  // Word wrap the notification
  String lines[4];
  int lineCount = wrapText(currentNotification, 20, lines, 4);

  for (int i = 0; i < lineCount; i++) {
    u8g2.drawStr(5, 30 + i * 12, lines[i].c_str());
  }

  u8g2.sendBuffer();
}

// === Utility Functions ===
String formatTime(unsigned long seconds) {
  int mins = seconds / 60;
  int secs = seconds % 60;
  return String(mins) + ":" + (secs < 10 ? "0" : "") + String(secs);
}

int wrapText(String text, int maxChars, String* lines, int maxLines) {
  int lineCount = 0;
  int start = 0;

  while (start < text.length() && lineCount < maxLines) {
    int end = start + maxChars;
    if (end >= text.length()) {
      lines[lineCount] = text.substring(start);
      lineCount++;
      break;
    }

    // Find last space before maxChars
    int lastSpace = text.lastIndexOf(' ', end);
    if (lastSpace > start) {
      lines[lineCount] = text.substring(start, lastSpace);
      start = lastSpace + 1;
    } else {
      lines[lineCount] = text.substring(start, end);
      start = end;
    }
    lineCount++;
  }

  return lineCount;
}

// === Hardware Control Functions ===
void playTone(int frequency, int duration) {
  tone(BUZZER_PIN, frequency, duration);
}