#include <SPI.h>
#include <U8g2lib.h>
#include <ESP32Encoder.h>
#include <Wire.h>
#include <NimBLEDevice.h>
#include <DHT.h>
#include <Adafruit_ADXL345_U.h>

#include <icons.h>
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
  NOTIFICATION_POPUP,
  SLEEP,
  GAMES
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
  COUNTDOWN,
  STOPWATCH,
  POMODORO
};
TimerType selectedTimerType = COUNTDOWN;

// === Music Display ===
enum MusicSubstate {
  SEEK,
  VOLUME
};
MusicSubstate selectedMusicSubstate = SEEK;

// === Game States ===
enum GameState {
  GAME_MENU,
  GAME_PLAYING,
  GAME_PAUSED,
  GAME_OVER
};
GameState gameState = GAME_MENU;
int gameMode = 1; // 1 = single player, 2 = two player

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
const unsigned long LONG_PRESS_TIME = 500;  // 0.5s second
const unsigned long debounceDelay = 200;     // debounce time (ms)

// === PIR Sensor ===
#define PIR_PIN 35
bool userPresent = false;
unsigned long lastMotionTime = 0;
const unsigned long SLEEP_DELAY = 20000;  // 20 seconds
bool isAsleep = false;

// === Sensors ===
DHT dht(DHT_PIN, DHT11);
Adafruit_ADXL345_Unified adxl = Adafruit_ADXL345_Unified(12345);

// === Display & Animation Timing ===
unsigned long lastEyeAnim = 0;
const unsigned long eyeAnimInterval = 4000;
unsigned long lastLookAround = 0;
unsigned long lookAroundInterval = 3000;
float lookOffsetX = 0, lookOffsetY = 0;
unsigned long lastAccelRead = 0;
const unsigned long accelReadInterval = 50;
unsigned long lastTempRead = 0;
const unsigned long tempReadInterval = 5000;

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

// === Timer Variables ===
unsigned long timerStartTime = 0;
unsigned long timerDuration = 0;  // in milliseconds
unsigned long timerElapsed = 0;
bool timerRunning = false;
int pomodoroSession = 1;
int timerMinutes = 5;  // Default timer setting
const int timerMinutesPreset[] = {1, 2, 5, 10, 15, 20, 25, 30, 45, 60, 90, 120};
const int timerPresets = 12;
int timerMinutesIndex = 0;
bool pomodoroOnBreak = false;

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

// === Clock ===
int currentHour = 0, currentMinute = 0, currentSecond = 0;
unsigned long lastTimeSync = 0;   // millis of last sync
unsigned long lastTick = 0;       // millis of last second update

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

// === Events ===
#define MAX_EVENTS 10

String eventNames[MAX_EVENTS];
int eventDurations[MAX_EVENTS]; // in minutes
int numEvents = 0;
int selectedEventIndex = 0;

// === Pong Game Variables ===
struct PongGame {
  float ballX, ballY;
  float ballVelX, ballVelY;
  float leftPaddleY, rightPaddleY;
  int leftScore, rightScore;
  bool gameActive;
  unsigned long lastUpdate;
  
