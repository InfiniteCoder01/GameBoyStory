#include "00Names.hpp"

void setup() {
  pinsSetup();
  load();
  xTaskCreatePinnedToCore(outputTask, "Output Task", 10240, NULL, configMAX_PRIORITIES - 1, NULL, 0);
  // Events::Easter::start();
}

void loop() {
  nextFrame();
}
