// === BLE Setup ===
#define SERVICE_UUID "fa974e39-7cab-4fa2-8681-1d71f9fb73bc"
#define CHARACTERISTIC_RX "12e87612-7b69-4e34-a21b-d2303bfa2691"
#define CHARACTERISTIC_TX "2b50f752-b04e-4c9f-b188-aa7d5cf93dab"
NimBLECharacteristic* txChar;
bool deviceConnected = false;

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