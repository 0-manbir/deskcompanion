void displayEventsFace() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(25, 32, "Events/TODOs");
  u8g2.drawStr(15, 45, "Coming from app...");
  u8g2.sendBuffer();
}
