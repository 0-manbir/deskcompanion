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
