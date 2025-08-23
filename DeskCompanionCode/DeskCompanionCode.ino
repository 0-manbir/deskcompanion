#include <SPI.h>
#include <U8g2lib.h>
#include <ESP32Encoder.h>
#include <Wire.h>
#include <NimBLEDevice.h>

// === Display ===
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
static const int SCREEN_WIDTH = 128;
static const int SCREEN_HEIGHT = 64;

// === States ===
enum AppState { IDLE, MUSIC, TIME };
AppState currentState = IDLE;

// === Menu Items ===
const char* menuItems[] = { "Timers", "Notifications", "Games" };
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
ESP32Encoder encoder1;
ESP32Encoder encoder2;
int encoder1Pos = 0;
int encoder2Pos = 0;
bool encoder1BtnPressed = false;
bool encoder2BtnPressed = false;

// === Battery ===
#define BATTERY 34
const float R1 = 100000.0;  // 100kΩ
const float R2 = 100000.0;  // 100kΩ
const float BATTERY_MAX = 4.2;   // Full
const float BATTERY_MIN = 3.3;   // Empty

// === PIR Sensor ===
#define PIR_PIN 35
bool userPresent = false;

// === Idle Animation Timing ===
unsigned long lastEyeAnim = 0;
const unsigned long eyeAnimInterval = 5000;

// === BLE Setup ===
#define SERVICE_UUID        "fa974e39-7cab-4fa2-8681-1d71f9fb73bc"
#define CHARACTERISTIC_RX   "12e87612-7b69-4e34-a21b-d2303bfa2691"  // From Flutter to ESP32
#define CHARACTERISTIC_TX   "2b50f752-b04e-4c9f-b188-aa7d5cf93dab"  // From ESP32 to Flutter
NimBLECharacteristic* txChar;
bool deviceConnected = false;

// === Eyes ===
enum EyeAnim { DRAW_EYES, SLEEP, WAKEUP, RESET, MOVE_RIGHT_EYE, MOVE_LEFT_EYE, BLINK, HAPPY, LOOK_AROUND };
int COLOR_WHITE = 1;
int COLOR_BLACK = 0;

// reference state
int ref_left_eye = 32;
int ref_eye_height = 40;
int ref_eye_width = 40;
int ref_space_between_eye = 10;
int ref_corner_radius = 10;

// current state of the eyes
int left_eye_height = ref_eye_height;
int left_eye_width = ref_eye_width;
int left_eye_x = ref_left_eye;
int left_eye_y = ref_left_eye;
int right_eye_x = ref_left_eye + ref_eye_width + ref_space_between_eye;
int right_eye_y = ref_left_eye;
int right_eye_height = ref_eye_height;
int right_eye_width = ref_eye_width;
int corner_radius = ref_corner_radius;


void setup() {
  Serial.begin(115200);
  u8g2.begin();

  u8g2.setContrast(255);

  setupNimBLE();

  pinMode(ENCODER1_BTN, INPUT_PULLUP);
  pinMode(ENCODER2_BTN, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);

  // Encoder setup
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder2.attachFullQuad(ENCODER2_A, ENCODER2_B);
  encoder2.setCount(0);
  encoder1.attachFullQuad(ENCODER1_A, ENCODER1_B);
  encoder1.setCount(0);
}

void loop() {
  encoder1BtnPressed = !digitalRead(ENCODER1_BTN);
  encoder2BtnPressed = !digitalRead(ENCODER2_BTN);

  checkEncoder1();
  checkEncoder2();
  handleState();

  if (deviceConnected) {
    static unsigned long lastSent = 0;
    if (millis() - lastSent > 3000) {
      txChar->setValue("📤 Hello from ESP32");
      txChar->notify();
      lastSent = millis();
    }
  }

  // batetery ry
  float voltage = readBatteryVoltage();
  int percent = batteryPercentage(voltage);

  
  Serial.print("Battery: ");
  Serial.print(voltage, 2);
  Serial.print(" V | ");
  Serial.print(percent);
  Serial.println("%");

  // --- Draw on OLED ---

  u8g2.clearBuffer();

  char charArray[10];
  dtostrf(voltage, 5, 4, charArray);
  
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(0, 20, charArray);
  u8g2.sendBuffer();
  // u8g2.clearBuffer();

  // char buf[20];
  // sprintf(buf, "Battery: %.2fV", voltage);
  // u8g2.drawStr(0, 20, buf);

  // sprintf(buf, "Charge: %d%%", percent);
  // u8g2.drawStr(0, 40, buf);

  // u8g2.sendBuffer();

  delay(2000);
}

// === Battery ===
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

// === State Handler ===
void handleState() {
  switch (currentState) {
    case IDLE:
      // eye animations
      if (millis() - lastEyeAnim > eyeAnimInterval) {
        lastEyeAnim = millis();
      }
      break;

    case MUSIC:
      break;

    case TIME:
      break;
  }
}

