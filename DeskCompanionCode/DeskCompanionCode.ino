#include <SPI.h>
#include <U8g2lib.h>
#include <ESP32Encoder.h>
#include <Wire.h>
#include <NimBLEDevice.h>
#include <DHT.h>
#include <Adafruit_ADXL345_U.h>
#include <rotating_music_note_16_frames.h>

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
  NOTIFICATION_POPUP,
  SLEEP
};
AppState currentState = IDLE;
AppState previousState = IDLE;

// === Timer Substates ===
enum TimerSubstate {
  TIMER_SELECT,   // Select timer type
  TIMER_SETUP,    // Set duration/parameters
  TIMER_RUNNING,  // Timer in progress
  TIMER_PAUSED,   // Timer paused
  TIMER_FINISHED  // Timer completed
};
TimerSubstate timerState = TIMER_SELECT;

enum TimerType {
  STOPWATCH,
  COUNTDOWN,
  POMODORO
};
TimerType selectedTimerType = STOPWATCH;

// === Music Display ===
enum MusicSubstate {
  SEEK,
  VOLUME
};
MusicSubstate selectedMusicSubstate = SEEK;

// === Encoder Pins ===
#define ENCODER1_A 33
#define ENCODER1_B 25
#define ENCODER1_BTN 26

#define ENCODER2_A 27
#define ENCODER2_B 14
#define ENCODER2_BTN 12

// === Sensors ===
#define DHT_PIN 4
#define BUZZER_PIN 23
#define LDR_PIN 36     // ADC pin
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
const unsigned long LONG_PRESS_TIME = 1000;  // 1 second
const unsigned long debounceDelay = 200;     // debounce time (ms)

// === Battery ===
#define BATTERY 34
const float R1 = 100000.0;      // 100kΩ
const float R2 = 100000.0;      // 100kΩ
const float BATTERY_MAX = 4.2;  // Full
const float BATTERY_MIN = 3.3;  // Empty

// === PIR Sensor ===
#define PIR_PIN 35
bool userPresent = false;
unsigned long lastMotionTime = 0;
const unsigned long SLEEP_DELAY = 30000;  // 30 seconds
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
const String faceNames[] = { "IDLE", "CLOCK", "MUSIC", "NOTIFS", "TIMER", "EVENTS", "SLEEP" };
const AppState faceStates[] = { IDLE, CLOCK, MUSIC, NOTIFICATIONS, TIMER, EVENTS, SLEEP };
const int numFaces = 7;
int menuSelectionIndex = 0;

// === Timer Variables ===
unsigned long timerStartTime = 0;
unsigned long timerDuration = 0;  // in milliseconds
unsigned long timerElapsed = 0;
bool timerRunning = false;
int pomodoroSession = 1;
int timerMinutes = 5;  // Default timer setting

