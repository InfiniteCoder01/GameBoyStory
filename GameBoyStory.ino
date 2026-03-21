#include "games.hpp"

void setup() {
  hardwareInit();
}

void loop() {
  static int secretStage = 0;
  if (joyMoved) {
    if (joy.x > 0 && secretStage == 0) secretStage = 1;
    else if (joy.x > 0 && secretStage == 1) secretStage = 2;
    else if (joy.x < 0 && secretStage == 2) secretStage = 3;
    else if (joy.x > 0 && secretStage == 3) /*Events::HappyBirthday::start(), */ secretStage = 0;
    else if (joy != 0) secretStage = 0;
  }

  oled.clear();
  const size_t nGames = 3;
  const String games[nGames] = {
    "Asteroids",
    "Tetris",
    "Mario",
  };
  static int pointer = 0;
  gui::drawList(oled, "Games", games, nGames, pointer);

  if (joyMoved) pointer = Math::wrap(pointer + joy.y, nGames);
  else if (buttonX.released) {
    if (pointer == 0) asteroids();
    // else if (pointer == 1) tetris();
    // else mario();
  }
  
  nextFrame();
}
