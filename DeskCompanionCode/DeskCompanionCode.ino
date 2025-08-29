#include <SPI.h>
#include <U8g2lib.h>
#include <ESP32Encoder.h>
#include <Wire.h>
#include <NimBLEDevice.h>
#include <DHT.h>
#include <Adafruit_ADXL345_U.h>

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
  STATUS_OVERLAY,
  NOTIFICATION_POPUP
};
AppState currentState = IDLE;
AppState previousState = IDLE;

// === Timer Substates ===
enum TimerSubstate {
  TIMER_SELECT,    // Select timer type
  TIMER_SETUP,     // Set duration/parameters
  TIMER_RUNNING,   // Timer in progress
  TIMER_PAUSED,    // Timer paused
  TIMER_FINISHED   // Timer completed
};
TimerSubstate timerState = TIMER_SELECT;

enum TimerType {
  STOPWATCH,
  COUNTDOWN,
  POMODORO
};
TimerType selectedTimerType = STOPWATCH;

// === Encoder Pins ===
#define ENCODER1_A 32
#define ENCODER1_B 33
#define ENCODER1_BTN 25

#define ENCODER2_A 26
#define ENCODER2_B 27
#define ENCODER2_BTN 14

// === New Component Pins ===
#define DHT_PIN 4
#define BUZZER_PIN 23
#define VIBRATION_PIN 19
#define LDR_PIN 36  // ADC pin
#define ACCEL_INT1 16  // ADXL345 Interrupt 1
#define ACCEL_INT2 17  // ADXL345 Interrupt 2

// === Encoders ===
ESP32Encoder encoder1;
ESP32Encoder encoder2;
int encoder1Pos = 0;
int encoder2Pos = 0;
bool encoder1BtnPressed = false;
bool encoder2BtnPressed = false;
bool encoder1LongPress = false;
bool encoder2LongPress = false;
unsigned long encoder1BtnStart = 0;
unsigned long encoder2BtnStart = 0;
const unsigned long LONG_PRESS_TIME = 1000; // 1 second
const unsigned long debounceDelay = 200;  // debounce time (ms)

// === Battery ===
#define BATTERY 34
const float R1 = 100000.0;  // 100kΩ
const float R2 = 100000.0;  // 100kΩ
const float BATTERY_MAX = 4.2;   // Full
const float BATTERY_MIN = 3.3;   // Empty

// === PIR Sensor ===
#define PIR_PIN 35
bool userPresent = false;
unsigned long lastMotionTime = 0;
const unsigned long SLEEP_DELAY = 300000; // 5 minutes
bool isAsleep = false;

// === Sensors ===
DHT dht(DHT_PIN, DHT11);
Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);

// === Display & Animation Timing ===
unsigned long lastEyeAnim = 0;
const unsigned long eyeAnimInterval = 5000;
unsigned long lastAccelRead = 0;
const unsigned long accelReadInterval = 50;
unsigned long lastTempRead = 0;
const unsigned long tempReadInterval = 5000;

// === Status Overlay ===
unsigned long statusOverlayStart = 0;
const unsigned long STATUS_OVERLAY_DURATION = 5000;
bool showStatusOverlay = false;

// === Notification Popup ===
unsigned long notificationPopupStart = 0;
const unsigned long NOTIFICATION_POPUP_DURATION = 5000;
bool showNotificationPopup = false;
String currentNotification = "";

// === Menu System ===
const String faceNames[] = {"IDLE", "CLOCK", "MUSIC", "NOTIFS", "TIMER", "EVENTS"};
const AppState faceStates[] = {IDLE, CLOCK, MUSIC, NOTIFICATIONS, TIMER, EVENTS};
const int numFaces = 6;
int selectedFaceIndex = 0;

// === Timer Variables ===
unsigned long timerStartTime = 0;
unsigned long timerDuration = 0;  // in milliseconds
unsigned long timerElapsed = 0;
bool timerRunning = false;
int pomodoroSession = 1;
int timerMinutes = 5;  // Default timer setting

