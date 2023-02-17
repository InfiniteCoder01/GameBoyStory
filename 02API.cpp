#include "00Names.hpp"

const int PIN_UP = 33;
const int PIN_DOWN = 14;
const int PIN_LEFT = 32;
const int PIN_RIGHT = 27;
const int PIN_X = 12;
const int PIN_Y = 13;
const int JOY_HOLD = 300;

void Button::tick() {
  bool bLast = bHeld;
  bHeld = !digitalRead(pin);
  bPressed = !bLast && bHeld;
  bReleased = bLast && !bHeld;
}

bool bJoyMoved;
vec2i joy = vec2i(0);
Button buttonX(PIN_X), buttonY(PIN_Y);
float deltaTime = 1;

void pinsSetup() {
  Serial.begin(115200);
  SD.begin(4, SPI, SPI_SPEED);
  digitalWrite(4, HIGH);
  pinMode(PIN_UP, INPUT_PULLUP);
  pinMode(PIN_DOWN, INPUT_PULLUP);
  pinMode(PIN_LEFT, INPUT_PULLUP);
  pinMode(PIN_RIGHT, INPUT_PULLUP);
  pinMode(PIN_X, INPUT_PULLUP);
  pinMode(PIN_Y, INPUT_PULLUP);
  oled::begin(5, 17, 16);
  //  audio::begin();
}

void updateController() {
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
    bJoyMoved = true;
  } else {
    bJoyMoved = false;
  }
}

enum class WriteState {
  WRITING,   // Drawment & Logic
  SWAPPING,  // SD
} writeState = WriteState::SWAPPING;

Game game;

void outputTask(void* arg) {
  while (true) {
    vTaskDelay(1);
    while (writeState != WriteState::WRITING) vTaskDelay(1);
    //    audio::loop();
    oled::update();
    writeState = WriteState::SWAPPING;
    oled::swapBuffers();
  }
}

void nextFrame() {
  static uint32_t t;
  deltaTime = (millis() - t) / 1000.f;
  t = millis();
  if (deltaTime <= 0) deltaTime = 1.f / 60.f;
  updateController();

  writeState = WriteState::WRITING;
  Script::update();
  if (game.update) game.update();
  if (game.draw) game.draw();
  UI::drawChat();

  while (writeState != WriteState::SWAPPING) yield();
  saveState();
  writeState = WriteState::WRITING;
}
