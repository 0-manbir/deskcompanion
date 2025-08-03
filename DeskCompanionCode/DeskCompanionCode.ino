#include <U8g2lib.h>
#include <ESP32Encoder.h>
#include <Wire.h>
#include <NimBLEDevice.h>

// === Display ===
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

// === States ===
enum AppState { IDLE, MENU, ACTIVITY };
AppState currentState = IDLE;

// === Menu Items ===
const char* menuItems[] = { "Timers", "Notifications", "Games", "Customize" };
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

// === PIR Sensor ===
#define PIR_PIN 35
bool userPresent = false;

// === Idle Animation Timing ===
unsigned long lastEyeAnim = 0;
const unsigned long eyeAnimInterval = 3000;

// === BLE Setup ===
#define SERVICE_UUID        "fa974e39-7cab-4fa2-8681-1d71f9fb73bc"
#define CHARACTERISTIC_RX   "12e87612-7b69-4e34-a21b-d2303bfa2691"  // From Flutter to ESP32
#define CHARACTERISTIC_TX   "2b50f752-b04e-4c9f-b188-aa7d5cf93dab"  // From ESP32 to Flutter
NimBLECharacteristic* txChar;


void setup() {
  Serial.begin(115200);
  u8g2.begin();

  pinMode(ENCODER1_BTN, INPUT_PULLUP);
  pinMode(ENCODER2_BTN, INPUT_PULLUP);
  pinMode(PIR_PIN, INPUT);

  // Encoder setup
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder2.attachFullQuad(ENCODER2_A, ENCODER2_B);
  encoder2.setCount(0);
  encoder1.attachFullQuad(ENCODER1_A, ENCODER1_B);
  encoder1.setCount(0);

  setupNimBLE();
}

void loop() {
  encoder1BtnPressed = !digitalRead(ENCODER1_BTN);
  encoder2BtnPressed = !digitalRead(ENCODER2_BTN);

  checkEncoder1();
  checkEncoder2();
  handleState();
  
  // You can call txChar->setValue("some text"); txChar->notify(); to send any data to app
}

// === State Handler ===
void handleState() {
  switch (currentState) {
    case IDLE:
      if (millis() - lastEyeAnim > eyeAnimInterval) {
        // blink(10);  // Add eye animation later
        lastEyeAnim = millis();
      }

      if (encoder1BtnPressed || encoder2BtnPressed)
        currentState = MENU;

      break;

    case MENU:
      if (encoder2BtnPressed) {
        Serial.println("current menu item clicked");
        encoder2BtnPressed = false;
      }

      drawMenu();

      if (encoder1BtnPressed) currentState = IDLE;
      break;

    case ACTIVITY:
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

    if (currentState == MENU) {
      selectedMenuIndex += (newPos > encoder2Pos) ? -1 : 1;
      if (selectedMenuIndex >= menuLength) selectedMenuIndex = 0;
      if (selectedMenuIndex < 0) selectedMenuIndex = menuLength - 1;
    }

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
class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    Serial.println("Connected to app");
  }

  void onDisconnect(NimBLEServer* pServer) {
    Serial.println("Disconnected");
  }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    Serial.print("Received from app: ");
    Serial.println(value.c_str());

    // Example: if app sends "PING", reply with "PONG"
    if (value == "PING") {
      txChar->setValue("PONG");
      txChar->notify();
    }
  }
};

void setupNimBLE() {
  NimBLEDevice::init("DeskCompanion");
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* service = server->createService(SERVICE_UUID);

  // RX (App -> ESP32)
  NimBLECharacteristic* rxChar = service->createCharacteristic(
    CHARACTERISTIC_RX,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  rxChar->setCallbacks(new RxCallbacks());

  // TX (ESP32 -> App)
  txChar = service->createCharacteristic(
    CHARACTERISTIC_TX,
    NIMBLE_PROPERTY::NOTIFY
  );

  service->start();
  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->start();

  Serial.println("BLE Ready, waiting for app");
}
