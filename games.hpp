#pragma once
#include <Arduino.h>
#include <SD.h>
#include <OreonBSSD1351.hpp>
#include <OreonBSSD1351Gui.hpp>

using namespace VectorMath;

// ================ Hardware ================ //
struct Button {
  Button() {}
  Button(int pin)
    : pin(pin) {}

  void tick();
  bool pressed, released, held;
  int pin;
};

extern OreonBSSD1351 oled;
extern vec2i joy;
extern bool joyMoved;
extern Button buttonX, buttonY;
extern float deltaTime;

void hardwareInit();
void nextFrame();
String format(const char* format, ...);

// ================ Games ================ //
void menu();
void asteroids();
void tetris();
void mario();