// === Menu Drawing ===
void drawMenu() {
  u8g2.clearBuffer();
  drawStatusBar();
  u8g2.setFont(u8g2_font_6x12_tr);
  int offset = 10;
  for (int i = 0; i < menuLength; i++) {
    if (i == selectedMenuIndex) {
      u8g2.drawBox(0, i * 12 + offset, 128, 12);
      u8g2.setDrawColor(0);
    } else {
      u8g2.setDrawColor(1);
    }
    u8g2.drawStr(2, ((i + 1) * 12 - 2) + offset, menuItems[i]);
  }
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

// === Encoder Checks ===
void checkEncoder1() {
  // Not implemented yet
}

void checkEncoder2() {
  int newPos = encoder2.getCount() / 4;

  if (newPos != encoder2Pos) {
    Serial.println("encoder 2 moved");

    // traverse main menu
    // if (currentState == MENU) {
    //   selectedMenuIndex += (newPos > encoder2Pos) ? -1 : 1;
    //   if (selectedMenuIndex >= menuLength) selectedMenuIndex = 0;
    //   if (selectedMenuIndex < 0) selectedMenuIndex = menuLength - 1;
    // }

    encoder2Pos = newPos;
  }
}

// === Status Bar ===
void drawStatusBar() {
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(0, 7, "12:00");
  u8g2.drawStr(40, 7, "26C");
  u8g2.drawStr(100, 7, "🔋");
}


// === BLE ===

// === RX Callback: handle incoming data ===
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override {
    std::string value = pChar->getValue();
    Serial.print("📩 Received: ");
    Serial.println(value.c_str());

    // You can process the data here...
  }
} rxCallbacks;

// === Server Callback: connection status ===
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
    deviceConnected = true;
    Serial.println("✅ Connected to phone");
  }

  void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
    deviceConnected = false;
    Serial.println("❌ Disconnected");
    NimBLEDevice::startAdvertising();
  }
} serverCallbacks;

void setupNimBLE() {
    // Initialize NimBLE
    NimBLEDevice::init("ESP32");
    
    // Create server
    NimBLEServer* pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);
    
    // Create service
    NimBLEService* pService = pServer->createService(SERVICE_UUID);
    
    // RX Characteristic
    NimBLECharacteristic* rxChar = pService->createCharacteristic(
        CHARACTERISTIC_RX,
        NIMBLE_PROPERTY::WRITE | 
        NIMBLE_PROPERTY::WRITE_NR
    );
    rxChar->setCallbacks(&rxCallbacks);
    
    // TX Characteristic
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
    
    Serial.println("🚀 BLE server ready");
}



// === EYES === 
class EyeManager {
public:
  struct EyeState {
    int x, y, w, h;
    int corner;
  };

  EyeState left, right;
  int spacing;

  EyeManager(int eyeW, int eyeH, int spacing, int corner) {
    this->spacing = spacing;
    left = { SCREEN_WIDTH/2 - eyeW/2 - spacing/2, SCREEN_HEIGHT/2, eyeW, eyeH, corner };
    right = { SCREEN_WIDTH/2 + eyeW/2 + spacing/2, SCREEN_HEIGHT/2, eyeW, eyeH, corner };
  }

  void display_display() { u8g2.sendBuffer(); } 
  void display_clearDisplay() { u8g2.clearBuffer(); }

  void display_fillRoundRect(int x, int y, int w, int h, int r, int color) {
    u8g2.setDrawColor(color); //behavior is not defined if r is smaller than the height or width,
    if (w < 2 * (r + 1)) {
      r = (w / 2) - 1; }
    if (h < 2 * (r + 1)) {
      r = (h / 2) - 1;
    }
    
    //check if height and width are valid when calling drawRBox 
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
    left.x = SCREEN_WIDTH/2 - left.w/2 - spacing/2;
    right.x = SCREEN_WIDTH/2 + right.w/2 + spacing/2;
    left.y = right.y = SCREEN_HEIGHT/2;
    draw();
  }

  void draw(bool update = true) {
    display_clearDisplay();
    drawEye(left);
    drawEye(right);
    if (update) display_display();
  }

  void blink() {
    reset();
    for (int i=0;i<3;i++) {
      left.h -= 10; right.h -= 10;
      draw();
      delay(30);
    }
    for (int i=0;i<3;i++) {
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
    // arcs like a smile under each eye
    display_fillTriangle(left.x-left.w/2, left.y, left.x+left.w/2, left.y, left.x, left.y+left.h/2, COLOR_BLACK);
    display_fillTriangle(right.x-right.w/2, right.y, right.x+right.w/2, right.y, right.x, right.y+right.h/2, COLOR_BLACK);
    display_display();
  }

  void sad() {
    reset();
    // inverted arcs
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

