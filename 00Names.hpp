#include "OreonContainer.hpp"
#define SPI_SPEED 39999999
#define TRIPPLE_BUFFER
// #define DRAW_COLLIDERS
#include <Arduino.h>

#include <SD.h>
#include <OreonBSSD1351.hpp>
#include <OreonBSSD1351Gui.hpp>
#include <OreonBSSD1351Tile.hpp>
#include <stdlib.h>
#include <stdint.h>

#undef min
#undef max
#undef abs
#undef round

using namespace Math;
using namespace Container;
using namespace VectorMath;

using Function = void (*)();

#pragma region Utils
String format(const char* format, ...);

template<typename T> inline T read(File& file) {
  T value;
  file.read((uint8_t*)&value, sizeof(value));
  return value;
}

template<typename T> inline void write(File& file, const T& value) {
  file.write((uint8_t*)&value, sizeof(value));
}

inline String readstr(File& file) {
  String str = "";
  while (true) {
    char c = file.read();
    if (c == '\0') break;
    str += c;
  }
  return str;
}

static void writestr(File& file, const String& str) {
  file.write((uint8_t*)str.c_str(), str.length() + 1);
}

template<typename T> inline void writeProperty(File& file, const T& value, String label) {
  writestr(file, label);
  write<T>(file, value);
}

static void removeDirectory(String path) {
  File dir = SD.open(path);

  dir.rewindDirectory();
  while (File entry = dir.openNextFile()) {
    auto name = entry.name();
    entry.close();
    if (entry.isDirectory()) removeDirectory(path + '/' + name);
    else SD.remove(path + name);
  }

  dir.close();
  SD.rmdir(path);
}
#pragma endregion Utils
#pragma region API
struct Button {
  Button() {}
  Button(int pin)
    : pin(pin) {}

  void tick();
  bool bPressed, bReleased, bHeld;
  int pin;
};

extern bool bJoyMoved;
extern vec2i joy;
extern Button buttonX, buttonY;
extern float deltaTime;

struct Game {
  Function update, draw, fileIO, save;

  Game(Function update = nullptr, Function draw = nullptr, Function save = nullptr, Function fileIO = nullptr)
    : update(update), draw(draw), fileIO(fileIO), save(save) {}
};
extern Game game;

void pinsSetup();
void outputTask(void* arg);
void nextFrame();
#pragma endregion API
#pragma region Menu
extern String savepath;

struct GState {
  uint8_t nGames = 3;

  enum MainQuest : uint16_t {
    GetMower
  };

  // Mario
  struct {
    uint32_t balance = 2000;
    int16_t level = 0;
    MainQuest state = GetMower;
    VectorMap<String, uint32_t> inventory;
  } mario;
};

extern GState g_State;

void fileIO();
void saveState();
void load();
extern Game gameSelect;
#pragma endregion Menu
#pragma region UI
namespace UI {
void drawCanvas();
void setCanvasText();
void resetText();

struct Dialog {
  String title;
  Vector<String> answers;

  bool active = false, choosed = false;
  uint16_t choice = 0;

  Dialog() = default;
  Dialog(const String& title, const Vector<String>& answers);
  Dialog(const String& title, const String* answers, uint32_t count);

  void draw();
};

extern Dialog dialog;
void drawDialog();

void sendMessage(String author, String message);
void drawChat();
}
#pragma endregion UI
#pragma region Script
namespace Script {

#pragma region Nodes
struct Node {
  Node* next = nullptr;

  Node();
  virtual void run() = 0;
  virtual bool update() = 0;
};

struct Dialog : public Node {
  String title;
  Vector<String> answers;
  Vector<Node*> actions;

  Dialog(String title)
    : Node(), title(title) {}

  Dialog* answer(String answer, Node* action = 0);
  void run() override;
  bool update() override;
};

template<typename T>
struct Set : public Node {
  T* variable;
  T value;

  Set(T& variable, T value)
    : Node(), variable(&variable), value(value) {}

  void run() override {
    *variable = value;
  }

  bool update() override {
    return true;
  }
};
#pragma endregion Nodes

void addThread(Node* head);
void update();
void save();
void load();
}

using Script::Dialog;
using ScriptNode = Script::Node;
extern Vector<ScriptNode*> scriptBank;
#pragma endregion Script
#pragma region MenuGames
namespace Mario {
void start();
}

namespace Asteroids {
void start();
}

namespace Tetris {
void start();
}
#pragma endregion MenuGames
#pragma region Story
const int MARIO_ATLAS_INDEX = 4;
namespace Mario {
extern TileEngine::Atlas* atlases;
void load();
}
#pragma endregion Story
#pragma region Events
namespace Events::Easter {
void start();
}
#pragma endregion Events