  // Game settings
  static const int PADDLE_HEIGHT = 20;
  static const int PADDLE_WIDTH = 3;
  static const int BALL_SIZE = 2;
  static constexpr float BALL_SPEED = 2.0;
  static constexpr float PADDLE_SPEED = 3.0;
} pongGame;

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
    for (int h = 2; h <= defaultH; h += 4) {
      left.h = right.h = h;
      left.corner = right.corner = min(h, defaultCorner);
      draw();
      delay(10);
    }
  }

  void happy() {
    reset();

    int eyeW = defaultW * 0.7;   // narrower eyes
    int eyeH = defaultH * 0.6;   // shorter eyes
    int radius = eyeW / 2;

    int eyeY = SCREEN_HEIGHT / 2 - eyeH / 2;
    int leftX = SCREEN_WIDTH / 2 - eyeW - spacing / 2;
    int rightX = SCREEN_WIDTH / 2 + spacing / 2;

    // --- Eyes with rounded top (fake half-circle using drawDisc) ---
    // Left
    display_fillRoundRect(leftX, eyeY + eyeH / 3, eyeW, eyeH * 2 / 3, 0, 1);
    u8g2.setDrawColor(1);
    u8g2.drawDisc(leftX + eyeW / 2, eyeY + eyeH / 3, radius);
    // Mask bottom half (erase it)
    u8g2.setDrawColor(0);
    u8g2.drawBox(leftX, eyeY + eyeH / 3, eyeW, eyeH * 2 / 3);
    u8g2.setDrawColor(1);

    // Right
    display_fillRoundRect(rightX, eyeY + eyeH / 3, eyeW, eyeH * 2 / 3, 0, 1);
    u8g2.drawDisc(rightX + eyeW / 2, eyeY + eyeH / 3, radius);
    u8g2.setDrawColor(0);
    u8g2.drawBox(rightX, eyeY + eyeH / 3, eyeW, eyeH * 2 / 3);
    u8g2.setDrawColor(1);

    // --- Smile arc ---
    int smileX = SCREEN_WIDTH / 2;
    int smileY = SCREEN_HEIGHT / 2 + eyeH;
    int smileR = 12;
    u8g2.drawArc(smileX, smileY, smileR, 200, 340); // valid call: x,y,r,start,end

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

EyeManager eyes(46, 46, 12, 10); // width, height, spacing, corner

// Threshold for detecting pickup movement
float pickupThreshold = 1.2;

void checkPickup() {
  sensors_event_t event;
  adxl.getEvent(&event);

  // Calculate magnitude of acceleration vector (ignoring gravity offset ~9.8)
  float ax = event.acceleration.x / 9.8;
  float ay = event.acceleration.y / 9.8;
  float az = event.acceleration.z / 9.8;

  float magnitude = sqrt(ax * ax + ay * ay + az * az);

  if (magnitude > pickupThreshold) {
    eyes.happy();
    delay(1500);
    eyes.reset();
  }
}

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
  
  // Initialize Pong game
  initializePongGame();
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
  updateClock();
  handleState();

  if (currentState != SLEEP) {
    // checkPickup();
    readSensors();
    handleSleepMode();
    handleOverlays();
    updateSeek(); // listen for seek input
  }

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

  // === Encoder 1 ===
  if (btn1 && !encoder1BtnPressed) {
    // Button just pressed
    encoder1BtnStart = millis();
    encoder1LongPress = false;
  } else if (btn1 && encoder1BtnPressed && !encoder1LongPress) {
    // Still holding -> check for long press
    if (millis() - encoder1BtnStart >= LONG_PRESS_TIME) {
      encoder1LongPress = true;  // Trigger once
    }
  }

  // === Encoder 2 ===
  if (btn2 && !encoder2BtnPressed) {
    encoder2BtnStart = millis();
    encoder2LongPress = false;
  } else if (btn2 && encoder2BtnPressed && !encoder2LongPress) {
    if (millis() - encoder2BtnStart >= LONG_PRESS_TIME) {
      encoder2LongPress = true;
    }
  }

  // Update states
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
    if (isAsleep) wakeUp();
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

    tiltX = event.acceleration.x / 9.8;   // forward/back (Y is downward arrow → invert for chest mount)
    tiltY = event.acceleration.z / 9.8;   // left/right tilt (X arrow is left)

    lastAccelRead = millis();
  }
}

void updateClock() {
  // Tick every second
  if (millis() - lastTick >= 1000) {
    lastTick += 1000;
    currentSecond++;
    if (currentSecond >= 60) {
      currentSecond = 0;
      currentMinute++;
      if (currentMinute >= 60) {
        currentMinute = 0;
        currentHour++;
        if (currentHour >= 24) currentHour = 0;
      }
    }
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
      goToSleep();
      break;
    case GAMES:
      handleGameState();
      break;
  }
}

void handleIdleState() {
  // Apply tilt to eyes for fluid animation
  eyes.applyTilt(tiltX + lookOffsetX, tiltY + lookOffsetY);
  eyes.draw();

  // Periodic blinking
  if (millis() - lastEyeAnim > eyeAnimInterval) {
    eyes.blink();
    lastEyeAnim = millis();
  }

  // Random look-around
  // if (millis() - lastLookAround > lookAroundInterval) {
  //   lookOffsetX = random(-40, 41) / 10.0f;  // -2.0 to +2.0
  //   lookOffsetY = random(-40, 41) / 10.0f;

  //   lookAroundInterval = random(3000, 8000);  // next glance in 3–8s
  //   lastLookAround = millis();
  // }
}

