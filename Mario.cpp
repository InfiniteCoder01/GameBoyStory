#include <OreonBSSD1351Tile.hpp>
#include "00Names.hpp"
#include "data.hpp"

namespace Mario {
int nextLevel = -1;

#pragma region Story
namespace Scripts {
ScriptNode* princessScreaming;
ScriptNode* trainTraderStrangeThings;
void load() {
  princessScreaming = (new Dialog("Help!"))->answer("");
  trainTraderStrangeThings = (new Dialog("Can I help you?"))
                               ->answer("Have you seen or\nheard something\nstrange?", (new Dialog("Yes, I heard scream."))
                                                                                         ->answer("", (new Dialog("And then someone in\na raincoat came\nhere"))
                                                                                                        ->answer("", (new Dialog("And bought a\nticket to Toadtown."))
                                                                                                                       ->answer(""))));
}
}
#pragma endregion Story
#pragma region Components
enum class ComponentType : uint16_t {
  // System
  LevelTransition = (uint16_t)TileEngine::BuiltinComponents::Count,
  Serialize,
  // AI
  Mario
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
  }

  void deserialize(TileEngine::GameObject& object, File& file) override {
    setup(object);
  }

  bool collides(TileEngine::GameObject& object) {
    for (int32_t x = object.pos.x / 16; x < (object.pos.x + renderer->size().x) / 16; x++) {
      for (int32_t y = object.pos.y / 16; y < (object.pos.y + renderer->size().y) / 16; y++) {
        uint8_t tile = TileEngine::getTile(x, y);
        if (tile > 0 && colliders[(tile - 1) / 16] & (1 << (15 - (tile - 1) % 16))) return true;
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
  String name;

  Serialize(String name)
    : name(name) {}

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
    if (other.getComponent<Mario>((uint16_t)ComponentType::Mario)) {
      if (!buttonX.bPressed) return;
      nextLevel = index;
    }
    other.pos = (target + 1) * 16 - other.size;
  }

  uint16_t getType() override {
    return (uint16_t)ComponentType::LevelTransition;
  }
};

TileEngine::Component* createComponent(uint16_t type) {
  auto* component = TileEngine::createComponent(type);
  if (component) return component;
  if (type == (uint16_t)ComponentType::LevelTransition) return new LevelTransition(0, 0);
  if (type == (uint16_t)ComponentType::Serialize) return new Serialize("");
  if (type == (uint16_t)ComponentType::Mario) return new Mario();
  return nullptr;
}

TileEngine::Component* loadComponent(uint16_t type, const uint8_t*& data) {
  if (type == (uint16_t)ComponentType::LevelTransition) return new LevelTransition(TileEngine::loadLevelIndex(data), vec2i(TileEngine::loadInt(data), TileEngine::loadInt(data)));
  else if (type == (uint16_t)ComponentType::Serialize) return new Serialize(TileEngine::loadString(data));
  else if (type == (uint16_t)ComponentType::Mario) return new Mario();
  return nullptr;
}
#pragma endregion Components
#pragma region SaveAndLoad
void save() {
  if (g_State.mario.level < 0) return;
  if (!SD.exists(savepath + "/mario")) SD.mkdir(savepath + "/mario");
  File save = SD.open(format("%s/mario/level%d.lvl", savepath.c_str(), g_State.mario.level), FILE_WRITE);
  for (uint16_t i = 0; i < TileEngine::objects.size(); i++) {
    auto& object = TileEngine::objects[i];
    const auto* serialize = object.getComponent<Serialize>((uint16_t)ComponentType::Serialize);
    if (!serialize) continue;
    writestr(save, serialize->name);
  }
  writestr(save, "");
  for (uint16_t i = 0; i < TileEngine::objects.size(); i++) {
    auto& object = TileEngine::objects[i];
    const auto* serialize = object.getComponent<Serialize>((uint16_t)ComponentType::Serialize);
    if (!serialize) continue;
    write<vec2f>(save, object.pos);
    write<uint8_t>(save, object.components.size());
    for (uint8_t j = 0; j < object.components.size(); j++) {
      write<uint16_t>(save, object.components[i]->getType());
      object.components[j]->serialize(object, save);
    }
  }
  save.close();
}

void loadNextLevel() {
  if (nextLevel < 0) return;
  TileEngine::loadLevel(nextLevel);

  String filepath = format("%s/mario/level%d.lvl", savepath.c_str(), nextLevel);
  if (!SD.exists(filepath)) return;
  File save = SD.open(filepath, FILE_READ);
  while (true) {
    String name = readstr(save);
    Serial.println(name);
    break;
    if (name.isEmpty()) break;
    for (int i = 0; i < TileEngine::objects.size(); i++) {
      auto* serialize = TileEngine::objects[i].getComponent<Serialize>((uint16_t)ComponentType::Serialize);
      if (serialize && serialize->name == name) TileEngine::objects.remove(i--);
    }
  }
  // while (save.available()) {
  //   auto& object = TileEngine::spawn(read<vec2f>(save));
  //   auto nComponents = read<uint8_t>(save);
  //   for (uint8_t j = 0; j < nComponents; j++) {
  //     auto* component = createComponent(read<uint16_t>(save));
  //     component->deserialize(object, save);
  //     object.addComponent(component);
  //   }
  // }
  save.close();
  g_State.mario.level = nextLevel;
  nextLevel = -1;
}

void fileIO() {
  if (nextLevel != -1) {
    save();
    loadNextLevel();
    saveState();
  }
}
#pragma endregion SaveAndLoad
#pragma region MainLoop
void load() {
  TileEngine::load(data, loadComponent);
  Scripts::load();
}

void update() {
  if (nextLevel != -1) return;
  if (!UI::dialog) TileEngine::update();
}

void draw() {
  oled::fillScreen(0x04D6);
  if (nextLevel != -1) return;
  TileEngine::draw();
  if (UI::dialog) UI::drawDialog();
}

void start() {
  nextLevel = g_State.mario.level;
  g_State.mario.level = -1;

  game = Game(update, draw, save, fileIO);
}
#pragma endregion MainLoop
}