// === Music Info ===
String currentSong = "No Music";
String currentArtist = "";
bool musicPlaying = false;
int musicVolume = 50;

// === Sensor Data ===
float temperature = 0;
float humidity = 0;
float tiltX = 0, tiltY = 0;
bool shakeDetected = false;

// === Notification System ===
struct Notification {
  String app;
  String title;
  String content;
  unsigned long timestamp;
};
const int MAX_NOTIFICATIONS = 10;
Notification notifications[MAX_NOTIFICATIONS];
int notificationCount = 0;
int notificationScrollPos = 0;

// === BLE Setup ===
#define SERVICE_UUID        "fa974e39-7cab-4fa2-8681-1d71f9fb73bc"
#define CHARACTERISTIC_RX   "12e87612-7b69-4e34-a21b-d2303bfa2691"
#define CHARACTERISTIC_TX   "2b50f752-b04e-4c9f-b188-aa7d5cf93dab"
NimBLECharacteristic* txChar;
bool deviceConnected = false;

// === Eyes ===
int COLOR_WHITE = 1;
int COLOR_BLACK = 0;

class EyeManager {
public:
  struct EyeState {
    float x, y, w, h;
    int corner;
  };

  EyeState left, right;
  int spacing;

  EyeManager(int eyeW, int eyeH, int spacing, int corner) {
    this->spacing = spacing;
    left = { SCREEN_WIDTH/2.0f - eyeW/2.0f - spacing/2.0f, SCREEN_HEIGHT/2.0f, (float)eyeW, (float)eyeH, corner };
    right = { SCREEN_WIDTH/2.0f + eyeW/2.0f + spacing/2.0f, SCREEN_HEIGHT/2.0f, (float)eyeW, (float)eyeH, corner };
  }

  void display_display() { u8g2.sendBuffer(); } 
  void display_clearDisplay() { u8g2.clearBuffer(); }

  void display_fillRoundRect(int x, int y, int w, int h, int r, int color) {
    u8g2.setDrawColor(color);
    if (w < 2 * (r + 1)) {
      r = (w / 2) - 1; }
    if (h < 2 * (r + 1)) {
      r = (h / 2) - 1;
    }
    u8g2.drawRBox(x, y, w < 1 ? 1 : w, h < 1 ? 1 : h, r);
  }
  
  void display_fillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, int color) {
    u8g2.setDrawColor(color);
    u8g2.drawTriangle(x0, y0, x1, y1, x2, y2);
  }

  void reset() {
    left.w = right.w = 40;
    left.h = right.h = 40;
    left.corner = right.corner = 10;
    left.x = SCREEN_WIDTH/2.0f - left.w/2.0f - spacing/2.0f;
    right.x = SCREEN_WIDTH/2.0f + right.w/2.0f + spacing/2.0f;
    left.y = right.y = SCREEN_HEIGHT/2.0f;
    draw();
  }

  void applyTilt(float tiltX, float tiltY) {
    // Subtle eye movement based on tilt
    float maxOffset = 3.0f;
    float eyeOffsetX = constrain(tiltX * 10, -maxOffset, maxOffset);
    float eyeOffsetY = constrain(tiltY * 10, -maxOffset, maxOffset);
    
    left.x = SCREEN_WIDTH/2.0f - left.w/2.0f - spacing/2.0f + eyeOffsetX;
    right.x = SCREEN_WIDTH/2.0f + right.w/2.0f + spacing/2.0f + eyeOffsetX;
    left.y = SCREEN_HEIGHT/2.0f + eyeOffsetY;
    right.y = SCREEN_HEIGHT/2.0f + eyeOffsetY;
  }

  void draw(bool update = true) {
    display_clearDisplay();
    drawEye(left);
    drawEye(right);
    if (update) display_display();
  }

  void blink() {
    float originalH = left.h;
    for (int i=0; i<3; i++) {
      left.h -= 10; right.h -= 10;
      draw();
      delay(30);
    }
    for (int i=0; i<3; i++) {
      left.h += 10; right.h += 10;
      draw();
      delay(30);
    }
  }

  void sleep() {
    reset();
    left.h = right.h = 2;
    left.corner = right.corner = 0;
    draw();
  }

  void wakeup() {
    reset();
    for (int h=2; h<=40; h+=4) {
      left.h = right.h = h;
      left.corner = right.corner = min(h,10);
      draw();
      delay(15);
    }
  }

  void happy() {
    reset();
    display_fillTriangle(left.x-left.w/2, left.y, left.x+left.w/2, left.y, left.x, left.y+left.h/2, COLOR_BLACK);
    display_fillTriangle(right.x-right.w/2, right.y, right.x+right.w/2, right.y, right.x, right.y+right.h/2, COLOR_BLACK);
    display_display();
  }

  void sad() {
    reset();
    display_fillTriangle(left.x-left.w/2, left.y, left.x+left.w/2, left.y, left.x, left.y-left.h/2, COLOR_BLACK);
    display_fillTriangle(right.x-right.w/2, right.y, right.x+right.w/2, right.y, right.x, right.y-right.h/2, COLOR_BLACK);
    display_display();
  }

  void excited() {
    reset();
    for (int i=0; i<3; i++) {
      left.w += 6; right.w += 6;
      left.h += 6; right.h += 6;
      draw();
      delay(60);
      left.w -= 6; right.w -= 6;
      left.h -= 6; right.h -= 6;
      draw();
      delay(60);
    }
  }

