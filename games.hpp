#pragma once
#include <Arduino.h>
#include <SD.h>
#include <OreonBSSD1351.hpp>
#include <OreonBSSD1351Gui.hpp>
#include <AudioFileSource.h>
#include <AudioGenerator.h>
#include <AudioOutputI2S.h>
#include <AudioOutputMixer.h>

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
void updateController(); // Called automatically in nextFrame()
void nextFrame();
String format(const char* format, ...);

// ================ Audio/Video ================ //
extern bool soundEnabled;
extern AudioOutputI2S audioOutput;
extern AudioOutputMixer mixer;
void playVideo(const String &path);

struct Sound {
  AudioFileSource *source;
  AudioGenerator *generator;
  AudioOutputMixerStub *stub;

  Sound(const String &filename);
  ~Sound() {
    delete source, generator, stub;
  }

  bool loop() {
    if (generator->isRunning()) {
      if (generator->loop()) return true;
      generator->stop();
      stub->stop();
    }
    return false;
  }
};

// ================ Games ================ //
void asteroids();
void tetris();
void mario();
