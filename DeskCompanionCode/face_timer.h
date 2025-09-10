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

// === Timer Variables ===
unsigned long timerStartTime = 0;
unsigned long timerDuration = 0;  // in milliseconds
unsigned long timerElapsed = 0;
bool timerRunning = false;
int pomodoroSession = 1;
int timerMinutes = 5;  // Default timer setting

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