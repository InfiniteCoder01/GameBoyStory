#include "00Names.hpp"

// TODO: What is after this line
namespace Tetris {
const int FIELD_WIDTH = 10, FIELD_HEIGHT = 20, BLOCK_SIZE = 6;
const vec2i FIELD_OFFSET = vec2i(64 - FIELD_WIDTH * BLOCK_SIZE / 2, 64 - FIELD_HEIGHT * BLOCK_SIZE / 2);
const vec2f TETROMINOES[][4] = {
  { vec2f(-1.5, -0.5), vec2f(-0.5, -0.5), vec2f(0.5, -0.5), vec2f(1.5, -0.5) },
  { vec2f(-1, -1), vec2f(-1, 0), vec2f(0, 0), vec2f(1, 0) },
  { vec2f(-1, 0), vec2f(0, 0), vec2f(1, 0), vec2f(1, -1) },
  { vec2f(-0.5, -0.5), vec2f(0.5, -0.5), vec2f(-0.5, 0.5), vec2f(0.5, 0.5) },
  { vec2f(-1, 0), vec2f(0, 0), vec2f(0, -1), vec2f(1, -1) },
  { vec2f(-1, 0), vec2f(0, 0), vec2f(1, 0), vec2f(0, -1) },
  { vec2f(-1, -1), vec2f(0, -1), vec2f(0, 0), vec2f(1, 0) },
};
uint16_t field[FIELD_WIDTH * FIELD_HEIGHT];
uint8_t clearLines[4];
uint32_t gameOverAnimationTimer, clearAnimationTimer;
uint16_t score, timeout;
void start();
bool line(uint8_t y);

struct Tetromino {
  uint8_t type = 0;
  uint8_t rotation = 0;
  vec2i pos = 0;
  uint16_t color;

  Tetromino() = default;
  Tetromino(vec2i pos, uint8_t type, uint16_t color)
    : type(type), pos(pos), color(color) {}

  void randomize() {
    const uint16_t colors[] = { 0x0215, 0x7647, 0xFEA0, 0xFCA3, 0xF982 };
    type = random(7);
    rotation = 0;
    pos = vec2i(FIELD_WIDTH / 2 - 1, 0);
    for (const auto& tile : TETROMINOES[type]) pos.y = max(-rotate(tile).y, pos.y);
    color = colors[random(sizeof(colors) / sizeof(colors[0]))];
    if (!fit()) gameOverAnimationTimer = millis();
  }

  vec2i rotate(vec2f tile) {
    vec2f offset = type == 0 || type == 3 ? 0.5 : 0;
    if (rotation == 1) return vec2f(-tile.y, tile.x) + offset;
    else if (rotation == 2) return -tile + offset;
    else if (rotation == 3) return vec2f(tile.y, -tile.x) + offset;
    return tile + offset;
  }

  void drawRaw() {
    for (const auto& tile : TETROMINOES[type]) {
      oled::fillRect((pos + rotate(tile)) * BLOCK_SIZE + FIELD_OFFSET, BLOCK_SIZE - 1, color);
    }
  }

  void draw() {
    drawRaw();
    vec2i lastPos = pos;
    while (fit()) pos.y++;
    pos.y--;
    for (const auto& tile : TETROMINOES[type]) {
      oled::drawRect((pos + rotate(tile)) * BLOCK_SIZE + FIELD_OFFSET, BLOCK_SIZE - 1, color);
    }
    pos = lastPos;
  }

  bool fit() {
    for (const auto& tile : TETROMINOES[type]) {
      vec2i point = pos + rotate(tile);
      if (point.x < 0 || point.x >= FIELD_WIDTH || point.y < 0 || point.y >= FIELD_HEIGHT) return false;
      if (field[point.x + point.y * FIELD_WIDTH]) return false;
    }
    return true;
  }

  void place() {
    for (const auto& tile : TETROMINOES[type]) {
      vec2i point = rotate(tile) + pos;
      field[point.x + point.y * FIELD_WIDTH] = color;
    }
    int lineN = 0;
    for (int y = FIELD_HEIGHT - 1; y >= 0; y--) {
      if (line(y)) {
        clearAnimationTimer = millis();
        clearLines[lineN++] = y;
      }
    }
    while (lineN < 4) clearLines[lineN++] = 255;
  }

  bool moveRaw(vec2i vel) {
    pos += vel;
    if (!fit()) {
      pos -= vel;
      return true;
    }
    return false;
  }

