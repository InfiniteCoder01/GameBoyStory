#include "OreonBSSD1351Tile.hpp"
#include "OreonBSSD1351.hpp"
#include "00Names.hpp"
#include "data.hpp"

namespace Mario {
int nextLevel = -1;
TileEngine::GameObject* mario;
void transfer(TileEngine::GameObject& object, vec2i target, uint16_t level);

#pragma region Story
namespace Story {
void useItem(const String& name) {
  if (name == "Coffee") g_State.mario.speedBuf = GState::Mario::Buf(10.f, 2.f);
  if (name == "Gold Coffee") {
    g_State.mario.speedBuf = GState::Mario::Buf(20.f, 2.5f);
    g_State.mario.jumpBuf = GState::Mario::Buf(20.f, 2.5f);
    g_State.mario.flipBuf = 20.f;
  }
}

void interact(String name) {
  if (::Story::dialogs.contains(name)) {
    Script::addThread(::Story::dialogs[name]);
  }

  if (name == "Coffee") UI::openShopDialog({ "Coffee (2$)", "Gold Coffee (2000$)", "Exit" }, 0, 2);
  if (name == "ToadShop") UI::openShopDialog("Welcome to my shop!", { "Coffee (2$)", "Gold Coffee (2000$)", "Exit" }, 0, 2);
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
  Interactible,
  Character
};

struct CharacterAnimator : public TileEngine::Component {
  TileEngine::AtlasRenderer* renderer;

  CharacterAnimator()
    : TileEngine::Component() {}

  void setup(TileEngine::GameObject& object) override {
    renderer = object.getComponent<TileEngine::AtlasRenderer>((uint16_t)TileEngine::BuiltinComponents::AtlasRenderer);
  }

  void update(vec2f motion) {
    if (motion.x != 0) {
      renderer->flip = motion.x < 0;
      renderer->frame += deltaTime * 16;
      if (renderer->frame >= 4) renderer->frame = 0;
    } else renderer->frame = 0;
    if (motion.y < 0) renderer->frame = 4;
  }
};

struct Character : public CharacterAnimator {
  vec2i target;

  Character(int x, int y)
    : CharacterAnimator(), target(x, y) {}

  void setup(TileEngine::GameObject& object) override {
    CharacterAnimator::setup(object);
    mario = &object;
  }

  void serialize(TileEngine::GameObject& object, File& file) override {
    write(file, target);
  }

  void deserialize(TileEngine::GameObject& object, File& file) override {
    target = read<vec2i>(file);
    setup(object);
  }

  void update(TileEngine::GameObject& object) override {
    if (target < 0) return;
    vec2f velocity = target * 16 - renderer->size() - object.pos;
    velocity.x = sign(velocity.x), velocity.y = sign(velocity.y);
    object.pos += velocity * deltaTime * 110.f;
    CharacterAnimator::update(velocity);
  }

  uint16_t getType() override {
    return (uint16_t)ComponentType::Character;
  }
};

struct Mario : public CharacterAnimator {
  vec2f velocity = 0;
  bool onGround = false;
  bool jumping = false, jumpPressed = false;
  uint32_t kayoteTime;

  Mario()
    : CharacterAnimator() {}