// === Music Info ===
String currentSong = "No Music";
String currentAlbum = "";
String currentArtist = "";
bool musicPlaying = false;
int musicVolume = 50;
int songDuration = 1000; // in ms
int playbackPosition = 0;

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
#define SERVICE_UUID "fa974e39-7cab-4fa2-8681-1d71f9fb73bc"
#define CHARACTERISTIC_RX "12e87612-7b69-4e34-a21b-d2303bfa2691"
#define CHARACTERISTIC_TX "2b50f752-b04e-4c9f-b188-aa7d5cf93dab"
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

  float defaultW, defaultH;
  int defaultCorner;

  EyeManager(int eyeW, int eyeH, int spacing, int corner) {
    this->spacing = spacing;
    this->defaultW = eyeW;
    this->defaultH = eyeH;
    this->defaultCorner = corner;
    left = { SCREEN_WIDTH / 2.0f - eyeW / 2.0f - spacing / 2.0f, SCREEN_HEIGHT / 2.0f, (float)eyeW, (float)eyeH, corner };
    right = { SCREEN_WIDTH / 2.0f + eyeW / 2.0f + spacing / 2.0f, SCREEN_HEIGHT / 2.0f, (float)eyeW, (float)eyeH, corner };
  }

  void display_display() {
    u8g2.sendBuffer();
  }
  void display_clearDisplay() {
    u8g2.clearBuffer();
  }

  void display_fillRoundRect(int x, int y, int w, int h, int r, int color) {
    u8g2.setDrawColor(color);
    if (w < 2 * (r + 1)) {
      r = (w / 2) - 1;
    }
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
    left.w = right.w = defaultW;
    left.h = right.h = defaultH;
    left.corner = right.corner = defaultCorner;
    left.x = SCREEN_WIDTH / 2.0f - left.w / 2.0f - spacing / 2.0f;
    right.x = SCREEN_WIDTH / 2.0f + right.w / 2.0f + spacing / 2.0f;
    left.y = right.y = SCREEN_HEIGHT / 2.0f;
    draw();
  }

  void applyTilt(float tiltX, float tiltY) {
    // Subtle eye movement based on tilt
    float maxOffset = 3.0f;
    float eyeOffsetX = constrain(tiltX * 10, -maxOffset, maxOffset);
    float eyeOffsetY = constrain(tiltY * 10, -maxOffset, maxOffset);

    left.x = SCREEN_WIDTH / 2.0f - left.w / 2.0f - spacing / 2.0f + eyeOffsetX;
    right.x = SCREEN_WIDTH / 2.0f + right.w / 2.0f + spacing / 2.0f + eyeOffsetX;
    left.y = SCREEN_HEIGHT / 2.0f + eyeOffsetY;
    right.y = SCREEN_HEIGHT / 2.0f + eyeOffsetY;
  }

  void draw(bool update = true) {
    display_clearDisplay();
    drawEye(left);
    drawEye(right);
    if (update) display_display();
  }

  void blink() {
    float originalH = left.h;
    for (int i = 0; i < 3; i++) {
      left.h -= 10;
      right.h -= 10;
      draw();
      delay(30);
    }
    for (int i = 0; i < 3; i++) {
      left.h += 10;
      right.h += 10;
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
    for (int h = 2; h <= defaultH; h += 4) {
      left.h = right.h = h;
      left.corner = right.corner = min(h, defaultCorner);
      draw();
      delay(15);
    }
  }

  void happy() {
    reset();
    display_fillTriangle(left.x - left.w / 2, left.y, left.x + left.w / 2, left.y, left.x, left.y + left.h / 2, COLOR_BLACK);
    display_fillTriangle(right.x - right.w / 2, right.y, right.x + right.w / 2, right.y, right.x, right.y + right.h / 2, COLOR_BLACK);
    display_display();
  }

  void sad() {
    reset();
    display_fillTriangle(left.x - left.w / 2, left.y, left.x + left.w / 2, left.y, left.x, left.y - left.h / 2, COLOR_BLACK);
    display_fillTriangle(right.x - right.w / 2, right.y, right.x + right.w / 2, right.y, right.x, right.y - right.h / 2, COLOR_BLACK);
    display_display();
  }

  void excited() {
    reset();
    for (int i = 0; i < 3; i++) {
      left.w += 6;
      right.w += 6;
      left.h += 6;
      right.h += 6;
      draw();
      delay(60);
      left.w -= 6;
      right.w -= 6;
      left.h -= 6;
      right.h -= 6;
      draw();
      delay(60);
    }
  }

private:
  void drawEye(EyeState& eye) {
    display_fillRoundRect(eye.x - eye.w / 2, eye.y - eye.h / 2, eye.w, eye.h, eye.corner, COLOR_WHITE);
  }
};

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
  updateSeek(); // listen for seek input

  if (deviceConnected) {
    static unsigned long lastSent = 0;
    if (millis() - lastSent > 10000) {  // Every 10 seconds
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

    tiltX = event.acceleration.x / 9.8;  // Normalize to g-force
    tiltY = event.acceleration.y / 9.8;

    // Shake detection
    float magnitude = sqrt(pow(event.acceleration.x, 2) + pow(event.acceleration.y, 2) + pow(event.acceleration.z, 2));

    if (magnitude > 15.0 && !shakeDetected) {  // Threshold for shake
      // shakeDetected = true;
      // triggerStatusOverlay();
    } else if (magnitude < 12.0) {
      shakeDetected = false;
      // shakeDetected = false;
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
  u8g2.setContrast(25);  // Dim the display
  eyes.sleep();
  Serial.println("Going to sleep mode");
}

void wakeUp() {
  isAsleep = false;
  u8g2.setContrast(255);  // Full brightness
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
  }
}

void handleState() {
  if (isAsleep && currentState != STATUS_OVERLAY && currentState != NOTIFICATION_POPUP) {
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
    case STATUS_OVERLAY:
      displayStatusOverlay();
      break;
    case NOTIFICATION_POPUP:
      displayNotificationPopup();
      break;
    case SLEEP:
      // TODO go to sleep, turn off all sensors
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
  String temp = String(temperature, 1);
  temp.remove(temp.length() - 1);
  String tempStr = temp + "°C " + String(humidity, 0) + "%";
  u8g2.drawStr(5, 55, tempStr.c_str());

  u8g2.sendBuffer();
}

// tiny volume icons (8×8)
static void drawMuteIcon(int x, int y) { // speaker + X
  // speaker box
  u8g2.drawLine(x+0, y+3, x+3, y+3);
  u8g2.drawLine(x+0, y+4, x+3, y+4);
  u8g2.drawLine(x+3, y+2, x+5, y+2);
  u8g2.drawLine(x+3, y+5, x+5, y+5);
  u8g2.drawLine(x+5, y+2, x+5, y+5);
  // X
  u8g2.drawLine(x+6, y+1, x+11, y+6);
  u8g2.drawLine(x+11, y+1, x+6, y+6);
}
static void drawMaxIcon(int x, int y) { // speaker + waves
  // speaker
  u8g2.drawLine(x+0, y+3, x+3, y+3);
  u8g2.drawLine(x+0, y+4, x+3, y+4);
  u8g2.drawLine(x+3, y+2, x+5, y+2);
  u8g2.drawLine(x+3, y+5, x+5, y+5);
  u8g2.drawLine(x+5, y+2, x+5, y+5);
  // waves
  u8g2.drawLine(x+7, y+2, x+9, y+4);
  u8g2.drawLine(x+9, y+4, x+7, y+6);
  u8g2.drawLine(x+10, y+1, x+12, y+4);
  u8g2.drawLine(x+12, y+4, x+10, y+7);
}

int scrollOffset = 0;
unsigned long lastPlaybackUpdate = 0;

// ---- main render ----
void displayMusicFace() {
  unsigned long now = millis();

  if (musicPlaying) {
    playbackPosition += now - lastPlaybackUpdate;
    if (playbackPosition > songDuration)
      playbackPosition = songDuration;
  }
  lastPlaybackUpdate = now;

  u8g2.clearBuffer();

  // === left circle ===
  const int r = 20;
  const int cx = 20, cy = 25;
  u8g2.drawDisc(cx, cy, r);

  u8g2.setDrawColor(0);
  drawRotatingMusicNote(cx - noteW / 2, cy - noteH / 2, musicPlaying);

  // === right text ===
  const int rightX = 50;
  const int rightW = SCREEN_WIDTH - rightX;

  // Enable clipping for the right area only
  u8g2.setClipWindow(rightX, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);

  // Song name with scrolling
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_ncenB10_tf);
  int songW = u8g2.getUTF8Width(currentSong.c_str());
  if (songW <= rightW) {
    u8g2.drawUTF8(rightX, 18, currentSong.c_str());
  } else {
    scrollOffset = (scrollOffset + 1) % (songW + 20);
    int x = rightX - scrollOffset;
    u8g2.drawUTF8(x, 18, currentSong.c_str());
    u8g2.drawUTF8(x + songW + 20, 18, currentSong.c_str());
  }
  
  // Disable clipping for the rest
  u8g2.setMaxClipWindow();

  // Album + artist with ellipses if too long
  u8g2.setFont(u8g2_font_6x12_tf);

  String albumStr = currentAlbum;
  int albumW = u8g2.getUTF8Width(albumStr.c_str());
  if (albumW > rightW) {
    // shrink until it fits with ".."
    while (albumStr.length() > 0 && u8g2.getUTF8Width((albumStr + "..").c_str()) > rightW) {
      albumStr.remove(albumStr.length() - 1);
    }
    albumStr += "..";
  }
  u8g2.drawUTF8(rightX, 30, albumStr.c_str());

  String artistStr = currentArtist;
  int artistW = u8g2.getUTF8Width(artistStr.c_str());
  if (artistW > rightW) {
    while (artistStr.length() > 0 && u8g2.getUTF8Width((artistStr + "..").c_str()) > rightW) {
      artistStr.remove(artistStr.length() - 1);
    }
    artistStr += "..";
  }
  u8g2.drawUTF8(rightX, 42, artistStr.c_str());

  if (selectedMusicSubstate == SEEK) {
    // === seek bar ===
    u8g2.drawFrame(17, 56, 94, 8); // width of the seek bar: 94px
    int seekBarWidth = map(playbackPosition, 0, songDuration, 0, 94);
    u8g2.drawBox(17, 56, seekBarWidth, 8);
    
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawUTF8(0, SCREEN_HEIGHT, formatTime(playbackPosition/1000).c_str());
    u8g2.drawUTF8(111, SCREEN_HEIGHT, formatTime(songDuration/1000).c_str());
  }
  else if (selectedMusicSubstate == VOLUME) {
    // === volume bar ===
    u8g2.drawFrame(17, 56, 94, 8); // width of the volume bar: 94px
    int volWidth = map(musicVolume, 0, 100, 0, 94);
    u8g2.drawBox(17, 56, volWidth, 8);

    drawMuteIcon(0, 56);
    drawMaxIcon(116, 56);
  }

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

  String types[] = { "Stopwatch", "Countdown", "Pomodoro" };
  for (int i = 0; i < 3; i++) {
    if (i == (int)selectedTimerType) {
      u8g2.drawStr(5, 30 + i * 12, (">" + types[i]).c_str());
    } else {
      u8g2.drawStr(10, 30 + i * 12, types[i].c_str());
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
  displayMenuItemIcon(menuSelectionIndex);

  String menuItemText = faceNames[menuSelectionIndex];
  int leftSpacing = (SCREEN_WIDTH - menuItemText.length() * 6) / 2;
  u8g2.drawStr(leftSpacing, SCREEN_HEIGHT - 4, menuItemText.c_str());

  u8g2.sendBuffer();
}

void displayMenuItemIcon(int index) {
  int centerX = 128 / 2;
  int centerY = 34;

  u8g2.setFontMode(1);

  switch (index) {
    case 0: // Idle - Eyes
      {
        int eyeWidth = 18;
        int eyeHeight = 26;
        int eyeSpacing = 8;
        int totalWidth = (eyeWidth * 2) + eyeSpacing;
        int startX = centerX - (totalWidth / 2);
        int startY = centerY - (eyeHeight / 2);
        
        // Draw left and right eyes as rounded rectangles
        u8g2.drawRBox(startX, startY, eyeWidth, eyeHeight, 5);
        u8g2.drawRBox(startX + eyeWidth + eyeSpacing, startY, eyeWidth, eyeHeight, 5);
      }
      break;

    case 1: // Clock
      {
        int radius = 16;
        u8g2.drawCircle(centerX, centerY, radius); // Clock face
        u8g2.drawLine(centerX, centerY, centerX, centerY - 8); // Hour hand (short)
        u8g2.drawLine(centerX, centerY, centerX + 12, centerY); // Minute hand (long)
      }
      break;

    case 2: // Music
      {
        // Draw two eighth notes (quavers) connected together
        int note1_x = centerX - 10;
        int note2_x = centerX + 10;
        int stemHeight = 22;
        int noteHeadRadius = 5;
        int beamY = centerY - stemHeight/2 - 2;

        // Note 1
        u8g2.drawDisc(note1_x, centerY + stemHeight/2, noteHeadRadius);
        u8g2.drawVLine(note1_x + noteHeadRadius, beamY, stemHeight);
        
        // Note 2
        u8g2.drawDisc(note2_x, centerY + stemHeight/2 - 2, noteHeadRadius);
        u8g2.drawVLine(note2_x + noteHeadRadius, beamY, stemHeight);

        // Connecting beam
        u8g2.drawBox(note1_x + noteHeadRadius, beamY, (note2_x - note1_x), 3);
      }
      break;

    case 3: // Notifications - Bell
      {
        int bellWidth = 24;
        int bellHeight = 20;
        int startX = centerX - bellWidth/2;
        int startY = centerY - bellHeight/2;

        // Draw the bell shape using lines
        u8g2.drawHLine(startX, startY + bellHeight, bellWidth); // Bottom rim
        u8g2.drawLine(startX, startY + bellHeight, startX + 4, startY); // Left side
        u8g2.drawLine(startX + bellWidth, startY + bellHeight, startX + bellWidth - 4, startY); // Right side
        u8g2.drawHLine(startX + 4, startY, bellWidth - 8); // Top flat part
        u8g2.drawDisc(centerX, startY + bellHeight - 2, 3); // Clapper
      }
      break;

    case 4: // Timers - Hourglass
      {
        int hgWidth = 20;
        int hgHeight = 28;
        int startX = centerX - hgWidth/2;
        int startY = centerY - hgHeight/2;

        // Frame
        u8g2.drawFrame(startX, startY, hgWidth, hgHeight);
        
        // Top part (sand falling)
        u8g2.drawTriangle(startX + 2, startY + 2, startX + hgWidth - 3, startY + 2, centerX, centerY);
        
        // Bottom part (sand collected)
        u8g2.drawTriangle(startX + 2, startY + hgHeight - 3, startX + hgWidth - 3, startY + hgHeight - 3, centerX, centerY + 1);
      }
      break;

    case 5: // Events - Calendar
      {
        int calWidth = 30;
        int calHeight = 30;
        int startX = centerX - calWidth/2;
        int startY = centerY - calHeight/2;

        // Page and binder rings
        u8g2.drawRFrame(startX, startY, calWidth, calHeight, 3);
        u8g2.drawHLine(startX, startY + 8, calWidth);
        u8g2.drawDisc(centerX - 7, startY - 2, 2);
        u8g2.drawDisc(centerX + 7, startY - 2, 2);
        
        // From your displayMenu function, the default font is u8g2_font_6x10_tf
        // We will restore this font manually since u8g2.getFont() does not exist.
        
        // Set a larger font for the date number
        u8g2.setFont(u8g2_font_ncenB10_tr);
        char dayStr[3] = "8"; // Example date
        int textWidth = u8g2.getStrWidth(dayStr);
        u8g2.drawStr(centerX - textWidth/2, startY + 23, dayStr);

        // IMPORTANT: Restore the original font
        u8g2.setFont(u8g2_font_6x10_tf);
      }
      break;

    case 6: // Go to sleep - Moon
      {
        // Create a crescent moon by drawing a white circle, then a black circle offset from it
        u8g2.setDrawColor(1); // Set color to white
        u8g2.drawDisc(centerX - 3, centerY, 16);
        u8g2.setDrawColor(0); // Set color to black (to "erase" part of the first circle)
        u8g2.drawDisc(centerX + 3, centerY, 14);
        u8g2.setDrawColor(1); // IMPORTANT: Reset draw color back to white for other elements
        
        // Add a few stars
        u8g2.drawPixel(centerX + 15, centerY - 10);
        u8g2.drawPixel(centerX + 10, centerY + 12);
        u8g2.drawPixel(centerX + 20, centerY + 5);
      }
      break;
  }
}

void displayStatusOverlay() {
  // Show current face in background (dimmed)
  u8g2.setContrast(100);
  handleState();  // This will call the previous state's display function
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
    u8g2.drawStr(5, 30 + i * 12, lines[i].c_str());
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
      // Play/Pause
      musicPlaying = !musicPlaying;
      sendBLECommand(musicPlaying ? "MUSIC_PLAY" : "MUSIC_PAUSE");
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

int seekDuration = 5000; // 5 seconds
int seekTimer = 500; // 0.5s
int relativeMillis = 0;
unsigned long lastSeek = millis();

void updateSeek() {
  if (relativeMillis != 0 && millis() - lastSeek >= seekTimer) {
    sendBLECommand("MUSIC_SEEK_RELATIVE:" + String(relativeMillis));
    relativeMillis = 0;
  }
}

void handleEncoder2Rotation(int direction) {
  switch (currentState) {
    case MUSIC:
      if (selectedMusicSubstate == VOLUME) {
       // volume controls
        musicVolume = constrain(musicVolume + direction * 5, 0, 100);
        sendBLECommand("MUSIC_VOLUME:" + String(musicVolume));
      } else if (selectedMusicSubstate == SEEK) {
        // accumulate relative seek
        playbackPosition += direction * seekDuration;
        relativeMillis += direction * seekDuration;
        lastSeek = millis(); // reset timer
      }
      break;
    case EVENTS:
      // traverse current group items
      break;
    case MENU:
      menuSelectionIndex -= direction;
      if (menuSelectionIndex < 0) menuSelectionIndex = numFaces - 1;
      if (menuSelectionIndex >= numFaces) menuSelectionIndex = 0;
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
      selectedMusicSubstate = selectedMusicSubstate == SEEK ? VOLUME : SEEK;
    case MENU:
      // Select face
      currentState = faceStates[menuSelectionIndex];
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
    menuSelectionIndex = 0;
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
  timerDuration = timerMinutes * 60000UL;  // Convert to milliseconds
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
  delay(2000);  // Show "FINISHED!" for 2 seconds
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

  notifications[notificationCount] = { app, title, content, millis() };
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
  }
}

// === Hardware Control Functions ===
void playTone(int frequency, int duration) {
  tone(BUZZER_PIN, frequency, duration);
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

  String status = "STATUS:" + String(batteryPercentage(readBatteryVoltage())) + "," + String(temperature) + "," + String(humidity) + "," + (timerRunning ? "TIMER_ON" : "TIMER_OFF");

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
    
   if (data.startsWith("MUSIC:")) {
      // Format: MUSIC:song|album|artist|playing|volume|songDuration|playbackPosition
      String payload = data.substring(6);

      int firstPipe  = payload.indexOf('|');
      int secondPipe = payload.indexOf('|', firstPipe + 1);
      int thirdPipe  = payload.indexOf('|', secondPipe + 1);
      int fourthPipe = payload.indexOf('|', thirdPipe + 1);
      int fifthPipe  = payload.indexOf('|', fourthPipe + 1);
      int sixthPipe  = payload.indexOf('|', fifthPipe + 1);

      if (firstPipe > 0 && secondPipe > firstPipe && thirdPipe > secondPipe && fourthPipe > thirdPipe && fifthPipe > fourthPipe && sixthPipe > fifthPipe) {
        currentSong      = payload.substring(0, firstPipe);
        currentAlbum     = payload.substring(firstPipe + 1, secondPipe);
        currentArtist    = payload.substring(secondPipe + 1, thirdPipe);
        musicPlaying     = payload.substring(thirdPipe + 1, fourthPipe) == "true";
        musicVolume      = payload.substring(fourthPipe + 1).toInt();
        songDuration     = payload.substring(fifthPipe + 1).toInt();
        playbackPosition = payload.substring(sixthPipe + 1).toInt();
      }
    }

    if (data.startsWith("TIME:")) {
      // Handle time sync if needed
      // Format: TIME:HH:MM:SS
    }
    
    if (data.startsWith("EVENTS:")) {
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
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rxChar->setCallbacks(&rxCallbacks);

  // TX Characteristic (send to phone)
  txChar = pService->createCharacteristic(
    CHARACTERISTIC_TX,
    NIMBLE_PROPERTY::NOTIFY);

  pService->start();
  pServer->start();

  // Advertising
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setName("DeskCompanion");
  pAdvertising->enableScanResponse(true);
  pAdvertising->start();

  Serial.println("🚀 BLE server ready - 'DeskCompanion'");
}


// BMP - 'Music Note (BG Transparent)', 25x25px
// const unsigned char musicNote [] PROGMEM = {
// 	0x00, 0x0f, 0xf8, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x0f, 0xf8, 0x00, 
// 	0x00, 0x0f, 0xf8, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x0f, 0xf8, 0x00, 0x00, 0x0f, 0x00, 0x00, 
// 	0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 
// 	0x00, 0x0f, 0x00, 0x00, 0x01, 0xff, 0x00, 0x00, 0x03, 0xff, 0x00, 0x00, 0x07, 0xff, 0x00, 0x00, 
// 	0x07, 0xff, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x00, 
// 	0x0f, 0xff, 0x00, 0x00, 0x0f, 0xfe, 0x00, 0x00, 0x07, 0xfe, 0x00, 0x00, 0x03, 0xfc, 0x00, 0x00, 
// 	0x01, 0xf8, 0x00, 0x00
// };