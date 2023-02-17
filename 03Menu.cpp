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

  file = SD.open(savepath + "/scripts.dat", FILE_READ);
  while (file.available()) Script::addThread(scriptBank[(int)read<uint32_t>(file)]);
  file.close();

  game = gameSelect;
}

void saveState(bool saveGame) {
  static uint32_t timer;  // Auto Save
  bool autosave = false;
  if (millis() - timer > 10000 || saveGame) {
    timer = millis();
    File file = SD.open(savepath + "/progress.dat", FILE_WRITE);
    write<GState>(file, g_State);
    file.close();
    Script::save();
    autosave = true;
  }
  if (game.save) game.save(autosave);
}

void mainMenu() {
  static int pointer = 0;
  if (bJoyMoved) pointer = wrap(pointer + joy.y, g_State.nGames);
  else if (buttonX.bReleased) games[pointer]();

  oled::clear();
  gui::drawList("Games", gameNames, g_State.nGames, pointer);
}