  void setup(TileEngine::GameObject& object) override {
    CharacterAnimator::setup(object);
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
    const float GRAVITY = 34.f, JUMP = -200.f, RUN = 110.f;
    const float ACCEL_RATE = 25.f, JUMP_CUT = 0.5f;
    const float JUMP_HANG_GRAVITY_MULTIPLIER = 0.5f, JUMP_HANG_ACCELERATION_MULTIPLIER = 1.5f;
    const uint32_t KAYOTE_TIME = 100;

    float speedMul = g_State.mario.speedBuf.multiplier, gravityMul = 1.f;

    object.size = TileEngine::atlases[MARIO_ATLAS_INDEX].size();

    if (onGround) kayoteTime = millis(), jumping = false;
    if (millis() - kayoteTime < KAYOTE_TIME && joy.y < 0) velocity.y = JUMP * g_State.mario.jumpBuf.multiplier, jumping = true, jumpPressed = true, gravityMul = 0.f;
    if (jumpPressed && joy.y >= 0) {  // Release jump
      jumpPressed = false;
      if (jumping) velocity.y *= JUMP_CUT;
    }
    if (jumping && abs(velocity.y) < 120.f) {  // Jump Hang
      speedMul *= JUMP_HANG_ACCELERATION_MULTIPLIER;
      gravityMul *= JUMP_HANG_GRAVITY_MULTIPLIER;
    }

    // Bufs
    if (g_State.mario.speedBuf.timer > 0.f) g_State.mario.speedBuf.timer -= deltaTime;
    else g_State.mario.speedBuf = GState::Mario::Buf();
    if (g_State.mario.jumpBuf.timer > 0.f) g_State.mario.jumpBuf.timer -= deltaTime;
    else g_State.mario.jumpBuf = GState::Mario::Buf();
    if (g_State.mario.flipBuf > 0.f) g_State.mario.flipBuf -= deltaTime;
    else g_State.mario.flipBuf = 0.f;

    float targetXVelocity = joy.x * RUN * speedMul;
    velocity.y += GRAVITY * gravityMul;

    if (g_State.mario.flipBuf > 0.f && jumping && abs(velocity.y) < 150.f) {  // Jump flip
      renderer->atlas = MARIO_ATLAS_INDEX + 1;
      renderer->frame = map(velocity.y, -150.f, 150.f, 0, renderer->getAtlas().frames);
    } else {
      renderer->atlas = MARIO_ATLAS_INDEX;
      CharacterAnimator::update(vec2f(joy.x, velocity.y));
    }

    velocity.x += (targetXVelocity - velocity.x) * ACCEL_RATE * deltaTime;
    if (abs(velocity.x) < 1.f) velocity.x = 0;

    // * Move
    while (collides(object)) object.pos.y -= 0.5;
    for (uint32_t i = 0; i < abs(velocity.x) * deltaTime * 2; i++) {
      object.pos.x += 0.5 * sign(velocity.x);
      if (collides(object)) {
        object.pos.x -= 0.5 * sign(velocity.x);
        velocity.x = 0;
        break;
      }
    }

    onGround = false;
    for (uint32_t i = 0; i < abs(velocity.y) * deltaTime * 2; i++) {
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
    if (!other.getComponent<Mario>((uint16_t)ComponentType::Mario)) return;
    if (!buttonX.bPressed) return;
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

Character* spawnCharacter(const String& name, vec2i pos) {
  uint16_t atlas;
  if (name == "Luigi") atlas = LUIGI_ATLAS_INDEX;
  auto* character = new Character(-1, -1);
  TileEngine::spawn(pos * 16 + 16 - TileEngine::atlases[atlas].size(), { new TileEngine::AtlasRenderer(atlas), new Serialize(), new Interactible(name), character });
  return character;
}

TileEngine::GameObject& findCharacter(const String& name) {
  for (auto* object : TileEngine::objects) {
    if (object->getComponent<Character>((uint16_t)ComponentType::Character)) {
      return *object;
    }
  }
}

TileEngine::Component* createComponent(uint16_t type) {
  auto* component = TileEngine::createComponent(type);
  if (component) return component;
  if (type == (uint16_t)ComponentType::LevelTransition) return new LevelTransition(0, 0);
  if (type == (uint16_t)ComponentType::Serialize) return new Serialize();
  if (type == (uint16_t)ComponentType::ScrollLock) return new ScrollLock(0);
  if (type == (uint16_t)ComponentType::Mario) return new Mario();
  if (type == (uint16_t)ComponentType::Interactible) return new Interactible("");
  if (type == (uint16_t)ComponentType::Character) return new Character(-1, -1);
  return nullptr;
}

TileEngine::Component* loadComponent(uint16_t type, const uint8_t*& data) {
  if (type == (uint16_t)ComponentType::LevelTransition) return new LevelTransition(TileEngine::loadLevelIndex(data), vec2i(TileEngine::loadInt(data), TileEngine::loadInt(data)));
  if (type == (uint16_t)ComponentType::Serialize) return new Serialize();
  if (type == (uint16_t)ComponentType::ScrollLock) return new ScrollLock(TileEngine::loadInt(data));
  if (type == (uint16_t)ComponentType::Mario) return new Mario();
  if (type == (uint16_t)ComponentType::Interactible) return new Interactible(TileEngine::loadString(data));
  if (type == (uint16_t)ComponentType::Character) return new Character(TileEngine::loadInt(data), TileEngine::loadInt(data));
  return nullptr;
}

String componentTypeToName(uint16_t type) {
  if (type == (uint16_t)TileEngine::BuiltinComponents::AtlasRenderer) return "AtlasRenderer";
  if (type == (uint16_t)ComponentType::LevelTransition) return "LevelTransition";
  if (type == (uint16_t)ComponentType::Serialize) return "Serialize";
  if (type == (uint16_t)ComponentType::ScrollLock) return "ScrollLock";
  if (type == (uint16_t)ComponentType::Mario) return "Mario";
  if (type == (uint16_t)ComponentType::Interactible) return "Interactible";
  if (type == (uint16_t)ComponentType::Character) return "Character";
  return "Unknown";
}

uint16_t componentNameToType(String name) {
  if (name == "AtlasRenderer") return (uint16_t)TileEngine::BuiltinComponents::AtlasRenderer;
  if (name == "LevelTransition") return (uint16_t)ComponentType::LevelTransition;
  if (name == "Serialize") return (uint16_t)ComponentType::Serialize;
  if (name == "ScrollLock") return (uint16_t)ComponentType::ScrollLock;
  if (name == "Mario") return (uint16_t)ComponentType::Mario;
  if (name == "Interactible") return (uint16_t)ComponentType::Interactible;
  if (name == "Character") return (uint16_t)ComponentType::Character;
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

void load() {
  TileEngine::load(data, loadComponent);
}

void update() {
  if (nextLevel != -1) return;
  if (!UI::dialog.active && UI::inventoryItem < 0) TileEngine::update();
  if (buttonY.bPressed) {
    if (savepath == "/saves/Dev") g_State.money = 2000;
    UI::toggleInventory();
  }
}

void draw() {
  oled::fillScreen(0x04D6);
  if (nextLevel != -1) return;
  TileEngine::draw();
  UI::draw();
}

void start() {
  nextLevel = g_State.mario.level;
  g_State.mario.level = -1;
  objectTransfer = Pair<TileEngine::GameObject*, uint16_t>(nullptr, 0);

  game = {
    .update = update,
    .draw = draw,
    .fileIO = fileIO,
    .save = save,
    .useItem = Story::useItem,
  };
}
#pragma endregion MainLoop
}

#pragma region StoryScripts
namespace Story {
static ScriptNode* Tito_GoToToadtown;
static ScriptNode* Luigi_Hello;
ScriptNode* Root_BeginningToLuigi;
void loadMario() {
  Root_BeginningToLuigi = new InvokeNode([]() {
    dialogs["Tito"] = Tito_GoToToadtown;
  });
  ((LinearNode*)root)
    ->add(new WaitLevel(9))
    ->add(new InvokeNode([]() {
      Mario::spawnCharacter("Luigi", vec2i(10, 7));
      dialogs["Luigi"] = Luigi_Hello;
    }));

  Tito_GoToToadtown = ([]() {
    Dialog* whatToDo = new Dialog("");
    auto* dialog =
      (new Dialog("Hey, Mario!\n"
                  "Do you know, what\n"
                  "is this rocky mess\n"
                  "at the edges of the\n"
                  "kingdom?"))
        ->answer("Yes, I do!")
        ->answer("No, I don't.")
        ->action(
          (new Dialog("Relly?\n"
                      "Tell me, please!"))
            ->answer("I've lied, I don't\n"
                     "actually know, what\n"
                     "this is...")
            ->action(whatToDo))
        ->action(whatToDo);

    // ******************************
    whatToDo->title = "It's growing.\n"
                      "I was forced to\n"
                      "remove my crops.";
    whatToDo
      ->answer("We need to weed it\n"
               "out!")
      ->action(
        (new Dialog("But how will you do\n"
                    "it? We don't even\n"
                    "have a mower!"))
          ->answer("I'll go to a\n"
                   "Toadtown and buy\n"
                   "everything we\n"
                   "might need.")
          ->action(
            (new Dialog("Great! I'll call\n"
                        "Luigi and book\n"
                        "a roundtrip ticket\n"
                        "for you. Your\n"
                        "seat is left."))
              ->answer("")
              ->action(nullptr)));

    return dialog;
  })();

  Luigi_Hello = ([]() {
    auto* dialog =
      (new Dialog("Hey, Mario!\n"
                  "Tito told me\n"
                  "about the rocky\n"
                  "mess. We need to\n"
                  "stop it!"))
        ->answer("Let's go buy a mower!")
        ->action(new InvokeNode([]() {
          Mario::findCharacter("Luigi").getComponent<Mario::Character>((uint16_t)Mario::ComponentType::Character)->target = vec2i(0, 7);
        }));

    ((LinearNode*)dialog->actions[0])
      ->add(new UnimplementedScriptNode());

    return dialog;
  })();
}
}
#pragma endregion StoryScripts