#define SPI_SPEED 39999999
#define TRIPPLE_BUFFER
// #define DRAW_COLLIDERS
#include <Arduino.h>

#include <SD.h>
#include <LinkedList.h>
#include <OreonBSSD1351.hpp>
#include <OreonBSSD1351Gui.hpp>
#include <OreonBSSD1351Tile.hpp>
#include <stdlib.h>
#include <stdint.h>

using namespace Math;
using namespace VectorMath;

using Function = void (*)();

#pragma region Utils
template<typename M, typename N> struct Pair {
  Pair() = default;
  Pair(M first, N second)
    : first(first), second(second) {}

  M first;
  N second;
};

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
  return file.readString();
}

inline void writestr(File& file, const String& str) {
  file.write((uint8_t*)str.c_str(), str.length() + 1);
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

  // Mario
  struct {
    int16_t level = 0;
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
extern bool dialog, choosed;
extern uint16_t choose;

void openDialog(String title, LinkedList<String> answers);
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
  LinkedList<String> answers;
  LinkedList<Node*> actions;

  Dialog(String title)
    : Node(), title(title) {}

  Dialog* answer(String answer, Node* action = 0);
  virtual void run() override;
  virtual bool update() override;
};
#pragma endregion Nodes

void addThread(Node* head);
void update();
void save();
}

using Script::Dialog;
using ScriptNode = Script::Node;
extern LinkedList<ScriptNode*> scriptBank;
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
#pragma region StoryNeeds
const int MARIO_ATLAS_INDEX = 1;
namespace Mario {
extern TileEngine::Atlas* atlases;
void load();
}
#pragma endregion StoryNeeds
#pragma region Events
namespace Events::Easter {
void start();
}
#pragma endregion Events
