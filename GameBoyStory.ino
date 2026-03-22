#include "games.hpp"

void setup() {
  hardwareInit();
}

void loop() {
  oled.clear();
  const size_t nGames = 3;
  const String games[nGames] = {
    "Asteroids",
    "Tetris",
    "Mario",
  };
  static int pointer = 0;
  gui::drawList(oled, "Games", games, nGames, pointer);
  gui::rightText(oled, "Press Y for menu", oled.getHeight() - oled.getCharHeight());
  nextFrame();

  if (joyMoved) pointer = Math::wrap(pointer + joy.y, nGames);
  else if (buttonX.released) {
    if (pointer == 0) asteroids();
    // else if (pointer == 1) tetris();
    // else mario();
  } else if (buttonY.released) {
    uint8_t selected = 0;
    const auto cursor = [&selected](const auto idx) {
      return idx == selected ? '>' : ' ';
    };
    while (true) {
      oled.clear();
      oled.setCursor(0, 0);
      oled.println(format("%c Sounds: %s", cursor(0), soundEnabled ? "Yes" : "No"));
      nextFrame();

      if (buttonY.released) break;
      if (buttonX.released) {
        if (selected == 0) audioOutput.SetGain((soundEnabled = !soundEnabled) ? 1.0 : 0.0);
      }
    }
  }

  static int secretStage = 0;
  if (joyMoved) {
    if (joy.x > 0 && secretStage == 0) secretStage = 1;
    else if (joy.x > 0 && secretStage == 1) secretStage = 2;
    else if (joy.x < 0 && secretStage == 2) secretStage = 3;
    else if (joy.x > 0 && secretStage == 3) /* Nothing here rn */ secretStage = 0;
    else if (joy != 0) secretStage = 0;
  }
}