void displayClockFace() {
  u8g2.clearBuffer();

  int displayHour = currentHour % 12;
  if (displayHour == 0) displayHour = 12;
  bool isPM = (currentHour >= 12);

  char timeStr[6];
  if (millis() / 500 % 2 == 0) {
    sprintf(timeStr, "%02d:%02d", displayHour, currentMinute);
  } else {
    sprintf(timeStr, "%02d %02d", displayHour, currentMinute); // replace colon with space
  }

  u8g2.setFont(u8g2_font_courB24_tn);  // monospaced digits + colon
  int timeWidth = u8g2.getStrWidth(timeStr);
  int timeX = (128 - timeWidth) / 2;
  int timeY = 34;
  u8g2.drawStr(timeX, timeY, timeStr);

  u8g2.setFont(u8g2_font_6x12_tf);
  const char* ampmStr = isPM ? "PM" : "AM";
  int ampmX = timeX + timeWidth + 2;
  int ampmY = timeY;
  u8g2.drawStr(ampmX, ampmY, ampmStr);

  u8g2.drawLine(10, 40, 118, 40);

  u8g2.setFont(u8g2_font_7x13_tf);
  char infoStr[20];
  sprintf(infoStr, "%.1f%sC  %d%%", temperature, "\xB0", (int)humidity);
  int infoWidth = u8g2.getStrWidth(infoStr);
  u8g2.drawStr((128 - infoWidth) / 2, 60, infoStr);

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
    u8g2.drawFrame(20, 56, 88, 8); // width of the seek bar: 88px
    int seekBarWidth = map(playbackPosition, 0, songDuration, 0, 88);
    u8g2.drawBox(20, 56, seekBarWidth, 8);
    
    u8g2.setFont(u8g2_font_4x6_tf);
    u8g2.drawUTF8(0, SCREEN_HEIGHT, formatTime(playbackPosition/1000).c_str());
    u8g2.drawUTF8(111, SCREEN_HEIGHT, formatTime(songDuration/1000).c_str());
  }
  else if (selectedMusicSubstate == VOLUME) {
    // === volume bar ===
    u8g2.drawFrame(20, 56, 88, 8); // width of the volume bar: 88px
    int volWidth = map(musicVolume, 0, 100, 0, 88);
    u8g2.drawBox(20, 56, volWidth, 8);

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
    case TIMER_SELECT: displayTimerSelect(); break;
    case TIMER_SETUP: displayTimerSetup(); break;
    case TIMER_RUNNING:
    case TIMER_PAUSED: displayTimerRunning(); updateTimer(); break;
    case TIMER_FINISHED: displayTimerFinished(); break;
  }
}

void displayTimerSelect() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.drawStr(15, 15, "Select Timer:");

  const char* types[] = { "Countdown", "Stopwatch", "Pomodoro" };
  for (int i = 0; i < 3; i++) {
    if (i == (int)selectedTimerType) u8g2.drawStr(10, 32 + i * 14, ("> " + String(types[i])).c_str());
    else u8g2.drawStr(20, 32 + i * 14, types[i]);
  }

  u8g2.sendBuffer();
}

// === Timer Setup Screen ===
void displayTimerSetup() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.drawStr(15, 15, "Set Duration:");

  u8g2.setFont(u8g2_font_logisoso18_tr);
  String minStr = String(timerMinutes) + " min";
  int w = u8g2.getStrWidth(minStr.c_str());
  u8g2.drawStr((128 - w) / 2, 46, minStr.c_str());

  u8g2.sendBuffer();
}

