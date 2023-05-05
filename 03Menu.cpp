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
  if (!file) {
    file.close();
    saveState();
    file = SD.open(savepath + "/progress.dat", FILE_READ);
  }

  while (file.available()) {
    String property = readstr(file);
    if (property == "nGames") g_State.nGames = read<uint8_t>(file);
    if (property == "mario.balance") g_State.mario.balance = read<uint32_t>(file);
    if (property == "mario.level") g_State.mario.level = read<int16_t>(file);
    if (property == "mario.state") g_State.mario.state = read<GState::MainQuest>(file);
    if (property.substring(0, 16) == "mario.inventory.") g_State.mario.inventory[property.substring(16)] = read<uint32_t>(file);
  }

  file.close();

  Script::load();
  Mario::load();
  game = gameSelect;
}

void saveState() {
  SD.mkdir(savepath);
  File file = SD.open(savepath + "/progress.dat", FILE_WRITE);
  writeProperty(file, g_State.nGames, "nGames");
  writeProperty(file, g_State.mario.balance, "mario.balance");
  writeProperty(file, g_State.mario.level, "mario.level");
  writeProperty(file, g_State.mario.state, "mario.state");
  for (auto& entry : g_State.mario.inventory) {
    writeProperty(file, entry.second, "mario.inventory." + entry.first);
  }
  file.close();

  Script::save();
  if (game.save) game.save();
}

static float reseting = 0;
void fileIO() {
  if (reseting > 2 && buttonX.bReleased) {
    removeDirectory(savepath);
    load();
    buttonX.bReleased = false;
    reseting = 0;
  }

  static uint32_t timer;  // Auto Save
  if (millis() - timer > 10000) {
    timer = millis();
    saveState();
  }
  if (game.fileIO) game.fileIO();
}

void mainMenu() {
  if (reseting > 2) {
    oled::clear();
    gui::centerText("X To reset", 0);
    gui::centerText("Y To quit", oled::getCharHeight());
    if (buttonY.bPressed) reseting = 0;
    return;
  }

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

  if (buttonY.bHeld) reseting += deltaTime;
  else reseting = 0;

  oled::clear();
  gui::drawList("Games", gameNames, g_State.nGames, pointer);
  gui::textAt(savepath.substring(savepath.lastIndexOf('/') + 1), 0, oled::height - oled::getCharHeight());
}
