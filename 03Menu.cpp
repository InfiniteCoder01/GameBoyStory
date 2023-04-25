#include "00Names.hpp"

void mainMenu();
String savepath = "/saves/Dev";
GState g_State;
Game gameSelect = Game(nullptr, mainMenu);

Function games[] = {
  Mario::start,
  Asteroids::start,
  Tetris::start,
};

String gameNames[] = {
  "Mario",
  "Asteroids",
  "Tetris",
};

void load() {
  File file = SD.open(savepath + "/progress.dat", FILE_READ);
  if (!file) saveState();
  g_State = read<GState>(file);
  file.close();

  Mario::load();

  // file = SD.open(savepath + "/scripts.dat", FILE_READ); // TODO: !
  // while (file.available()) Script::addThread(scriptBank[(int)read<uint32_t>(file)]);
  // file.close();

  game = gameSelect;
}

void saveState() {
  File file = SD.open(savepath + "/progress.dat", FILE_WRITE);
  write<GState>(file, g_State);
  file.close();
  // Script::save();
  if (game.save) game.save();
}

void fileIO() {
  static uint32_t timer;  // Auto Save
  if (millis() - timer > 10000) {
    timer = millis();
    saveState();
  }
  if (game.fileIO) game.fileIO();
}

void mainMenu() {
  static int pointer = 0;
  static int secretStage = 0;
  if (bJoyMoved) pointer = wrap(pointer + joy.y, g_State.nGames);
  else if (buttonX.bReleased) games[pointer](), secretStage = 0;

  if (bJoyMoved) {
    if (joy.x > 0 && secretStage == 0) secretStage = 1;
    else if (joy.x > 0 && secretStage == 1) secretStage = 2;
    else if (joy.x < 0 && secretStage == 2) secretStage = 3;
    else if (joy.x > 0 && secretStage == 3) /*Events::HappyBirthday::start(), */ secretStage = 0;
    else if (joy != 0) secretStage = 0;
  }

  oled::clear();
  gui::drawList("Games", gameNames, g_State.nGames, pointer);
}
