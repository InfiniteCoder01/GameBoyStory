#include "00Names.hpp"

void mainMenu();
String savepath = "/saves/Dev";
GState g_State;
Game gameSelect = { .draw = mainMenu };

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
  Story::load();
  Mario::load();
  Script::load();

  File file = SD.open(savepath + "/progress.dat", FILE_READ);
  if (!file) {
    file.close();
    saveState();
    file = SD.open(savepath + "/progress.dat", FILE_READ);
  }

  while (file.available()) {
    String property = readstr(file);
    readProperty(file, property, "nGames", g_State.nGames);
    readProperty(file, property, "money", g_State.money);
    readProperty(file, property, "mario.level", g_State.mario.level);
    readProperty(file, property, "mario.speedBuf", g_State.mario.speedBuf);
    readProperty(file, property, "mario.jumpBuf", g_State.mario.jumpBuf);
    readProperty(file, property, "mario.flipBuf", g_State.mario.flipBuf);
    readMap(file, property, "inventory.", g_State.inventory);
    if (property.substring(0, 8) == "dialogs.") Story::dialogs[property.substring(8)] = scriptBank[read<uint32_t>(file)];
  }

  file.close();

  game = gameSelect;
}

void saveState() {
  SD.mkdir(savepath);
  File file = SD.open(savepath + "/progress.dat", FILE_WRITE);
  writeProperty(file, "nGames", g_State.nGames);
  writeProperty(file, "money", g_State.money);
  writeProperty(file, "mario.level", g_State.mario.level);
  writeProperty(file, "mario.speedBuf", g_State.mario.speedBuf);
  writeProperty(file, "mario.jumpBuf", g_State.mario.jumpBuf);
  writeProperty(file, "mario.flipBuf", g_State.mario.flipBuf);
  writeMap(file, "inventory.", g_State.inventory);
  for (const auto& dialog : Story::dialogs) {
    writeProperty(file, "dialogs." + dialog.first, dialog.second->index);
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