// === Timer Running Screen ===
void displayTimerRunning() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_logisoso24_tr);
  unsigned long value;

  if (selectedTimerType == STOPWATCH)
    value = timerRunning ? (millis() - timerStartTime + timerElapsed) : timerElapsed;
  else
    value = timerDuration - (timerRunning ? (millis() - timerStartTime + timerElapsed) : timerElapsed);

  String timeStr = formatTime(value / 1000);
  int w = u8g2.getStrWidth(timeStr.c_str());
  u8g2.drawStr((128 - w) / 2, 30, timeStr.c_str());

  // Progress bar (for countdown/pomodoro only)
  if (selectedTimerType != STOPWATCH) {
    unsigned long remaining = timerDuration - (timerRunning ? (millis() - timerStartTime + timerElapsed) : timerElapsed);
    int progress = map(timerDuration - remaining, 0, timerDuration, 0, 118);

    int barX = 5, barY = 36, barW = 118, barH = 12;
    int fillH = barH - 3;   // reduce height by ~3 px
    int offsetY = (barH - fillH) / 2;

    u8g2.drawFrame(barX, barY, barW, barH); // Frame
    u8g2.drawBox(barX, barY + offsetY, progress, fillH); // Fill

    // Rounded ends
    u8g2.drawDisc(barX, barY + barH/2, fillH/2, U8G2_DRAW_ALL);
    u8g2.drawDisc(barX + progress, barY + barH/2, fillH/2, U8G2_DRAW_ALL);
  }

  if (timerRunning) {
    u8g2.drawBox(108, 55, 3, 10);
    u8g2.drawBox(114, 55, 3, 10);
  } else u8g2.drawTriangle(110, 55, 110, 65, 118, 60);

  // Pomodoro mode
  if (selectedTimerType == POMODORO) {
    u8g2.setFont(u8g2_font_6x10_tf);
    const char* modeStr = pomodoroOnBreak ? "BREAK" : "WORK";
    int mw = u8g2.getStrWidth(modeStr);
    u8g2.drawStr((128 - mw) / 2, 62, modeStr);
  }

  u8g2.sendBuffer();
}

// === Timer Finished Screen ===
void displayTimerFinished() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_logisoso18_tr);
  int w = u8g2.getStrWidth("FINISHED!");
  u8g2.drawStr((128 - w) / 2, 30, "FINISHED!");

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(25, 55, "Click knob to reset");

  u8g2.sendBuffer();

  // alarm beep
  static unsigned long lastBeep = 0;
  if (millis() - lastBeep > 1000) {
    playTone(1000, 200);
    lastBeep = millis();
  }
}

void displayEventsFace() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  if (numEvents == 0) {
    u8g2.drawStr(35, 32, "No events!");
  } else {
    int rowHeight = 12;
    int yStart = 20;
    int timeColWidth = 25;  // space reserved for "XXXm"
    int nameStartX = timeColWidth + 8;

    for (int i = 0; i < numEvents; i++) {
      int y = yStart + i * rowHeight;

      // Highlight bar if selected
      if (i == selectedEventIndex) {
        u8g2.drawBox(0, y - rowHeight + 3, SCREEN_WIDTH, rowHeight);
        u8g2.setDrawColor(0);
      }

      // Duration (right aligned in time column)
      char timeBuf[8];
      sprintf(timeBuf, "%dm", eventDurations[i]);
      int timeWidth = u8g2.getStrWidth(timeBuf);
      u8g2.drawStr(timeColWidth - timeWidth, y, timeBuf);

      // Event name
      u8g2.drawStr(nameStartX, y, eventNames[i].c_str());

      // Reset draw color
      if (i == selectedEventIndex) {
        u8g2.setDrawColor(1);
      }
    }
  }

  u8g2.sendBuffer();
}

void displayMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);

  int unselectedRad = 1, selectedRad = 2, discSpace = 5;
  int xPos = (128 - numFaces * (unselectedRad*2 + discSpace)) / 2 + discSpace, yPos = 4;

  for (int i = 0; i < numFaces; i++)
    u8g2.drawDisc(xPos + (i * (unselectedRad*2 + discSpace)), yPos, i == menuSelectionIndex ? selectedRad : unselectedRad);

  displayMenuItemIcon(menuSelectionIndex);

  String menuItemText = faceNames[menuSelectionIndex];
  int leftSpacing = (SCREEN_WIDTH - menuItemText.length() * 6) / 2;
  u8g2.drawStr(leftSpacing, SCREEN_HEIGHT - 4, menuItemText.c_str());

  int leftArrowX = 20;
  int arrowY = (SCREEN_HEIGHT / 2) + 5;
  u8g2.drawTriangle(leftArrowX, arrowY, leftArrowX + 6, arrowY - 5, leftArrowX + 6, arrowY + 5);

  int rightArrowX = SCREEN_WIDTH - 20;
  u8g2.drawTriangle(rightArrowX, arrowY, rightArrowX - 6, arrowY - 5, rightArrowX - 6, arrowY + 5);

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
    case 3: u8g2.drawXBM(startX, startY, faceIconSize, faceIconSize, notifFace); break;
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

