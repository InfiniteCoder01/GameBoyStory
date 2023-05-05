#include "OreonBSSD1351.hpp"
#include "00Names.hpp"
#include "data.hpp"

namespace Mario {
int nextLevel = -1;
void transfer(TileEngine::GameObject& object, vec2i target, uint16_t level);
TileEngine::GameObject* mario;

#pragma region Story
namespace Story {
uint32_t shopStart = 0, shopLength = 0;

ScriptNode* Tito_GoToToadtown;
void load() {
  {
    auto whatToDo = (new Dialog("It's growing.\nI was forced to\nremove my crops."))
                      ->answer("We need to weed it\nout!",
                               (new Dialog("But how will you do\nit? We don't even\nhave a mower!"))
                                 ->answer("I'll go to a\nToadtown and buy\neverything we\nmight need.",
                                          (new Dialog("Great! I'll call\nLuigi and book\na roundtrip ticket\nfor you. Your\nseat is left."))
                                            ->answer("")));

    Tito_GoToToadtown = (new Dialog("Hey, Mario!\nDo you know, what\nis this rocky mess\nat the edges of the\nkingdom?"))
                          ->answer("Yes, I do!", (new Dialog("Relly?\nTell me, please!"))->answer("I've lied, I don't\nactually know, what\nthis is...", whatToDo))
                          ->answer("No, I don't.", whatToDo);
  }
}

void update() {
  if (shopLength > 0 && UI::dialog.choosed) {
    buttonX.bPressed = false;

    if (UI::dialog.choice < shopLength) {
      uint32_t choice = UI::dialog.choice + shopStart;
      if (g_State.mario.balance < shopPrices[choice]) UI::sendMessage("Mario", "I'm moneyless!");
      else {
        g_State.mario.balance -= shopPrices[choice];
        g_State.mario.inventory[shopItems[choice]]++;
      }
    }
    shopLength = 0;
  }
}

void interact(String name) {
  if (name == "Tito") {
    if (g_State.mario.state == GState::GetMower) Script::addThread(Tito_GoToToadtown);
  }

  if (name == "Train") {
    if (g_State.mario.state == GState::GetMower) transfer(*mario, vec2i(12, 6), 4);
  }

  if (name == "Coffee") UI::dialog = UI::Dialog("", { "Coffee (2$)", "Gold Coffee (200$)", "Exit" }), shopStart = 0, shopLength = 2;
  if (name == "ToadShop") UI::dialog = UI::Dialog("Welcome to my shop!", { "Coffee (2$)", "Gold Coffee (200$)", "Exit" }), shopStart = 0, shopLength = 2;
}
}
#pragma endregion Story
#pragma region Components
enum class ComponentType : uint16_t {
  // System
  LevelTransition = (uint16_t)TileEngine::BuiltinComponents::Count,
  Serialize,
  ScrollLock,
  // AI
  Mario,
  Interactible
};

struct Mario : public TileEngine::Component {
  vec2f velocity = 0;
  bool onGround = false;
  bool jumping = false, jumpPressed = false;
  uint32_t kayoteTime;

  TileEngine::AtlasRenderer* renderer;

  Mario()
    : TileEngine::Component() {}

  void setup(TileEngine::GameObject& object) override {
    renderer = object.getComponent<TileEngine::AtlasRenderer>((uint16_t)TileEngine::BuiltinComponents::AtlasRenderer);
    renderer->atlas = MARIO_ATLAS_INDEX;
    mario = &object;
  }

  void deserialize(TileEngine::GameObject& object, File& file) override {
    setup(object);
  }

  bool collides(TileEngine::GameObject& object) {
    for (int32_t x = object.pos.x / 16; x < (object.pos.x + renderer->size().x) / 16; x++) {
      for (int32_t y = object.pos.y / 16; y < (object.pos.y + renderer->size().y) / 16; y++) {
        uint8_t tile = TileEngine::getTile(x, y);
        if (tile > 0 && colliders[TileEngine::level->tileset - 1][(tile - 1) / 8] & (1 << (7 - (tile - 1) % 8))) return true;
      }
    }
    return !inRange(object.pos, 0, TileEngine::level->size() * 16 - object.size);
  }

