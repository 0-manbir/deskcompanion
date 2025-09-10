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