private:
  void drawEye(EyeState &eye) {
    display_fillRoundRect(eye.x-eye.w/2, eye.y-eye.h/2, eye.w, eye.h, eye.corner, COLOR_WHITE);
  }
};

EyeManager eyes(40,40,10,10);

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.setContrast(255); // Full brightness initially
  
  eyes.reset();

  setupNimBLE();
  setupSensors();

  pinMode(ENCODER1_BTN, INPUT_PULLUP);
  pinMode(ENCODER2_BTN, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(VIBRATION_PIN, OUTPUT);

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
    // Optional: Setup interrupts for advanced features
    // adxl.setInterrupt(ADXL345_INT_SINGLE_TAP, true);
    // adxl.setInterrupt(ADXL345_INT_DOUBLE_TAP, true);
    Serial.println("ADXL345 initialized at I2C address 0x53");
  }
  
  // Setup interrupt pins (optional)
  pinMode(ACCEL_INT1, INPUT);
  pinMode(ACCEL_INT2, INPUT);
}

void loop() {
  checkEncoders();
  readSensors();
  handleSleepMode();
  handleOverlays();
  handleState();
  
  if (deviceConnected) {
    static unsigned long lastSent = 0;
    if (millis() - lastSent > 10000) { // Every 10 seconds
      sendStatusToBLE();
      lastSent = millis();
    }
  }
}

void checkEncoders() {
  // Read button states
  bool btn1 = !digitalRead(ENCODER1_BTN);
  bool btn2 = !digitalRead(ENCODER2_BTN);
  
  // Handle long press detection
  if (btn1 && !encoder1BtnPressed) {
    encoder1BtnStart = millis();
  } else if (!btn1 && encoder1BtnPressed) {
    if (millis() - encoder1BtnStart >= LONG_PRESS_TIME) {
      encoder1LongPress = true;
    }
  }
  if (btn2 && !encoder2BtnPressed) {
    encoder2BtnStart = millis();
  } else if (!btn2 && encoder2BtnPressed) {
    if (millis() - encoder2BtnStart >= LONG_PRESS_TIME) {
      encoder2LongPress = true;
    }
  }
  
  encoder1BtnPressed = btn1;
  encoder2BtnPressed = btn2;
  
  checkEncoder1();
  checkEncoder2();
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
    
    tiltX = event.acceleration.x / 9.8; // Normalize to g-force
    tiltY = event.acceleration.y / 9.8;
    
    // Shake detection
    float magnitude = sqrt(pow(event.acceleration.x, 2) + pow(event.acceleration.y, 2) + pow(event.acceleration.z, 2));
    
    if (magnitude > 15.0 && !shakeDetected) { // Threshold for shake
      shakeDetected = true;
      triggerStatusOverlay();
    } else if (magnitude < 12.0) {
      shakeDetected = false;
    }
    
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
  u8g2.setContrast(25); // Dim the display
  eyes.sleep();
  Serial.println("Going to sleep mode");
}