// === PONG GAME IMPLEMENTATION ===
void initializePongGame() {
  pongGame.ballX = SCREEN_WIDTH / 2.0;
  pongGame.ballY = SCREEN_HEIGHT / 2.0;
  pongGame.ballVelX = PongGame::BALL_SPEED;
  pongGame.ballVelY = PongGame::BALL_SPEED;
  pongGame.leftPaddleY = SCREEN_HEIGHT / 2.0;
  pongGame.rightPaddleY = SCREEN_HEIGHT / 2.0;
  pongGame.leftScore = 0;
  pongGame.rightScore = 0;
  pongGame.gameActive = false;
  pongGame.lastUpdate = millis();
}

void handleGameState() {
  switch (gameState) {
    case GAME_MENU:
      displayGameMenu();
      break;
    case GAME_PLAYING:
      updatePongGame();
      displayPongGame();
      break;
    case GAME_PAUSED:
      displayPongPaused();
      break;
    case GAME_OVER:
      displayPongGameOver();
      break;
  }
}

void displayGameMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(45, 15, "PONG");
  
  u8g2.drawStr(20, 35, gameMode == 1 ? ">1 Player" : " 1 Player");
  u8g2.drawStr(20, 47, gameMode == 2 ? ">2 Player" : " 2 Player");
  
  u8g2.drawStr(15, 60, "Rotate: Select, Click: Start");
  
  u8g2.sendBuffer();
}

void displayPongGame() {
  u8g2.clearBuffer();
  
  // Draw paddles
  u8g2.drawBox(2, pongGame.leftPaddleY - PongGame::PADDLE_HEIGHT/2, 
               PongGame::PADDLE_WIDTH, PongGame::PADDLE_HEIGHT);
  u8g2.drawBox(SCREEN_WIDTH - 2 - PongGame::PADDLE_WIDTH, 
               pongGame.rightPaddleY - PongGame::PADDLE_HEIGHT/2, 
               PongGame::PADDLE_WIDTH, PongGame::PADDLE_HEIGHT);
  
  // Draw ball
  u8g2.drawBox(pongGame.ballX - PongGame::BALL_SIZE/2, 
               pongGame.ballY - PongGame::BALL_SIZE/2, 
               PongGame::BALL_SIZE, PongGame::BALL_SIZE);
  
  // Draw center line
  for (int y = 0; y < SCREEN_HEIGHT; y += 4) {
    u8g2.drawPixel(SCREEN_WIDTH/2, y);
  }
  
  // Draw scores
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(SCREEN_WIDTH/2 - 20, 12, String(pongGame.leftScore).c_str());
  u8g2.drawStr(SCREEN_WIDTH/2 + 15, 12, String(pongGame.rightScore).c_str());
  
  u8g2.sendBuffer();
}

void displayPongPaused() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(40, 30, "PAUSED");
  u8g2.drawStr(20, 45, "Click to resume");
  u8g2.sendBuffer();
}

void displayPongGameOver() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(30, 20, "GAME OVER");
  
  String winner = pongGame.leftScore > pongGame.rightScore ? "Left Player Won" : "Right Player Won";
  if (gameMode == 1) {
    winner = pongGame.leftScore > pongGame.rightScore ? "You Win!" : "You Lose!";
  }
  
  u8g2.drawStr(15, 35, winner.c_str());
  u8g2.drawStr(10, 50, ("Score: " + String(pongGame.leftScore) + " - " + String(pongGame.rightScore)).c_str());
  u8g2.drawStr(20, 62, "Click to restart");
  
  u8g2.sendBuffer();
}

