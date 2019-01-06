#include <Arduino.h>
#include <Adafruit_Neopixel.h>

/*
 * This pixel-fading class is designed to use relatively little memory, at 
 * the expense of CPU time. It is bound to the size of a byte, so cannot 
 * handle more than 256 LEDs as coded (cf. numPixels).
 *
 * My ideal version of this (that can eat as much RAM as it wants) would 
 * keep track of
 *   uint8_t fadeTime[TOTAL_LEDS]; -- how long we want a fadein/out to take
 *   uint32_t targetColor[TOTAL_LEDS]; -- what is our brightest expected color
 *   uint8_t isFading[TOTAL_LEDS]; -- are we fading this pixel or not?
 *                                 -- and if so, which direction are we going?
 *
 * instead, we use bitwise arrays that less RAM but require math to
 * figure out which bit we care about.
 *
 *   uint8_t fadeBitmap[(TOTAL_LEDS/8)+1];
 *   uint8_t fadeDirection[(TOTAL_LEDS/8)+1];
 *
 * We also piggyback on the actual pixelData passed in at runtime, so that we 
 * can share that array with whatever is actually drawing rather than needing 
 * our own.
 *
 * Our target color is also data-reduced to 2 bits per RGB value so that it 
 * fits in a uint8_t per pixel:
 *
 *   uint8_t targetColor[TOTAL_LEDS];
 *
 * For 196 pixels, this all means that our total RAM usage is:
 *   pixelData (external): 196*3 = 588 bytes
 *   targetColor: 196 bytes
 *   fadeBitmap: (196/8)+1 = 25 bytes
 *   fadeDirection: (196/8)+1 = 25 bytes
 *     Total: 834 bytes
 *
 * ... for a savings of 734 bytes.
 * 
 * We have sacrificed the fadeTime and have to accept a fixed stepwise 
 * increment based on targetColor; we have sacrificed precision of the target
 * color; we have sacrificed CPU time to calculate the bitwise indexes; and 
 * we have sacrificed in the direction of code size and complexity. But for a 
 * device that only has 1500 bytes of RAM, this buys us a significant chunk 
 * of RAM.
 *
 */

class Fader8bit {

 public:
  Fader8bit(Adafruit_NeoPixel *s);
  ~Fader8bit();
  void reset();

  bool isFading(uint8_t pixelNum);
  void setFading(uint8_t pixelNum, uint8_t r, uint8_t g, uint8_t b);
  void setFading(uint8_t pixelNum, uint32_t c);
  void stopFading(uint8_t pixelNum);
  bool isIncreasing(uint8_t pixelNum);
  void setDirection(uint8_t pixelNum, bool increasing);
  int countFading();
  bool areAnyFading();

  void setFadeMode(bool fadeInOnly);

  uint8_t reduceColorTo8bit(uint32_t rgb);
  uint8_t reduceColorTo8bit(uint8_t r, uint8_t g, uint8_t b);
  uint32_t expandColorFrom8bit(uint8_t c);

  uint32_t fadeStepForPixel(uint8_t pixelNum);
  bool capColorValue(uint8_t pixelNum, bool increasing, uint32_t RGBstep);

  bool performFade();
  bool stepOnePixel(uint8_t pixelNum);

  uint8_t howManyWentOut();

  uint8_t get8bitTargetColor(uint8_t pixelNum);

 private:
  // Private copies of pixel data pointer/size
  Adafruit_NeoPixel *strip;
  uint8_t numPixels;

  //   Are we fading? (yes/no) - an array of (TOTAL_LEDS/8) + 1
  uint8_t *fadingBits;
  
  //   Is the fade increasing (1) or decreasing (0)?
  uint8_t *fadeDirectionBits;
  
  //   What is the target brightest value of this fade? - an array of TOTAL_LEDS
  //      (2 bits per R/G/B)
  uint8_t *targetColor;

  uint8_t numExtinguishedLastFade;
  bool fadeInOnly;

  unsigned long nextMillis;
};
