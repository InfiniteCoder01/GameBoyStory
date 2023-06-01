#include "OreonBSSD1351.hpp"
#include "OreonBSSD1351Gui.hpp"
#include <initializer_list>
#include "00Names.hpp"

namespace UI {
const int DIALOG_PADDING = 10, TEXT_PADDING = 3;
const uint16_t BORDER_COLORS[] = { 0x3102, 0x4983, 0x49A4 };
const uint16_t TEXT_COLOR = 0x6AEA;

void drawCanvas() {
  oled::fillRect(DIALOG_PADDING, oled::size() - DIALOG_PADDING * 2, 0xE5D2);
  for (int layer = 1; layer <= 3; layer++) {
    vec2i tl = DIALOG_PADDING, tr = vec2i(oled::width - DIALOG_PADDING - 1, DIALOG_PADDING), bl = vec2i(DIALOG_PADDING, oled::height - DIALOG_PADDING - 1), br = vec2i(tr.x, bl.y);
    oled::drawFastHLine(tl - vec2i(0, layer), tr.x - tl.x + 1, BORDER_COLORS[layer - 1]);
    oled::drawFastHLine(bl + vec2i(0, layer), tr.x - tl.x + 1, BORDER_COLORS[layer - 1]);
    oled::drawFastVLine(tl - vec2i(layer, 0), bl.y - tl.y + 1, BORDER_COLORS[layer - 1]);
    oled::drawFastVLine(tr + vec2i(layer, 0), bl.y - tl.y + 1, BORDER_COLORS[layer - 1]);
    oled::drawLine(tl - vec2i(1, layer), tl - vec2i(layer, 1), BORDER_COLORS[layer - 1]);
    oled::drawLine(tr - vec2i(-1, layer), tr + vec2i(layer, -1), BORDER_COLORS[layer - 1]);
    oled::drawLine(br + vec2i(1, layer), br + vec2i(layer, 1), BORDER_COLORS[layer - 1]);
    oled::drawLine(bl + vec2i(-1, layer), bl - vec2i(layer, -1), BORDER_COLORS[layer - 1]);
  }
}

void setCanvasText() {
  oled::pageX = DIALOG_PADDING + TEXT_PADDING, oled::pageR = oled::width - DIALOG_PADDING - TEXT_PADDING;
  oled::setCursor(0, DIALOG_PADDING + TEXT_PADDING);
  oled::setTextColor(TEXT_COLOR);
  oled::setFont(font5x7);
}

void resetText() {
  oled::pageX = 0, oled::pageR = oled::width;
  oled::setTextColor(WHITE);
  oled::setFont(font8x12);
}

#pragma region Dialog
Dialog::Dialog(const String& title, const Vector<String>& answers)
  : title(title), answers(answers) {
  active = true;
}

Dialog::Dialog(const String& title, const String* answers, uint32_t count)
  : title(title), answers(answers, answers + count) {
  active = true;
}

void Dialog::draw() {
  if (!active) return;

  drawCanvas();
  setCanvasText();
  if (bJoyMoved) choice = wrap(choice + joy.y, answers.size());
  if (buttonX.bPressed) {
    choosed = true;
    active = false;
  }

  if (!title.isEmpty()) oled::println(title);
  if (answers.size() > 0) {
    if (!title.isEmpty()) oled::write('\n');
    for (size_t i = 0; i < answers.size(); i++) {
      oled::write(i == choice ? '>' : ' ');
      oled::println(answers[i]);
    }
  }

  resetText();
}

Dialog dialog;
#pragma endregion Dialog
#pragma region ShopAndInventory
// * Shop
static uint32_t itemsOffset = 0, itemCount = 0;
void openShopDialog(const String& title, std::initializer_list<String> options, uint32_t itemsOffset, uint32_t itemCount) {
  dialog = Dialog(title, options);
  UI::itemsOffset = itemsOffset;
  UI::itemCount = itemCount;
}

static void updateShop() {
  if (itemCount <= 0 || !UI::dialog.choosed) return;
  buttonX.bPressed = false;

  if (UI::dialog.choice < itemCount) {
    uint32_t choice = UI::dialog.choice + itemsOffset;
    if (g_State.money < shopPrices[choice]) UI::sendMessage("Mario", "I'm moneyless!");
    else {
      g_State.money -= shopPrices[choice];
      g_State.inventory[shopItems[choice]]++;
      if (game.buyItem) game.buyItem(shopItems[choice]);
    }
  }
  itemCount = 0;
}

// * Inventory
int32_t inventoryItem = -1;
void toggleInventory() {
  inventoryItem = inventoryItem < 0 ? 0 : -1;
}

static void drawInventory() {
  if (inventoryItem < 0) return;
  UI::drawCanvas();
  UI::setCanvasText();

  if (g_State.inventory.empty()) {
    oled::println("Nothing is here yet.");
    UI::resetText();
    return;
  }

  uint32_t width = (oled::pageR - oled::pageX) / 11;
  if (bJoyMoved) inventoryItem = wrap(inventoryItem + joy.x + joy.y * width, g_State.inventory.size());

  auto& highlightedItem = g_State.inventory.begin()[inventoryItem];
  oled::println(highlightedItem.first + " x" + highlightedItem.second);
  if (buttonX.bReleased) {
    if (game.useItem) game.useItem(highlightedItem.first);
    highlightedItem.second--;
    if (highlightedItem.second <= 0) g_State.inventory.erase(inventoryItem);
  }

  uint32_t index = 0;
  vec2i redBox;
  for (auto& item : g_State.inventory) {
    vec2i tl = oled::getCursor() + vec2u(oled::pageX, 0);

    // * Draw Items
    if (item.first == "Coffee") TileEngine::drawTileFromAtlas(tl + TileEngine::camera, 0, 0);
    else if (item.first == "Gold Coffee") TileEngine::drawTileFromAtlas(tl + TileEngine::camera, 1, 0);

    if (index == inventoryItem) redBox = tl;
    oled::cursorX += 11;
    if (index % width == width - 1) oled::cursorX = 0, oled::cursorY += 11;
    index++;
  }
  oled::drawRect(redBox - 1, 11, RED);
  UI::resetText();
}

#pragma endregion ShopAndInventory
#pragma region Chat
struct ChatMessage {
  String author, message;
  uint64_t time;
};

Vector<ChatMessage> chat;

void sendMessage(String author, String message) {
  chat.push_back(ChatMessage{ .author = author, .message = message, .time = millis() });
}

static void drawChat() {
  uint16_t chatHeight = 0;
  for (int i = 0; i < chat.size(); i++) {
    uint16_t index = 0;
    while (chat[i].message.indexOf('\n', index) != -1) {
      index = chat[i].message.indexOf('\n', index) + 1;
      chatHeight++;
    }
    chatHeight++;
  }

  oled::setCursor(0, oled::height - chatHeight * (oled::getCharHeight() - 4) - 1);
  gui::darkenRect(oled::cursorX, oled::cursorY - 1, oled::width, oled::height - oled::cursorY + 1, 70);
  oled::setFont(font5x7);
  for (int i = 0; i < chat.size(); i++) {
    oled::setTextColor(0x2ddf);
    oled::print(format("[%s] ", chat[i].author.c_str()));
    oled::setTextColor(WHITE);
    oled::println(chat[i].message);
    if (millis() - chat[i].time > chat[i].message.length() * 150) chat.erase(i--);
  }
  oled::setTextColor(WHITE);
  oled::setFont(font8x12);
}
#pragma endregion Chat

void draw() {
  drawChat();
  oled::setTextColor(MAGENTA);
  gui::drawFPS();
  oled::println(format("%zu$", g_State.money));
  oled::setTextColor(WHITE);
  dialog.draw();
  drawInventory();
  updateShop();
}
}