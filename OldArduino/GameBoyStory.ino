#include "00Names.hpp"

void updateController();
void setup() {
  pinsSetup();

  {
    updateController();
    if (joy.y > 0) savepath = "/saves/Dev";
    else if (buttonY.bHeld) savepath = "/saves/video";
    else savepath = "/saves/player";
  }

  load();
  xTaskCreatePinnedToCore(outputTask, "Output Task", 10240, NULL, configMAX_PRIORITIES - 1, NULL, 0);
}

void loop() {
  nextFrame();
}