  void update(TileEngine::GameObject& object) override {
    const float GRAVITY = 1.7f, JUMP = -10.f, RUN = 4.f;
    const float ACCEL_RATE = 10.f, JUMP_CUT = 0.5f;
    const float JUMP_HANG_GRAVITY_MULTIPLIER = 0.5f, JUMP_HANG_ACCELERATION_MULTIPLIER = 1.5f;
    const uint32_t KAYOTE_TIME = 100;

    float speedMul = 1.f, gravityMul = 1.f;

    if (onGround) kayoteTime = millis(), jumping = false;
    if (millis() - kayoteTime < KAYOTE_TIME && joy.y < 0) velocity.y = JUMP, jumping = true, jumpPressed = true, gravityMul = 0.f;
    if (jumpPressed && joy.y >= 0) {  // Release jump
      jumpPressed = false;
      if (jumping) velocity.y *= JUMP_CUT;
    }
    if (jumping && abs(velocity.y) < 0.3) {  // Jump Hang
      speedMul *= JUMP_HANG_ACCELERATION_MULTIPLIER;
      gravityMul *= JUMP_HANG_GRAVITY_MULTIPLIER;
    }

    float targetXVelocity = joy.x * RUN * speedMul;
    velocity.y += GRAVITY * gravityMul;
    if (joy.x != 0) {
      renderer->flip = joy.x < 0;
      renderer->frame += deltaTime * 16;
      if (renderer->frame >= 4) renderer->frame = 0;
    } else renderer->frame = 0;
    if (velocity.y < 0) renderer->frame = 4;
    velocity.x += (targetXVelocity - velocity.x) * ACCEL_RATE * deltaTime;
    if (abs(velocity.x) < 0.05) velocity.x = 0;

    // * Move
    while (collides(object)) object.pos.y -= 0.5;
    for (uint32_t i = 0; i < abs(velocity.x) * 2; i++) {
      object.pos.x += 0.5 * sign(velocity.x);
      if (collides(object)) {
        object.pos.x -= 0.5 * sign(velocity.x);
        velocity.x = 0;
        break;
      }
    }

    onGround = false;
    for (uint32_t i = 0; i < abs(velocity.y) * 2; i++) {
      object.pos.y += 0.5 * sign(velocity.y);
      if (collides(object)) {
        object.pos.y -= 0.5 * sign(velocity.y);
        onGround = velocity.y > 0;
        velocity.y = 0;
        break;
      }
    }

    TileEngine::camera = min<int>(max<int>(object.pos + renderer->size() / 2 - oled::size() / 2, 0), TileEngine::level->size() * 16 - oled::size());
  }

  uint16_t getType() override {
    return (uint16_t)ComponentType::Mario;
  }
};

struct Serialize : public TileEngine::Component {
  Serialize() {}

  uint16_t getType() override {
    return (uint16_t)ComponentType::Serialize;
  }
};

struct LevelTransition : public TileEngine::Component {
  uint32_t index = 0;
  vec2i target;

  LevelTransition(uint32_t index, vec2i target)
    : TileEngine::Component(), index(index), target(target) {
  }

  void collide(TileEngine::GameObject& object, TileEngine::GameObject& other) override {
    if (!other.getComponent<Serialize>((uint16_t)ComponentType::Serialize)) return;
    if (other.getComponent<Mario>((uint16_t)ComponentType::Mario) && !buttonX.bPressed) return;
    transfer(other, target, index);
  }

  uint16_t getType() override {
    return (uint16_t)ComponentType::LevelTransition;
  }
};

struct ScrollLock : public TileEngine::Component {
  uint16_t width;

  ScrollLock(uint16_t width)
    : width(width) {}

  void setup(TileEngine::GameObject& object) override {
    object.size = vec2u(width * 16, TileEngine::level->height * 16);
  }

  void collide(TileEngine::GameObject& object, TileEngine::GameObject& other) override {
    if (!other.getComponent<Mario>((uint16_t)ComponentType::Mario)) return;
    if (!inRangeW(other.pos.x + other.size.x / 2.f, object.pos.x, (float)object.size.x)) return;
    TileEngine::camera = VectorMath::max(VectorMath::min(TileEngine::camera, vec2i(object.pos + object.size - oled::size())), vec2i(object.pos));
  }


  uint16_t getType() override {
    return (uint16_t)ComponentType::ScrollLock;
  }
};

struct Interactible : public TileEngine::Component {
  String name;

  Interactible(String name)
    : name(name) {}