void updatePongGame() {
  if (!pongGame.gameActive) return;
  
  unsigned long currentTime = millis();
  float deltaTime = (currentTime - pongGame.lastUpdate) / 1000.0;
  pongGame.lastUpdate = currentTime;
  
  // Update ball position
  pongGame.ballX += pongGame.ballVelX;
  pongGame.ballY += pongGame.ballVelY;
  
  // Ball collision with top/bottom walls
  if (pongGame.ballY <= PongGame::BALL_SIZE/2 || pongGame.ballY >= SCREEN_HEIGHT - PongGame::BALL_SIZE/2) {
    pongGame.ballVelY = -pongGame.ballVelY;
  }
  
  // Ball collision with left paddle
  if (pongGame.ballX <= 2 + PongGame::PADDLE_WIDTH + PongGame::BALL_SIZE/2 &&
      pongGame.ballY >= pongGame.leftPaddleY - PongGame::PADDLE_HEIGHT/2 &&
      pongGame.ballY <= pongGame.leftPaddleY + PongGame::PADDLE_HEIGHT/2 &&
      pongGame.ballVelX < 0) {
    bounceOffPaddle(pongGame.leftPaddleY);
    playTone(800, 50);
  }
  
  // Ball collision with right paddle
  if (pongGame.ballX >= SCREEN_WIDTH - 2 - PongGame::PADDLE_WIDTH - PongGame::BALL_SIZE/2 &&
      pongGame.ballY >= pongGame.rightPaddleY - PongGame::PADDLE_HEIGHT/2 &&
      pongGame.ballY <= pongGame.rightPaddleY + PongGame::PADDLE_HEIGHT/2 &&
      pongGame.ballVelX > 0) {
    bounceOffPaddle(pongGame.rightPaddleY);
    playTone(800, 50);
  }
  
  // AI for single player mode (right paddle)
  if (gameMode == 1) {
    float aiSpeed = PongGame::PADDLE_SPEED * 0.7; // Make AI slightly slower
    if (pongGame.ballY < pongGame.rightPaddleY - 5) {
      pongGame.rightPaddleY -= aiSpeed;
    } else if (pongGame.ballY > pongGame.rightPaddleY + 5) {
      pongGame.rightPaddleY += aiSpeed;
    }
    
    // Keep AI paddle in bounds
    pongGame.rightPaddleY = constrain(pongGame.rightPaddleY, 
                                     PongGame::PADDLE_HEIGHT/2, 
                                     SCREEN_HEIGHT - PongGame::PADDLE_HEIGHT/2);
  }
  
  // Score detection
  if (pongGame.ballX < 0) {
    pongGame.rightScore++;
    resetBall();
    playTone(400, 200);
    
    if (pongGame.rightScore >= 3) {
      gameState = GAME_OVER;
    }
  } else if (pongGame.ballX > SCREEN_WIDTH) {
    pongGame.leftScore++;
    resetBall();
    playTone(600, 200);
    
    if (pongGame.leftScore >= 3) {
      gameState = GAME_OVER;
    }
  }
}

// Adjust bounce angle + increase speed over time
void bounceOffPaddle(float paddleY) {
  // relative hit position: -1.0 (top) → 0 (center) → +1.0 (bottom)
  float relativeY = (pongGame.ballY - paddleY) / (PongGame::PADDLE_HEIGHT / 2.0);

  // max bounce angle ~ 60 degrees
  float maxAngle = PI / 3; 
  float bounceAngle = relativeY * maxAngle;

  // increase speed gradually (up to a limit)
  static float currentSpeed = PongGame::BALL_SPEED;
  currentSpeed *= 1.05;                     // +5% each hit
  if (currentSpeed > 6.0) currentSpeed = 6.0;  // cap speed

  // flip X depending on which side
  float dir = (pongGame.ballVelX > 0) ? -1 : 1;

  pongGame.ballVelX = dir * currentSpeed * cos(bounceAngle);
  pongGame.ballVelY = currentSpeed * sin(bounceAngle);

  // safeguard: never perfectly flat
  if (fabs(pongGame.ballVelY) < 0.1) {
    pongGame.ballVelY = (random(0, 2) == 0 ? 1 : -1) * 0.5;
  }
}

void resetBall() {
  pongGame.ballX = SCREEN_WIDTH / 2;
  pongGame.ballY = SCREEN_HEIGHT / 2;

  float angle = random(-45, 45) * PI / 180.0; // random small angle
  float dir = (random(0, 2) == 0 ? -1 : 1);

  // reset speed back to base
  bounceOffPaddle(pongGame.leftPaddleY); // just to use formula
  pongGame.ballVelX = dir * PongGame::BALL_SPEED * cos(angle);
  pongGame.ballVelY = PongGame::BALL_SPEED * sin(angle);
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
    case GAMES:
      if (gameState == GAME_MENU) {
        gameMode = constrain(gameMode + direction, 1, 2);
      } else if (gameState == GAME_PLAYING && pongGame.gameActive) {
        // Move left paddle
        pongGame.leftPaddleY -= direction * PongGame::PADDLE_SPEED;
        pongGame.leftPaddleY = constrain(pongGame.leftPaddleY, PongGame::PADDLE_HEIGHT/2, SCREEN_HEIGHT - PongGame::PADDLE_HEIGHT/2);
      }
      break;
  }
}