void wakeUp() {
  isAsleep = false;
  u8g2.setContrast(255); // Full brightness
  eyes.wakeup();
  Serial.println("Waking up");
}

void handleOverlays() {
  // Status overlay timeout
  if (showStatusOverlay && millis() - statusOverlayStart > STATUS_OVERLAY_DURATION) {
    showStatusOverlay = false;
    currentState = previousState;
  }
  
  // Notification popup timeout
  if (showNotificationPopup && millis() - notificationPopupStart > NOTIFICATION_POPUP_DURATION) {
    showNotificationPopup = false;
    currentState = previousState;
  }
}

void triggerStatusOverlay() {
  if (currentState != STATUS_OVERLAY) {
    previousState = currentState;
    currentState = STATUS_OVERLAY;
    showStatusOverlay = true;
    statusOverlayStart = millis();
    vibrate(100); // Short vibration feedback
  }
}

void handleState() {
  if (isAsleep && currentState != STATUS_OVERLAY && currentState != NOTIFICATION_POPUP) {
    return; // Don't update display when asleep except for overlays
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
    case STATUS_OVERLAY:
      displayStatusOverlay();
      break;
    case NOTIFICATION_POPUP:
      displayNotificationPopup();
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

void displayClockFace() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB18_tr);
  
  // Get current time (you'll need to implement RTC or NTP)
  String timeStr = "12:34";
  u8g2.drawStr(20, 30, timeStr.c_str());
  
  u8g2.setFont(u8g2_font_6x10_tf);
  String tempStr = String(temperature, 1) + "°C " + String(humidity, 0) + "%";
  u8g2.drawStr(5, 55, tempStr.c_str());
  
  u8g2.sendBuffer();
}

void displayMusicFace() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  // Song info
  u8g2.drawStr(5, 15, currentSong.substring(0, 20).c_str());
  u8g2.drawStr(5, 30, currentArtist.substring(0, 20).c_str());
  
  // Play/pause indicator
  u8g2.drawStr(5, 45, musicPlaying ? "Playing" : "Paused");
  
  // Volume bar
  int volumeWidth = map(musicVolume, 0, 100, 0, 118);
  u8g2.drawFrame(5, 50, 118, 8);
  u8g2.drawBox(5, 50, volumeWidth, 8);
  
  u8g2.sendBuffer();
}

void displayNotificationsFace() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  if (notificationCount == 0) {
    u8g2.drawStr(25, 32, "No notifications");
  } else {
    // Show notifications with scrolling
    int startIdx = notificationScrollPos;
    int y = 12;
    
    for (int i = startIdx; i < min(startIdx + 5, notificationCount); i++) {
      String notifText = notifications[i].app + ": " + notifications[i].title;
      u8g2.drawStr(2, y, notifText.substring(0, 20).c_str());
      y += 12;
    }
  }
  
  u8g2.sendBuffer();
}

void handleTimerState() {
  switch (timerState) {
    case TIMER_SELECT:
      displayTimerSelect();
      break;
    case TIMER_SETUP:
      displayTimerSetup();
      break;
    case TIMER_RUNNING:
    case TIMER_PAUSED:
      displayTimerRunning();
      updateTimer();
      break;
    case TIMER_FINISHED:
      displayTimerFinished();
      break;
  }
}

void displayTimerSelect() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(5, 15, "Select Timer Type:");
  
  String types[] = {"Stopwatch", "Countdown", "Pomodoro"};
  for (int i = 0; i < 3; i++) {
    if (i == (int)selectedTimerType) {
      u8g2.drawStr(5, 30 + i*12, (">" + types[i]).c_str());
    } else {
      u8g2.drawStr(10, 30 + i*12, types[i].c_str());
    }
  }
  
  u8g2.sendBuffer();
}

