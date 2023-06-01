#include "OreonContainer.hpp"
#define SPI_SPEED 39999999  // 10000000
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

template<typename T> inline void readProperty(File& file, const String& property, const String& label, T& value) {
  if (property == label) value = read<T>(file);
}

template<typename T> inline void writeProperty(File& file, const String& label, const T& value) {
  writestr(file, label);
  write<T>(file, value);
}

template<typename T> inline void readMap(File& file, const String& property, const String& prefix, VectorMap<String, T>& map) {
  if (property.substring(0, prefix.length()) == prefix) map[property.substring(prefix.length())] = read<T>(file);
}

template<typename T> inline void writeMap(File& file, const String& prefix, VectorMap<String, T>& map) {
  for (const auto& entry : map) {
    writeProperty(file, prefix + entry.first, entry.second);
  }
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
  void (*useItem)(const String& item);
  void (*buyItem)(const String& item);
};

extern Game game;

void pinsSetup();
void outputTask(void* arg);
void nextFrame();
#pragma endregion API
#pragma region Global
extern String savepath;

struct GState {
  uint8_t nGames = 3;
  uint32_t money = 2000;
  VectorMap<String, uint32_t> inventory;

  // Mario
  struct Mario {
    int16_t level = 0;

    struct Buf {
      float timer = 0.f, multiplier = 1.f;
      Buf() = default;
      Buf(float timer, float multiplier)
        : timer(timer), multiplier(multiplier) {}
    };

    Buf speedBuf, jumpBuf;
    float flipBuf = 0.f;
  } mario;
};

extern GState g_State;

void fileIO();
void saveState();
void load();
extern Game gameSelect;
#pragma endregion Global
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
void draw();

void openShopDialog(const String& title, std::initializer_list<String> options, uint32_t itemsOffset, uint32_t itemCount);
inline void openShopDialog(std::initializer_list<String> options, uint32_t itemsOffset, uint32_t itemCount) {
  openShopDialog("", options, itemsOffset, itemCount);
}

void sendMessage(String author, String message);
void toggleInventory();

extern int32_t inventoryItem;
}
#pragma endregion UI
#pragma region Script

namespace Script {
#pragma region Nodes
struct Node {
  uint32_t index = 0;
  Node* next = nullptr;

  Node();
  virtual void run() {}
  virtual bool update() = 0;
};

struct LinearNode : Node {
  LinearNode()
    : Node() {}

  bool update() override {
    return true;
  }

  template<typename T> T* add(T* node) {
    next = node;
    return node;
  }
};

struct UnimplementedScriptNode : public LinearNode {
  UnimplementedScriptNode()
    : LinearNode() {}

  bool update() override {
    return false;
  }
};

struct Invoke : public LinearNode {
  std::function<void()> function;

  Invoke(std::function<void()> function)
    : LinearNode(), function(function) {}

  void run() override {
    function();
  }
};

struct WaitUntil : public LinearNode {
  std::function<bool()> function;

  WaitUntil(std::function<bool()> function)
    : LinearNode(), function(function) {}

  bool update() override {
    return function();
  }
};

struct Dialog : public Node {
  String title;
  Vector<String> answers;
  Vector<Node*> actions;

  Dialog(const String& title)
    : Node(), title(title) {}

  Dialog* answer(const String& answer);
  Dialog* action(Node* action);
  void run() override;
  bool update() override;
};
#pragma endregion Nodes

void addThread(Node* head);
void update();
void save();
void load();
}

using Script::Dialog;
using Script::LinearNode;
using ScriptNode = Script::Node;
using InvokeNode = Script::Invoke;
using WaitUntilNode = Script::WaitUntil;
using Script::UnimplementedScriptNode;

extern Vector<ScriptNode*> scriptBank;
#pragma endregion Script
#pragma region CustomScripts
struct WaitLevel : public LinearNode {
  uint16_t index;

  WaitLevel(uint16_t index)
    : LinearNode(), index(index) {}

  bool update() override {
    return TileEngine::levelIndex == index;
  }
};
#pragma endregion CustomScripts
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
const int LUIGI_ATLAS_INDEX = MARIO_ATLAS_INDEX + 2;

namespace Mario {
void load();
}

namespace Story {
extern ScriptNode* root;
extern VectorMap<String, ScriptNode*> dialogs;
void load();
}
#pragma endregion Story
#pragma region Items
static const uint32_t shopPrices[] = {
  2, 2000
};

static const String shopItems[] = {
  "Coffee", "Gold Coffee"
};
#pragma endregion Items
#pragma region Events
namespace Events::Easter {
void start();
}
#pragma endregion Events