void handleEncoder1Click() {
  if (isAsleep) {
    wakeUp();
    previousState = SLEEP;
    currentState = IDLE;
  }
  switch (currentState) {
    case MUSIC:
      // Play/Pause
      musicPlaying = !musicPlaying;
      sendBLECommand(musicPlaying ? "MUSIC_PLAY" : "MUSIC_PAUSE");
      break;
    case NOTIFICATIONS:
    case EVENTS:
      previousState = currentState;
      currentState = MENU;
      break;
    case TIMER:
      if (timerState == TIMER_RUNNING || timerState == TIMER_PAUSED) { // stop timer
        timerRunning = false;
        timerElapsed = 0;
        timerState = TIMER_SELECT;
      }
      if (timerState == TIMER_SETUP) timerState = TIMER_SELECT; // go back to timer selection menu
      if (timerState == TIMER_SELECT) {
        previousState = currentState;
        currentState = MENU;
      }
      break;
    case GAMES:
      if (gameState == GAME_MENU) {
        // Start game
        initializePongGame();
        pongGame.gameActive = true;
        gameState = GAME_PLAYING;
      } else if (gameState == GAME_PLAYING) {
        // Pause game
        gameState = GAME_PAUSED;
        pongGame.gameActive = false;
      } else if (gameState == GAME_PAUSED) {
        // Resume game
        gameState = GAME_PLAYING;
        pongGame.gameActive = true;
        pongGame.lastUpdate = millis();
      } else if (gameState == GAME_OVER) {
        // Restart game
        gameState = GAME_MENU;
      }
      break;
  }
}

void handleEncoder1LongPress() {
  switch (currentState) {
    case GAMES:
      // Exit to menu
      gameState = GAME_MENU;
      pongGame.gameActive = false;
      break;
  }
}

int seekDuration = 5000; // 5 seconds
int seekTimer = 500; // 0.5s
int relativeMillis = 0;
unsigned long lastSeek = millis();

void updateSeek() {
  unsigned long now = millis();

  if (musicPlaying) {
    playbackPosition += now - lastPlaybackUpdate;
    if (playbackPosition > songDuration)
      playbackPosition = songDuration;
  }
  lastPlaybackUpdate = now;

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
        playbackPosition = constrain(playbackPosition + direction * seekDuration, 0, songDuration);
        relativeMillis += direction * seekDuration;
        lastSeek = millis(); // reset timer
      }
      break;
    case EVENTS:
      if (numEvents == 0) return;
      selectedEventIndex = (selectedEventIndex + direction + numEvents) % numEvents;
      break;
    case MENU:
      menuSelectionIndex -= direction;
      if (menuSelectionIndex < 0) menuSelectionIndex = numFaces - 1;
      if (menuSelectionIndex >= numFaces) menuSelectionIndex = 0;
      break;
    case TIMER:
      if (timerState == TIMER_SELECT) selectedTimerType = (TimerType)constrain((int)selectedTimerType - direction, 0, 2);

      if (timerState == TIMER_SETUP) {
        timerMinutesIndex = constrain(timerMinutesIndex - direction, 0, timerPresets - 1);
        timerMinutes = timerMinutesPreset[timerMinutesIndex];
      }
      break;
    case GAMES:
      if (gameState == GAME_PLAYING && pongGame.gameActive && gameMode == 2) {
        // Move right paddle (only in 2-player mode)
        pongGame.rightPaddleY -= direction * PongGame::PADDLE_SPEED;
        pongGame.rightPaddleY = constrain(pongGame.rightPaddleY, PongGame::PADDLE_HEIGHT/2, SCREEN_HEIGHT - PongGame::PADDLE_HEIGHT/2);
      }
      break;
  }
}

