extern AppState currentState;

// === Encoder Pins ===
#define ENCODER1_A 33
#define ENCODER1_B 25
#define ENCODER1_BTN 26

#define ENCODER2_A 27
#define ENCODER2_B 14
#define ENCODER2_BTN 12

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