  void collide(TileEngine::GameObject& object, TileEngine::GameObject& other) override {
    if (!other.getComponent<Mario>((uint16_t)ComponentType::Mario) || !buttonX.bPressed) return;
    buttonX.bPressed = false;
    Story::interact(name);
  }

  void serialize([[maybe_unused]] TileEngine::GameObject& object, File& file) override {
    writestr(file, name);
  }

  void deserialize([[maybe_unused]] TileEngine::GameObject& object, File& file) override {
    name = readstr(file);
  }

  uint16_t getType() override {
    return (uint16_t)ComponentType::Interactible;
  }
};

TileEngine::Component* createComponent(uint16_t type) {
  auto* component = TileEngine::createComponent(type);
  if (component) return component;
  if (type == (uint16_t)ComponentType::LevelTransition) return new LevelTransition(0, 0);
  if (type == (uint16_t)ComponentType::Serialize) return new Serialize();
  if (type == (uint16_t)ComponentType::ScrollLock) return new ScrollLock(0);
  if (type == (uint16_t)ComponentType::Mario) return new Mario();
  if (type == (uint16_t)ComponentType::Interactible) return new Interactible("");
  return nullptr;
}

TileEngine::Component* loadComponent(uint16_t type, const uint8_t*& data) {
  if (type == (uint16_t)ComponentType::LevelTransition) return new LevelTransition(TileEngine::loadLevelIndex(data), vec2i(TileEngine::loadInt(data), TileEngine::loadInt(data)));
  if (type == (uint16_t)ComponentType::Serialize) return new Serialize();
  if (type == (uint16_t)ComponentType::ScrollLock) return new ScrollLock(TileEngine::loadInt(data));
  if (type == (uint16_t)ComponentType::Mario) return new Mario();
  if (type == (uint16_t)ComponentType::Interactible) return new Interactible(TileEngine::loadString(data));
  return nullptr;
}

String componentTypeToName(uint16_t type) {
  if (type == (uint16_t)TileEngine::BuiltinComponents::AtlasRenderer) return "AtlasRenderer";
  if (type == (uint16_t)ComponentType::LevelTransition) return "LevelTransition";
  if (type == (uint16_t)ComponentType::Serialize) return "Serialize";
  if (type == (uint16_t)ComponentType::ScrollLock) return "ScrollLock";
  if (type == (uint16_t)ComponentType::Mario) return "Mario";
  if (type == (uint16_t)ComponentType::Interactible) return "Interactible";
  return "Unknown";
}

uint16_t componentNameToType(String name) {
  if (name == "AtlasRenderer") return (uint16_t)TileEngine::BuiltinComponents::AtlasRenderer;
  if (name == "LevelTransition") return (uint16_t)ComponentType::LevelTransition;
  if (name == "Serialize") return (uint16_t)ComponentType::Serialize;
  if (name == "ScrollLock") return (uint16_t)ComponentType::ScrollLock;
  if (name == "Mario") return (uint16_t)ComponentType::Mario;
  if (name == "Interactible") return (uint16_t)ComponentType::Interactible;
  return 0xffff;
}
#pragma endregion Components
#pragma region SaveAndLoad
Pair<TileEngine::GameObject*, uint16_t> objectTransfer;

void transfer(TileEngine::GameObject& object, vec2i target, uint16_t level) {
  if (object.getComponent<Mario>((uint16_t)ComponentType::Mario)) nextLevel = level;
  object.pos = (target + 1) * 16 - object.size;
  objectTransfer = Pair<TileEngine::GameObject*, uint16_t>(&object, level);
}

void save() {
  if (g_State.mario.level < 0) return;
  if (!SD.exists(savepath + "/mario")) SD.mkdir(savepath + "/mario");
  File save = SD.open(format("%s/mario/level%d.lvl", savepath.c_str(), g_State.mario.level), FILE_WRITE);
  for (uint16_t i = 0; i < TileEngine::objects.size(); i++) {
    auto& object = *TileEngine::objects[i];
    const auto* serialize = object.getComponent<Serialize>((uint16_t)ComponentType::Serialize);
    if (!serialize) continue;
    write<vec2f>(save, object.pos);
    write<uint8_t>(save, object.components.size());
    for (uint8_t j = 0; j < object.components.size(); j++) {
      writestr(save, componentTypeToName(object.components[j]->getType()));
      object.components[j]->serialize(object, save);
    }
  }
  save.close();
}

void loadNextLevel() {
  g_State.mario.level = nextLevel;
  nextLevel = -1;

  TileEngine::loadLevel(g_State.mario.level);

  String filepath = format("%s/mario/level%d.lvl", savepath.c_str(), g_State.mario.level);
  if (!SD.exists(filepath)) return;
  File save = SD.open(filepath, FILE_READ);
  for (int i = 0; i < TileEngine::objects.size(); i++) {
    if (TileEngine::objects[i]->getComponent<Serialize>((uint16_t)ComponentType::Serialize)) {
      delete TileEngine::objects[i];
      TileEngine::objects.erase(i--);
    }
  }
  while (save.available()) {
    auto& object = TileEngine::spawn(read<vec2f>(save));
    auto nComponents = read<uint8_t>(save);
    for (uint8_t j = 0; j < nComponents; j++) {
      auto* component = createComponent(componentNameToType(readstr(save)));
      component->deserialize(object, save);
      object.addComponent(component);
    }
  }
  save.close();
}

void fileIO() {
  if (objectTransfer.first) {
    for (uint16_t i = 0; i < TileEngine::objects.size(); i++) {
      if (objectTransfer.first == TileEngine::objects[i]) {
        TileEngine::objects.erase(i);
        break;
      }
    }
    File save = SD.open(format("%s/mario/level%d.lvl", savepath.c_str(), objectTransfer.second), FILE_APPEND);
    write<vec2f>(save, objectTransfer.first->pos);
    write<uint8_t>(save, objectTransfer.first->components.size());
    for (uint8_t j = 0; j < objectTransfer.first->components.size(); j++) {
      writestr(save, componentTypeToName(objectTransfer.first->components[j]->getType()));
      objectTransfer.first->components[j]->serialize(*objectTransfer.first, save);
    }
    delete objectTransfer.first;
    objectTransfer.first = nullptr;
    saveState();
  }

  if (nextLevel != -1) {
    save();
    loadNextLevel();
    saveState();
  }
}
#pragma endregion SaveAndLoad
#pragma region MainLoop
int32_t inventoryItem = -1;

void load() {
  TileEngine::load(data, loadComponent);
  Story::load();
}

void update() {
  if (nextLevel != -1) return;
  if (!UI::dialog.active && inventoryItem < 0) TileEngine::update();
  Story::update();
  if (buttonY.bPressed) {
    if (savepath == "/saves/Dev") g_State.mario.balance = 2000;
    inventoryItem = inventoryItem < 0 ? 0 : -1;
  }
}

void draw() {
  oled::fillScreen(0x04D6);
  if (nextLevel != -1) return;
  TileEngine::draw();
  oled::setTextColor(MAGENTA);
  gui::textAt(format("%d$", g_State.mario.balance), 0, 0);
  oled::setTextColor(WHITE);
  UI::dialog.draw();
  if (inventoryItem >= 0) {
    UI::drawCanvas();
    UI::setCanvasText();

    uint32_t width = (oled::pageR - oled::pageX) / 11;
    if (bJoyMoved && !g_State.mario.inventory.empty()) inventoryItem = wrap(inventoryItem + joy.x + joy.y * width, g_State.mario.inventory.size());

    if (g_State.mario.inventory.empty()) oled::println("Nothing is here yet.");
    else {
      auto& highlightedItem = g_State.mario.inventory.begin()[inventoryItem];
      oled::println(highlightedItem.first + " x" + highlightedItem.second);
    }

    uint32_t index = 0;
    vec2i redBox;
    for (auto& item : g_State.mario.inventory) {
      vec2i tl = oled::getCursor() + vec2u(oled::pageX, 0);

      if (item.first == "Coffee") TileEngine::drawTileFromAtlas(tl + TileEngine::camera, 0, 0);
      else if (item.first == "Gold Coffee") TileEngine::drawTileFromAtlas(tl + TileEngine::camera, 1, 0);

      if (index == inventoryItem) redBox = tl;
      oled::cursorX += 11;
      if (index % width == width - 1) oled::cursorX = 0, oled::cursorY += 11;
      index++;
    }
    if (!g_State.mario.inventory.empty()) oled::drawRect(redBox - 1, 11, RED);
    UI::resetText();
  }
}

void start() {
  nextLevel = g_State.mario.level;
  g_State.mario.level = -1;
  objectTransfer = Pair<TileEngine::GameObject*, uint16_t>(nullptr, 0);

  game = Game(update, draw, save, fileIO);
}
#pragma endregion MainLoop
}
