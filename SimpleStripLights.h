#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "Fader8bit.h"
#include <RingBuffer.h>

#define MAX_TWINKLE_LIT ((2*numLights)/3)
#define MAX_COMMAND_SIZE 6

enum runmode {
  InvalidMode = -1,
  RawMode = 0,
  TwinkleMode,
  WipeMode,
  ChaseMode,
  TardisMode,
  ColorMode,
  PulseMode
};

struct _ModeData {
  int8_t repeat;
  uint32_t color;
  uint32_t color2;
  byte wantFade;
  byte fadeMode;
  union _mode {
    struct _wipe {
      uint8_t pos;
    } wipe;
  } mode;
};

class SimpleStripLights {
 public:
  SimpleStripLights(uint8_t pin, int numLights, runmode defaultMode = WipeMode, uint32_t defaultColor = 0x000000F0, uint32_t defaultColor2 = 0xFFFFC4); // defaultColor is xxRRGGBB. 0xFFFFC4 is a pleasing white on my test strips.

  ~SimpleStripLights();

  void update();
  void handleCommands(const uint8_t *data, int datalen);
  void resetMode(runmode newMode);
  void setupPulseMode();
  

 private:
  bool performCommand();
  bool handleInput(byte b);
  int findRandomUnfadedPixel();
  bool twinkle();
  bool pulse();
  bool wipe();
  bool tardis();

 private:
  Adafruit_NeoPixel *strip;
  runmode currentMode;
  unsigned long nextMillis;
  struct _ModeData modeData;
  Fader8bit *fader = NULL;
  int numLights;
  RingBuffer *bufferedInput;
  byte pendingCommand[MAX_COMMAND_SIZE];
  byte currentCommandSize;
};
