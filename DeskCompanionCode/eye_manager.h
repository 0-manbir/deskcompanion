extern U8G2 u8g2;

// === Display & Animation Timing ===
unsigned long lastEyeAnim = 0;
const unsigned long eyeAnimInterval = 5000;
unsigned long lastAccelRead = 0;
const unsigned long accelReadInterval = 50;
unsigned long lastTempRead = 0;
const unsigned long tempReadInterval = 5000;

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
    reset();
    for (int h = 2; h <= defaultH; h += 4) {
      left.h = right.h = h;
      left.corner = right.corner = min(h, defaultCorner);
      draw();
      delay(15);
    }
  }

  void happy() {
    reset();
    display_fillTriangle(left.x - left.w / 2, left.y, left.x + left.w / 2, left.y, left.x, left.y + left.h / 2, COLOR_BLACK);
    display_fillTriangle(right.x - right.w / 2, right.y, right.x + right.w / 2, right.y, right.x, right.y + right.h / 2, COLOR_BLACK);
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