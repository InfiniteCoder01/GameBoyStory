#include "00Names.hpp"

namespace UI {
const int DIALOG_PADDING = 10, TEXT_PADDING = 3;
const uint16_t BORDER_COLORS[] = { 0x3102, 0x4983, 0x49A4 };
const uint16_t TEXT_COLOR = 0x6AEA;

bool dialog = false, choosed = false;
uint16_t choose;
String title;
LinkedList<String> answers;

struct ChatMessage {
  String author, message;
  uint32_t time;
};

LinkedList<ChatMessage> chat;

void openDialog(String title, LinkedList<String> answers) {
  dialog = true, choosed = false;
  choose = 0;
  UI::answers.clear();
  UI::title = title;
  for (int i = 0; i < answers.size(); i++) UI::answers.add(answers[i]);
}

void drawDialog() {
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

  oled::pageX = DIALOG_PADDING + TEXT_PADDING, oled::pageR = oled::width - DIALOG_PADDING - TEXT_PADDING;
  oled::setCursor(0, DIALOG_PADDING + TEXT_PADDING);
  oled::setTextColor(TEXT_COLOR);
  oled::setFont(font5x7);

  if (bJoyMoved) choose = wrap(choose + joy.y, answers.size());
  if (buttonX.bPressed) choosed = true;

  oled::println(title);
  if (answers.size() > 0) {
    oled::write('\n');
    for (int i = 0; i < answers.size(); i++) {
      oled::write(i == choose ? '>' : ' ');
      oled::println(answers[i]);
    }
  }

  oled::pageX = 0, oled::pageR = oled::width;
  oled::setTextColor(WHITE);
  oled::setFont(font8x12);
}

void sendMessage(String author, String message) {
  chat.add(ChatMessage{ .author = author, .message = message, .time = millis() });
}

void drawChat() {
  uint16_t chatHeight;
  for (int i = 0; i < chat.size(); i++) {
    uint16_t index = 0;
    while (chat[i].message.indexOf('\n', index) != -1) {
      index = chat[i].message.indexOf('\n', index) + 1;
      chatHeight++;
    }
    chatHeight++;
  }
  oled::setCursor(0, oled::height - chatHeight * oled::getCharHeight());
  for (int i = 0; i < chat.size(); i++) {
    oled::setTextColor(0x35df);
    oled::print(format("[%s] ", chat[i].author.c_str()));
    oled::setTextColor(WHITE);
    oled::println(chat[i].message);
    if (millis() - chat[i].time > chat[i].message.length() * 150) chat.remove(i--);
  }
}
}