void displayTimerSetup() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(5, 15, "Set Duration:");
  String minStr = String(timerMinutes) + " minutes";
  u8g2.drawStr(5, 35, minStr.c_str());
  u8g2.drawStr(5, 50, "Click to start");
  
  u8g2.sendBuffer();
}

void displayTimerRunning() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  
  if (selectedTimerType == STOPWATCH) {
    unsigned long elapsed = timerRunning ? (millis() - timerStartTime + timerElapsed) : timerElapsed;
    String timeStr = formatTime(elapsed / 1000);
    u8g2.drawStr(10, 30, timeStr.c_str());
  } else {
    unsigned long remaining = timerDuration - (timerRunning ? (millis() - timerStartTime + timerElapsed) : timerElapsed);
    String timeStr = formatTime(remaining / 1000);
    u8g2.drawStr(10, 30, timeStr.c_str());
    
    // Progress bar
    int progress = map(timerDuration - remaining, 0, timerDuration, 0, 118);
    u8g2.drawFrame(5, 45, 118, 8);
    u8g2.drawBox(5, 45, progress, 8);
  }
  
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(5, 60, timerRunning ? "Running" : "Paused");
  
  u8g2.sendBuffer();
}

void displayTimerFinished() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB14_tr);
  u8g2.drawStr(10, 30, "FINISHED!");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(5, 50, "Click to reset");
  u8g2.sendBuffer();
  
  // Sound alarm
  static unsigned long lastBeep = 0;
  if (millis() - lastBeep > 1000) {
    playTone(1000, 200);
    vibrate(200);
    lastBeep = millis();
  }
}

void displayEventsFace() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(25, 32, "Events/TODOs");
  u8g2.drawStr(15, 45, "Coming from app...");
  u8g2.sendBuffer();
}

void displayMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(5, 12, "Select Face:");
  
  for (int i = 0; i < numFaces; i++) {
    if (i == selectedFaceIndex) {
      u8g2.drawStr(5, 25 + i*8, (">" + faceNames[i]).c_str());
    } else {
      u8g2.drawStr(10, 25 + i*8, faceNames[i].c_str());
    }
  }
  
  u8g2.sendBuffer();
}

void displayStatusOverlay() {
  // Show current face in background (dimmed)
  u8g2.setContrast(100);
  handleState(); // This will call the previous state's display function
  u8g2.setContrast(255);
  
  // Draw status bar at bottom
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 54, 128, 10);
  u8g2.setDrawColor(1);
  u8g2.drawFrame(0, 54, 128, 10);
  
  u8g2.setFont(u8g2_font_4x6_tf);
  
  // Time
  u8g2.drawStr(2, 62, "12:34");
  
  // Battery
  int batteryPercent = batteryPercentage(readBatteryVoltage());
  String battStr = String(batteryPercent) + "%";
  u8g2.drawStr(35, 62, battStr.c_str());
  
  // BLE status
  u8g2.drawStr(65, 62, deviceConnected ? "BLE" : "---");
  
  // Notification count
  if (notificationCount > 0) {
    String notifStr = "N:" + String(notificationCount);
    u8g2.drawStr(85, 62, notifStr.c_str());
  }
  
  // Show current encoder functions at top
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 10);
  u8g2.setDrawColor(1);
  u8g2.drawFrame(0, 0, 128, 10);
  
  String controls = getControlsText();
  u8g2.drawStr(2, 8, controls.c_str());
  
  u8g2.sendBuffer();
}

void displayNotificationPopup() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(5, 15, "New Notification:");
  
  // Word wrap the notification
  String lines[4];
  int lineCount = wrapText(currentNotification, 20, lines, 4);
  
  for (int i = 0; i < lineCount; i++) {
    u8g2.drawStr(5, 30 + i*12, lines[i].c_str());
  }
  
  u8g2.sendBuffer();
}

