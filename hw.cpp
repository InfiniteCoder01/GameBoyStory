#include "games.hpp"

const int PIN_UP = 33;
const int PIN_DOWN = 14;
const int PIN_LEFT = 32;
const int PIN_RIGHT = 27;
const int PIN_X = 12;
const int PIN_Y = 13;
const int JOY_HOLD = 300;

void Button::tick() {
  const auto last = held;
  held = !digitalRead(pin);
  pressed = !last && held;
  released = last && !held;
}

OreonBSSD1351 oled;
vec2i joy = vec2i(0, 0);
bool joyMoved = false;
Button buttonX(PIN_X), buttonY(PIN_Y);
float deltaTime = 1.0 / 30.0;

void hardwareInit() {
  Serial.begin(115200);
  SD.begin(4, SPI, SPI_SPEED);
  digitalWrite(4, HIGH);
  pinMode(PIN_UP, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_X, INPUT_PULLUP);
  pinMode(PIN_Y, INPUT_PULLUP);
  oled.begin(5, 17, 16);
}

static void updateController() {
  static uint32_t t;
  yield();
  int newJoyX = digitalRead(PIN_LEFT) - digitalRead(PIN_RIGHT);
  int newJoyY = digitalRead(PIN_UP) - digitalRead(PIN_DOWN);
  buttonX.tick();
  buttonY.tick();
  if ((joy.x != newJoyX) || (joy.y != newJoyY) || (millis() - t > JOY_HOLD)) {
    t = millis();
    joy.x = newJoyX;
    joy.y = newJoyY;
    joy = vec2i(joy.x, joy.y);
    joyMoved = true;
  } else {
    joyMoved = false;
  }
}

void nextFrame() {
  static uint32_t t;
  deltaTime = (millis() - t) / 1000.f;
  t = millis();
  if (deltaTime <= 0) deltaTime = 1.f / 30.f;
  updateController();
  oled.update();
  yield();
}

String format(const char* format, ...) {
  va_list args;

  va_start(args, format);
  char buf[vsnprintf(NULL, 0, format, args)];
  vsprintf(buf, format, args);
  va_end(args);

  return String(buf);
}
