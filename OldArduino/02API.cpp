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
  WRITING,  // Drawment & Logic
  WAITING_TO_SWAP,
  SWAPPING,  // SD
  WAITNG_TO_WRITE
} writeState = WriteState::WAITNG_TO_WRITE;

Game game;

void outputTask(void* arg) {
  while (true) {
    do vTaskDelay(1);
    while (writeState != WriteState::WRITING);
    //    audio::loop();
    oled::update();
    writeState = WriteState::WAITING_TO_SWAP;
    while (writeState != WriteState::SWAPPING) vTaskDelay(1);
    oled::swapBuffers();
    writeState = WriteState::WAITNG_TO_WRITE;
  }
}

void nextFrame() {
  static uint32_t t;
  writeState = WriteState::WRITING;

  deltaTime = (millis() - t) / 1000.f;
  t = millis();
  if (deltaTime <= 0) deltaTime = 1.f / 60.f;

  updateController();
  Script::update();
  if (game.update) game.update();
  if (game.draw) game.draw();

  while (writeState != WriteState::WAITING_TO_SWAP) yield();
  writeState = WriteState::SWAPPING;
  fileIO();
  while (writeState != WriteState::WAITNG_TO_WRITE) yield();
}