// === Encoder 1 ===
void checkEncoder1() {
  int newPos = encoder1.getCount() / 4;

  if (newPos != encoder1Pos) {
    handleEncoder1Rotation(newPos - encoder1Pos);
    encoder1Pos = newPos;
  }

  static unsigned long lastClickTime1 = 0;

  if (encoder1LongPress) {
    handleEncoder1LongPress();
    encoder1LongPress = false;
  } else if (encoder1BtnPressed && millis() - encoder1BtnStart < 50) {
    if (millis() - lastClickTime1 > debounceDelay) {
      handleEncoder1Click();
      lastClickTime1 = millis();
    }
  }
}

// === Encoder 2 ====
void checkEncoder2() {
  int newPos = encoder2.getCount() / 4;

  if (newPos != encoder2Pos) {
    handleEncoder2Rotation(newPos - encoder2Pos);
    encoder2Pos = newPos;
  }

  static unsigned long lastClickTime2 = 0;

  if (encoder2LongPress) {
    handleEncoder2LongPress();
    encoder2LongPress = false;
  } else if (encoder2BtnPressed && millis() - encoder2BtnStart < 50) {
    if (millis() - lastClickTime2 > debounceDelay) {
      handleEncoder2Click();
      lastClickTime2 = millis();
    }
  }
}

void handleEncoder1Rotation(int direction) {
  switch (currentState) {
    case MUSIC:
      // Previous/Next song
      if (direction > 0) {
        sendBLECommand("MUSIC_NEXT");
      } else {
        sendBLECommand("MUSIC_PREV");
      }
      break;
    case NOTIFICATIONS:
      notificationScrollPos = constrain(notificationScrollPos + direction, 0, max(0, notificationCount - 5));
      break;
    case EVENTS:
      // change event-group
      break;
    case TIMER:
      if (timerState == TIMER_SELECT) {
        selectedTimerType = (TimerType)constrain((int)selectedTimerType + direction, 0, 2);
      }
      break;
  }
}

void handleEncoder1Click() {
  switch (currentState) {
    case MUSIC:
      // Start seek
      break;
    case NOTIFICATIONS:
      // clear all notifications
      break;
    case TIMER:
      if (timerState == TIMER_RUNNING || timerState == TIMER_PAUSED) {
        // Pause/Resume
        timerRunning = !timerRunning;
        if (timerRunning) {
          timerStartTime = millis();
        } else {
          timerElapsed += millis() - timerStartTime;
        }
        timerState = timerRunning ? TIMER_RUNNING : TIMER_PAUSED;
      }
      break;
  }
}

void handleEncoder1LongPress() {
  switch (currentState) {
    case IDLE:
      // change face type
      break;
    case MUSIC:
      // change music layout
      break;
    case NOTIFICATIONS:
      // change grid<->linear
      break;
    case EVENTS:
      // change grid<->linear
      break;
  }
}

void handleEncoder2Rotation(int direction) {
  switch (currentState) {
    case MUSIC:
      // Volume control
      musicVolume = constrain(musicVolume + direction * 5, 0, 100);
      sendBLECommand("MUSIC_VOLUME:" + String(musicVolume));
      break;
    case EVENTS:
      // traverse current group items
      break;
    case MENU:
      selectedFaceIndex = constrain(selectedFaceIndex + direction, 0, numFaces - 1);
      break;
    case TIMER:
      if (timerState == TIMER_SETUP) {
        timerMinutes = constrain(timerMinutes + direction, 1, 120);
      }
      break;
  }
}

void handleEncoder2Click() {
  switch (currentState) {
    case MUSIC:
      // Play/Pause
      musicPlaying = !musicPlaying;
      sendBLECommand(musicPlaying ? "MUSIC_PLAY" : "MUSIC_PAUSE");
      break;
    case MENU:
      // Select face
      currentState = faceStates[selectedFaceIndex];
      break;
    case NOTIFICATIONS:
      // clear selected notification
      break;
    case EVENTS:
      // start selected event's timer
      break;
    case TIMER:
      handleTimerClick();
      break;
  }
}

