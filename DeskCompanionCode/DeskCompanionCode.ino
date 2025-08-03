#include <U8g2lib.h>
#include <RotaryEncoder.h> // https://github.com/mathertel/RotaryEncoder
#include <Wire.h>

// === Display ===
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// === States ===
enum AppState { IDLE, MENU, ACTIVITY };
AppState currentState = IDLE;

// === Menu Items ===
const char* menuItems[] = {"Timers", "Notifications", "Games", "Customize"};
int menuLength = sizeof(menuItems) / sizeof(menuItems[0]);
int selectedMenuIndex = 0;

// === Encoder Pins ===
#define ENCODER1_A 32
#define ENCODER1_B 33
#define ENCODER1_BTN 25

#define ENCODER2_A 26
#define ENCODER2_B 27
#define ENCODER2_BTN 14

// === Encoders ===
RotaryEncoder encoder1(ENCODER1_A, ENCODER1_B);
RotaryEncoder encoder2(ENCODER2_A, ENCODER2_B);
bool encoder1BtnPressed = false;
bool encoder2BtnPressed = false;

// === PIR Sensor ===
#define PIR_PIN 35
bool userPresent = false;

// === Idle Animation Timing ===
unsigned long lastEyeAnim = 0;
const unsigned long eyeAnimInterval = 3000;

// === Function Prototypes ===
void updateMenu();
void drawMenu();
void checkEncoders();
void handleState();
void drawStatusBar();
void updateIdleEyes();

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  pinMode(ENCODER1_BTN, INPUT_PULLUP);
  pinMode(ENCODER2_BTN, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);
  reset_eyes();
}

void loop() {
  userPresent = digitalRead(PIR_PIN);
  encoder1.tick();
  encoder2.tick();

  encoder1BtnPressed = !digitalRead(ENCODER1_BTN);
  encoder2BtnPressed = !digitalRead(ENCODER2_BTN);

  checkEncoders();
  handleState();
}

// === State Handler ===
void handleState() {
  switch (currentState) {
    case IDLE:
      if (userPresent) {
        if (millis() - lastEyeAnim > eyeAnimInterval) {
          // blink(10);  // You can randomize or cycle eyes
          lastEyeAnim = millis();
        }
      } else {
        sleep();
      }

      if (encoder1BtnPressed || encoder2BtnPressed)
        currentState = MENU;

      break;

    case MENU:
      drawMenu();

      if (encoder1BtnPressed) {
        currentState = ACTIVITY;
        // TODO: Load selected feature
      }
      break;

    case ACTIVITY:
      // Placeholder: just return to idle after timeout or back button
      drawStatusBar();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(10, 40, menuItems[selectedMenuIndex]);
      u8g2.sendBuffer();

      if (encoder1BtnPressed) {
        currentState = MENU;
      }

      break;
  }
}

// === Menu Drawing ===
void drawMenu() {
  u8g2.clearBuffer();
  drawStatusBar();
  u8g2.setFont(u8g2_font_6x12_tr);
  for (int i = 0; i < menuLength; i++) {
    if (i == selectedMenuIndex) {
      u8g2.drawBox(0, i * 12, 128, 12);
      u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);
    }
    u8g2.drawStr(2, (i + 1) * 12 - 2, menuItems[i]);
  }
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

// === Encoder Check ===
void checkEncoders() {
  static int lastPos = -999;

  int pos = encoder1.getPosition();
  if (pos != lastPos) {
    lastPos = pos;
    if (currentState == MENU) {
      selectedMenuIndex = (selectedMenuIndex + (pos > lastPos ? 1 : -1) + menuLength) % menuLength;
    }
  }
}

// === Status Bar ===
void drawStatusBar() {
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, "12:00");
  u8g2.drawStr(40, 7, "26C");
  u8g2.drawStr(100, 7, "🔋");
}

// === Eyes ===