void handleEncoder2Click() {
  if (isAsleep) {
    wakeUp();
    previousState = SLEEP;
    currentState = IDLE;
  }
  switch (currentState) {
    case IDLE:
      eyes.happy();
      delay(2000);
      eyes.reset();
      break;
    case MUSIC:
      selectedMusicSubstate = selectedMusicSubstate == SEEK ? VOLUME : SEEK;
      break;
    case MENU:
      // Select face
      currentState = faceStates[menuSelectionIndex];
      break;
    case NOTIFICATIONS:
      // clear selected notification
      break;
    case EVENTS:
    {
      if (numEvents == 0) return;

      // Start timer for this event
      timerMinutes = eventDurations[selectedEventIndex];
      timerDuration = timerMinutes * 60000UL;
      timerElapsed = 0;
      startTimer();
      previousState = currentState;
      currentState = TIMER;

      // Notify phone app
      String msg = "EVENT_COMPLETE:" + eventNames[selectedEventIndex];
      sendBLECommand(msg.c_str());

      // Remove the event from the list
      for (int i = selectedEventIndex; i < numEvents - 1; i++) {
        eventNames[i] = eventNames[i + 1];
        eventDurations[i] = eventDurations[i + 1];
      }
      numEvents--;

      // Keep selection valid
      if (selectedEventIndex >= numEvents) {
        selectedEventIndex = max(0, numEvents - 1);
      }
      break;
    }
    case TIMER:
      handleTimerClick();
      break;
    case GAMES:
      handleEncoder1Click();
      break;
  }
}

void handleEncoder2LongPress() {
  // OPEN MENU =====
  if (currentState != MENU) {
    previousState = currentState;
    currentState = MENU;
  }
}

void handleTimerClick() {
  switch (timerState) {
    case TIMER_RUNNING:
    case TIMER_PAUSED:
      timerRunning = !timerRunning;
      if (timerRunning) {
        timerStartTime = millis();
      } else {
        timerElapsed += millis() - timerStartTime;
      }
      timerState = timerRunning ? TIMER_RUNNING : TIMER_PAUSED;
      break;
    case TIMER_SELECT:
      timerState = TIMER_SETUP;
      break;
    case TIMER_SETUP:
      startTimer();
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

  if (selectedTimerType == STOPWATCH) return; // Stopwatch runs indefinitely

  // Countdown timer
  if (totalElapsed >= timerDuration) {
    timerRunning = false;
    timerState = TIMER_FINISHED;

    if (selectedTimerType == POMODORO)
      handlePomodoroComplete();
  }
}

void handlePomodoroComplete() {
  if (!pomodoroOnBreak) {
    // Finished a work session → go into break
    pomodoroSession++;
    pomodoroOnBreak = true;
    if (pomodoroSession % 4 == 0) timerMinutes = 15; // long break after 4 sessions
    else timerMinutes = 5; // short break
  } else {
    // Finished a break → go back to work
    timerMinutes = 25;    // default work session
    pomodoroOnBreak = false;
  }

  // Auto-start next timer
  delay(2000); // Show "FINISHED!" screen for 2s
  startTimer();
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

void parseEvents(String data) {
  numEvents = 0;
  selectedEventIndex = 0;

  // Strip prefix
  data.remove(0, 7); // remove "EVENTS:"

  int start = 0;
  while (numEvents < MAX_EVENTS) {
    int sep = data.indexOf('|', start);
    String token = (sep == -1) ? data.substring(start) : data.substring(start, sep);
    if (token.length() == 0) break;

    int parenOpen = token.indexOf('(');
    int parenClose = token.indexOf(')');
    if (parenOpen != -1 && parenClose != -1) {
      eventNames[numEvents] = token.substring(0, parenOpen);
      String durationStr = token.substring(parenOpen + 1, parenClose);
      durationStr.replace("m", "");
      eventDurations[numEvents] = durationStr.toInt();
      numEvents++;
    }

    if (sep == -1) break;
    start = sep + 1;
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

  // String status = "STATUS:" + String(temperature) + "," + String(humidity) + "," + (timerRunning ? "TIMER_ON" : "TIMER_OFF");
  // sendBLECommand(status);
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
      // Format: TIME:HH:MM:SS
      int hh = data.substring(5, 7).toInt();
      int mm = data.substring(8, 10).toInt();
      int ss = data.substring(11, 13).toInt();

      currentHour = hh;
      currentMinute = mm;
      currentSecond = ss;

      lastTimeSync = millis();
      lastTick = millis();
    }
    
    if (data.startsWith("EVENTS:")) {
      parseEvents(data);
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