void handleEncoder2LongPress() {
  // OPEN MENU =====
  if (currentState != MENU) {
    previousState = currentState;
    currentState = MENU;
    selectedFaceIndex = 0;
  }
}

void handleTimerClick() {
  switch (timerState) {
    case TIMER_SELECT:
      timerState = TIMER_SETUP;
      break;
    case TIMER_SETUP:
      startTimer();
      break;
    case TIMER_RUNNING:
    case TIMER_PAUSED:
      // Reset timer
      timerRunning = false;
      timerElapsed = 0;
      timerState = TIMER_SELECT;
      break;
    case TIMER_FINISHED:
      timerState = TIMER_SELECT;
      break;
  }
}

void startTimer() {
  timerDuration = timerMinutes * 60000UL; // Convert to milliseconds
  timerStartTime = millis();
  timerElapsed = 0;
  timerRunning = true;
  timerState = TIMER_RUNNING;
  
  if (selectedTimerType == POMODORO) {
    pomodoroSession = 1;
  }
}

void updateTimer() {
  if (!timerRunning) return;
  
  unsigned long currentTime = millis();
  unsigned long totalElapsed = timerElapsed + (currentTime - timerStartTime);
  
  if (selectedTimerType == STOPWATCH) {
    // Stopwatch runs indefinitely
    return;
  }
  
  // Countdown timer
  if (totalElapsed >= timerDuration) {
    timerRunning = false;
    timerState = TIMER_FINISHED;
    
    if (selectedTimerType == POMODORO) {
      handlePomodoroComplete();
    }
  }
}

void handlePomodoroComplete() {
  if (pomodoroSession % 4 == 0) {
    // Long break after 4 sessions
    timerMinutes = 15;
  } else {
    // Short break
    timerMinutes = 5;
  }
  pomodoroSession++;
  
  // Auto-start break timer
  delay(2000); // Show "FINISHED!" for 2 seconds
  startTimer();
}

// === Utility Functions ===
String formatTime(unsigned long seconds) {
  int mins = seconds / 60;
  int secs = seconds % 60;
  return String(mins) + ":" + (secs < 10 ? "0" : "") + String(secs);
}