  bool move(vec2i vel) {
    if (moveRaw(vel)) {
      if (vel.y != 0) {
        place();
        randomize();
      }
      return true;
    }
    return false;
  }

  void rotate() {
    rotation = (rotation + 1) % 4;
    if (!fit()) rotation = rotation == 0 ? 3 : rotation - 1;
  }
};

Tetromino tetromino;
uint32_t fallTimer, moveTimer;

bool line(uint8_t y) {
  for (int x = 0; x < FIELD_WIDTH; x++) {
    if (!field[x + y * FIELD_WIDTH]) return false;
  }
  return true;
}

void checkClearLines() {
  static bool clear = false;
  if (clearAnimationTimer) {
    for (auto line : clearLines) {
      if (line == 255) break;
      for (int x = 0; x < FIELD_WIDTH / 2; x++) {
        int dist = FIELD_WIDTH / 2 - (millis() - clearAnimationTimer) * FIELD_WIDTH / 2 / 300;
        field[x + line * FIELD_WIDTH] = field[FIELD_WIDTH - x - 1 + line * FIELD_WIDTH] = x < dist ? WHITE : BLACK;
      }
    }
    if (millis() - clearAnimationTimer > 300) clearAnimationTimer = 0;
    clear = true;
    return;
  }
  if (!clear) return;
  clear = false;
  int row = 0, lineN = 0;
  for (int y = FIELD_HEIGHT - 1; y >= 0; y--) {
    if (clearLines[lineN] == y) row++, lineN++;
    else if (row != 0 && y < FIELD_HEIGHT - 1) {
      for (int x = 0; x < FIELD_WIDTH; x++) field[x + (y + row) * FIELD_WIDTH] = field[x + y * FIELD_WIDTH];
    }
  }
  if (row) score += 1 << row;
  timeout = max(timeout - 50, 100);
}

void drawField(bool border = true) {
  if (border) oled::drawRect(FIELD_OFFSET - 2, vec2i(FIELD_WIDTH, FIELD_HEIGHT) * BLOCK_SIZE + 2, BLACK);
  for (int x = 0; x < FIELD_WIDTH; x++) {
    for (int y = 0; y < FIELD_HEIGHT; y++) {
      float k = gameOverAnimationTimer ? min((millis() - gameOverAnimationTimer) / 500.f, 1) * 0.5f : 0;
      if (field[x + y * FIELD_WIDTH]) oled::fillRect(vec2i(x, y) * BLOCK_SIZE + FIELD_OFFSET, BLOCK_SIZE - 1, oled::interpolateColor(field[x + y * FIELD_WIDTH], BLACK, k));
      if (border) oled::drawRect(vec2i(x, y) * BLOCK_SIZE + FIELD_OFFSET - 1, BLOCK_SIZE, BLACK);
    }
  }
}

void update() {
  if (!gameOverAnimationTimer && !clearAnimationTimer) {
    if (millis() - fallTimer > (buttonY.bHeld ? 100 : timeout)) {
      fallTimer = millis();
      tetromino.move(vec2i(0, 1));
    }
    if (millis() - moveTimer > 500 || bJoyMoved) {
      moveTimer = millis();
      tetromino.move(vec2i(joy.x, 0));
      if (joy.y > 0) {
        while (!tetromino.move(vec2i(0, 1))) continue;
      }
    }
    if (buttonX.bPressed) tetromino.rotate();
  }
  if (!gameOverAnimationTimer) checkClearLines();
}

void draw() {
  oled::fillScreen(0x3124);
  drawField();
  if (!gameOverAnimationTimer) {
    if (!clearAnimationTimer)
      tetromino.draw();
    gui::textAt(format("%03d", score), FIELD_OFFSET + vec2i(FIELD_WIDTH * BLOCK_SIZE + 5, 0));
  } else {
    gui::centerText(gui::typeAsync("Game Over!", gameOverAnimationTimer), 10);
    if (millis() - gameOverAnimationTimer > 1000) {
      gui::centerText("You score is", 10 + oled::getCharHeight());
      gui::centerText(format("0x%x", score), 10 + oled::getCharHeight() * 2);
      gui::centerText("X to restart!", 10 + oled::getCharHeight() * 3);
    }
    if (buttonX.bReleased) start();
  }
}

void start() {
  for (int i = 0; i < FIELD_WIDTH * FIELD_HEIGHT; i++) field[i] = 0;
  tetromino.randomize();
  gameOverAnimationTimer = 0;
  score = 0;
  timeout = 500;
  game = { .update = update, .draw = draw };
}
}