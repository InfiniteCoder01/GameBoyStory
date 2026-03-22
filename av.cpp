#include "games.hpp"

/* MP3 Audio */
#include <AudioFileSourceFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>

/* MJPEG Video */
#include "MjpegClass.h"
static MjpegClass mjpeg;

bool soundEnabled = true;
AudioOutputMixer mixer(32, &audioOutput);

Sound::Sound(const String &filename) {
  source = new AudioFileSourceFS(SD);
  if (!source->open(filename.c_str())) {
    Serial.println("Couldn't open sound " + filename);
    delete source;
    source = nullptr;
    generator = nullptr;
    stub = nullptr;
    return;
  }
  generator = filename.endsWith(".mp3") ? (AudioGenerator *)new AudioGeneratorMP3() : (AudioGenerator *)new AudioGeneratorWAV();
  stub = mixer.NewInput();
  generator->begin(source, stub);
}

// pixel drawing callback
static int drawMCU(JPEGDRAW *pDraw) {
  uint32_t s = millis();
  oled.fastDrawImage(pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight, (const uint8_t *)pDraw->pPixels);
  return 1;
}

const uint32_t FPS = 24;
void playVideo(const String &path) {
  AudioFileSourceFS aFile(SD);
  AudioGeneratorMP3 mp3;

  if (!aFile.open((path + ".mp3").c_str())) Serial.println(F("Failed to open audio file"));   

  File vFile = SD.open(path + ".mjpeg");
  uint8_t *mjpeg_buf = new uint8_t[(size_t)oled.getWidth() * oled.getHeight() * 2 / 4];
  mjpeg.setup(&vFile, mjpeg_buf, drawMCU, false, true); //MJPEG SETUP -> bool setup(Stream *input, uint8_t *mjpeg_buf, JPEG_DRAW_CALLBACK *pfnDraw, bool enableMultiTask, bool useBigEndian)

  if (!vFile || vFile.isDirectory()) {
    Serial.println(("ERROR: Failed to open " + path + ".mjpeg"));
    oled.clear();
    oled.setCursor(0, 0);
    oled.println(("ERROR: Failed to open " + path + ".mjpeg"));
    nextFrame();
    delay(1000);
    return;
  }

  // init audio
  if (!mp3.begin(&aFile, &audioOutput))
    Serial.println(F("Failed to start audio!"));

  uint32_t next_frame = 0;
  uint32_t start_ms = millis();
  uint32_t next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
  while (vFile.available() && !buttonY.pressed) {
    // Read video
    mjpeg.readMjpegBuf();
    if (millis() < next_frame_ms) mjpeg.drawJpg();
    else Serial.println(F("Skip frame"));
    nextFrame();

    // Play audio
    if (mp3.isRunning() && !mp3.loop()) mp3.stop();

    while (millis() < next_frame_ms) vTaskDelay(1);
    next_frame_ms = start_ms + (++next_frame * 1000 / FPS);
  }
  vFile.close();
  delete[] mjpeg_buf;

  if (buttonY.pressed) mp3.stop();
  else {
    while (mp3.isRunning()) {
      if (!mp3.loop()) mp3.stop();
    }
  }

  aFile.close();
}