String getControlsText() {
  switch (currentState) {
    case IDLE:
      return "L:- R:Menu";
    case CLOCK:
      return "L:- R:Menu";
    case MUSIC:
      return "L:Prev/Next R:Vol/Play";
    case NOTIFICATIONS:
      return "L:Scroll R:Menu";
    case TIMER:
      return "L:Start/Pause R:Setup";
    case EVENTS:
      return "L:Scroll R:Menu";
    default:
      return "L:- R:Menu";
  }
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

void addNotification(String app, String title, String content) {
  // Shift notifications if array is full
  if (notificationCount >= MAX_NOTIFICATIONS) {
    for (int i = 0; i < MAX_NOTIFICATIONS - 1; i++) {
      notifications[i] = notifications[i + 1];
    }
    notificationCount = MAX_NOTIFICATIONS - 1;
  }
  
  notifications[notificationCount] = {app, title, content, millis()};
  notificationCount++;
  
  // Trigger notification popup
  currentNotification = app + ": " + title;
  if (!showNotificationPopup && currentState != NOTIFICATION_POPUP) {
    previousState = currentState;
    currentState = NOTIFICATION_POPUP;
    showNotificationPopup = true;
    notificationPopupStart = millis();
    
    // Notification feedback
    playTone(800, 200);
    vibrate(100);
  }
}

// === Hardware Control Functions ===
void playTone(int frequency, int duration) {
  tone(BUZZER_PIN, frequency, duration);
}

void vibrate(int duration) {
  digitalWrite(VIBRATION_PIN, HIGH);
  delay(duration);
  digitalWrite(VIBRATION_PIN, LOW);
}

void sendBLECommand(String command) {
  if (deviceConnected && txChar) {
    txChar->setValue(command.c_str());
    txChar->notify();
    Serial.println("📤 Sent: " + command);
  }
}

void sendStatusToBLE() {
  if (!deviceConnected) return;
  
  String status = "STATUS:" + 
                 String(batteryPercentage(readBatteryVoltage())) + "," +
                 String(temperature) + "," +
                 String(humidity) + "," +
                 (timerRunning ? "TIMER_ON" : "TIMER_OFF");
                 
  sendBLECommand(status);
}

// === Battery Functions ===
float readBatteryVoltage() {
  int raw = analogRead(BATTERY);
  float adcVoltage = (raw / 4095.0) * 3.3;  // ESP32 ADC max 3.3V
  float batteryVoltage = adcVoltage * ((R1 + R2) / R2);
  return batteryVoltage;
}

int batteryPercentage(float voltage) {
  if (voltage > BATTERY_MAX) voltage = BATTERY_MAX;
  if (voltage < BATTERY_MIN) voltage = BATTERY_MIN;
  return (int)(((voltage - BATTERY_MIN) / (BATTERY_MAX - BATTERY_MIN)) * 100);
}

// === BLE Callbacks ===
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
    std::string value = pChar->getValue();
    String data = String(value.c_str());
    Serial.print("📩 Received: ");
    Serial.println(data);

    // Parse incoming data
    if (data.startsWith("NOTIFICATION:")) {
      // Format: NOTIFICATION:app|title|content
      String payload = data.substring(13);
      int firstPipe = payload.indexOf('|');
      int secondPipe = payload.indexOf('|', firstPipe + 1);
      
      if (firstPipe > 0 && secondPipe > firstPipe) {
        String app = payload.substring(0, firstPipe);
        String title = payload.substring(firstPipe + 1, secondPipe);
        String content = payload.substring(secondPipe + 1);
        addNotification(app, title, content);
      }
    }
    else if (data.startsWith("MUSIC:")) {
      // Format: MUSIC:song|artist|playing|volume
      String payload = data.substring(6);
      int firstPipe = payload.indexOf('|');
      int secondPipe = payload.indexOf('|', firstPipe + 1);
      int thirdPipe = payload.indexOf('|', secondPipe + 1);
      
      if (firstPipe > 0 && secondPipe > firstPipe && thirdPipe > secondPipe) {
        currentSong = payload.substring(0, firstPipe);
        currentArtist = payload.substring(firstPipe + 1, secondPipe);
        musicPlaying = payload.substring(secondPipe + 1, thirdPipe) == "true";
        musicVolume = payload.substring(thirdPipe + 1).toInt();
      }
    }
    else if (data.startsWith("TIME:")) {
      // Handle time sync if needed
      // Format: TIME:HH:MM:SS
    }
    else if (data.startsWith("EVENTS:")) {
      // Handle events/todos
      // Format: EVENTS:event1|event2|event3...
    }
  }
} rxCallbacks;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    deviceConnected = true;
    Serial.println("✅ Connected to phone");
    
    // Show connection feedback
    eyes.excited();
    playTone(1200, 100);
    delay(50);
    playTone(1500, 100);
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    deviceConnected = false;
    Serial.println("❌ Disconnected");
    NimBLEDevice::startAdvertising();
    
    // Show disconnection feedback
    eyes.sad();
    playTone(800, 200);
  }
} serverCallbacks;

void setupNimBLE() {
    // Initialize NimBLE
    NimBLEDevice::init("DeskCompanion");
    
    // Create server
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);
    
    // Create service
    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    
    // RX Characteristic (receive from phone)
    NimBLECharacteristic* rxChar = pService->createCharacteristic(
        CHARACTERISTIC_RX,
        NIMBLE_PROPERTY::WRITE | 
        NIMBLE_PROPERTY::WRITE_NR
    );
    rxChar->setCallbacks(&rxCallbacks);
    
    // TX Characteristic (send to phone)
    txChar = pService->createCharacteristic(
        CHARACTERISTIC_TX,
        NIMBLE_PROPERTY::NOTIFY
    );
    
    // Start service
    pService->start();
    
    // Start server
    pServer->start();
    
    // Advertising
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->start();
    
    Serial.println("🚀 BLE server ready - 'DeskCompanion'");
}