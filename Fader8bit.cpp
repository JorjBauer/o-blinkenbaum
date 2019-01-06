#include "Fader8bit.h"

// How many steps are in a fade in/out?
#define NUMSTEPS 80

Fader8bit::Fader8bit(Adafruit_NeoPixel *s)
{
  this->strip = s;
  this->numPixels = s->numPixels();
  this->fadingBits = (uint8_t*)malloc((this->numPixels/8)+1);
  this->fadeDirectionBits = (uint8_t*)malloc((this->numPixels/8)+1);
  this->targetColor = (uint8_t*)malloc(this->numPixels);
  this->fadeInOnly = false;
  for (int i=0; i<(this->numPixels/8)+1; i++) {
    this->fadingBits[i] = 0;
  }
  this->nextMillis = 0;
}

Fader8bit::~Fader8bit()
{
  free(this->fadingBits);
  free(this->fadeDirectionBits);
  free(this->targetColor);
}

void Fader8bit::reset()
{
  // For anything that is still fading, make sure it's 
  // fading *out* now
  for (int i=0; i<this->numPixels; i++) {
    if (isFading(i)) {
      setDirection(i, false);
    }
  }
}

bool Fader8bit::isFading(uint8_t pixelNum)
{
  uint8_t idx = pixelNum / 8;
  uint8_t bitNum = pixelNum % 8;

  return (this->fadingBits[idx] & (1 << bitNum));
}

void Fader8bit::setFading(uint8_t pixelNum, 
          uint8_t r, uint8_t g, uint8_t b)
{
  uint8_t idx = pixelNum / 8;
  uint8_t bitNum = pixelNum % 8;

  this->fadingBits[idx] |= (1 << bitNum);        // Yes, we are fading;
  this->fadeDirectionBits[idx] |= (1 << bitNum); // and we are increasing.

  this->targetColor[pixelNum] = reduceColorTo8bit(r, g, b); // calc target color

  this->strip->setPixelColor(pixelNum, 0); // fade in from black
}

void Fader8bit::setFading(uint8_t pixelNum, uint32_t c)
{
  uint8_t r, g, b;
  r = (c >> 16) & 0xFF;
  g = (c >>  8) & 0xFF;
  b = (c      ) & 0xFF;
  setFading(pixelNum, r, g, b);
}

void Fader8bit::stopFading(uint8_t pixelNum)
{
  uint8_t idx = pixelNum / 8;
  uint8_t bitNum = pixelNum % 8;

  this->fadingBits[idx] &= ~(1 << bitNum);
}

bool Fader8bit::isIncreasing(uint8_t pixelNum)
{
  uint8_t idx = pixelNum / 8;
  uint8_t bitNum = pixelNum % 8;

  return (this->fadeDirectionBits[idx] & (1 << bitNum));
}

void Fader8bit::setDirection(uint8_t pixelNum, bool increasing)
{
  uint8_t idx = pixelNum / 8;
  uint8_t bitNum = pixelNum % 8;

  if (increasing) {
    this->fadeDirectionBits[idx] |= (1 << bitNum);
  } else {
    this->fadeDirectionBits[idx] &= ~(1 << bitNum);
  }
}

bool Fader8bit::areAnyFading()
{
  for (uint8_t idx = 0; idx < this->numPixels; idx++) {
    if (isFading(idx)) {
      return true;
    }
  }
  return false;
}

int Fader8bit::countFading()
{
  int count = 0;
  for (uint8_t idx = 0; idx < this->numPixels; idx++) {
    if (isFading(idx)) {
      count++;
    }
  }
  return count;
}

void Fader8bit::setFadeMode(bool fadeInOnly)
{
  this->fadeInOnly = fadeInOnly;
}

uint8_t Fader8bit::reduceColorTo8bit(uint32_t rgb)
{
  return reduceColorTo8bit((rgb >> 16) & 0xFF,
			   (rgb >> 8)  & 0xFF,
			   (rgb)       & 0xFF);
}

uint8_t Fader8bit::reduceColorTo8bit(uint8_t r, uint8_t g, uint8_t b)
{
  // Reduce this to a 3/3/2-bit R/G/B
  return ( (r & 0xC0) | 
	   ( (g & 0xC0) >> 3) |
	   ( (b & 0xC0) >> 6) );
}

uint32_t Fader8bit::expandColorFrom8bit(uint8_t c)
{
  // Losing precision, return a color to a 32-bit form
  uint32_t r, g, b;

  r = (c & 0xE0);
  g = (c & 0x1C) << 3;
  b = (c & 0x03) << 6;

  return (strip->Color(r, g, b));
}

uint32_t Fader8bit::fadeStepForPixel(uint8_t pixelNum)
{
  uint32_t target = expandColorFrom8bit(this->targetColor[pixelNum]);
  
  // Special case: if we're trying to fade to black, that's not allowed.
  // return a bogus value that allows some attempt at fading.
  //
  // (Instead of fading to black, we "reverse fade" [fade out] from a color.
  // That's why there is no "fade to black.")
  if (target == 0) {
    return 0x010101;
  }

  uint8_t r,g,b;
  r = (target >> 16) & 0xFF;
  g = (target >> 8 ) & 0xFF;
  b = (target      ) & 0xFF;

  // +1 in each of these b/c...
  //   ... 8 bit division is lossy;
  //   ... we want to ensure each channel has *some* fade,
  //       b/c we might be "fading" by less than 8.
  uint8_t 
    sr = (r / NUMSTEPS) + 1,
    sg = (g / NUMSTEPS) + 1,
    sb = (b / NUMSTEPS) + 1;

  target = sr;
  target <<= 8;
  target |= sg;
  target <<= 8;
  target |= sb;

  return target;
}

bool Fader8bit::capColorValue(uint8_t pixelNum, bool increasing, uint32_t RGBstep)
{
  // What color is the pixel right now?
  uint32_t c = this->strip->getPixelColor(pixelNum);
  uint8_t r = (c >> 16) & 0xFF;
  uint8_t g = (c >>  8) & 0xFF;
  uint8_t b = (c      ) & 0xFF;

  // what are the stepwise changes?
  uint8_t sr, sg, sb;
  sr = (RGBstep >> 16) & 0xFF;
  sg = (RGBstep >> 8 ) & 0xFF;
  sb = (RGBstep      ) & 0xFF;

  // Count of how many values are at the target
  uint8_t count = 0;

  if (increasing) {
    // Find the target color
    uint32_t target = expandColorFrom8bit(this->targetColor[pixelNum]);

    // separate the target component values
    uint8_t tr, tg, tb;
    tr = (target >> 16) & 0xFF;
    tg = (target >> 8 ) & 0xFF;
    tb = (target      ) & 0xFF;

    // If the current pixel values are higher than our target, then adjust the target.
    if (r > tr) tr = r;
    if (g > tg) tg = g;
    if (b > tb) tb = b;

    // Careful of unsigned byte overflow...
    if (tr - r <= sr) { // If (TargetRed - CurrentRed < StepRed) then
      r = tr; count++; //   we're too close to the target and would overflow
    } else {           //   so just set to the target, and note that we capped
      r += sr;         // else add the StepRed to the red channel value
    }
    if (tg - g <= sg) {
      g = tg; count++;
    } else {
      g += sg;
    }
    if (tb - b <= sb) {
      b = tb; count++;
    } else {
      b += sb;
    }
    
  } else {

    // decreasing to zero. Careful: these are unsigned bytes...
    if (r > sr) {    // If red is still larger than the stepRed, then 
      r -= sr;       //    simply subtract the step from the current red value
    } else {         // else 
      r = 0; count++;//    we're too close and would underflow; set to zero
    }
    if (g > sg) {
      g -= sg;
    } else {
      g = 0; count++;
    }
    if (b > sb) {
      b -= sb;
    } else {
      b = 0; count++;
    }
  }

  // Set the pixelData to the value we want
  this->strip->setPixelColor(pixelNum, r, g, b);

  // Return true if we've reached our current target (in- or de-creasing)
  return (count == 3);
}

bool Fader8bit::stepOnePixel(uint8_t idx)
{
  uint32_t s = fadeStepForPixel(idx);
  if (s == 0)
    return false;
  
  if (!isFading(idx)) 
    return false;
  
  if (isIncreasing(idx)) {
    if (capColorValue(idx, true, s)) {
      // If we reached the max, then change the direction                                  
      if (this->fadeInOnly) {
	stopFading(idx);
	numExtinguishedLastFade++;
      } else {
	setDirection(idx, false);
      }
    }
  } else {
    if (capColorValue(idx, false, s)) {
      // If we reached zero, then turn off isFading                                        
      stopFading(idx);
      numExtinguishedLastFade++;
    }
  }

  return true;
}

// One step in the fade action, to be called regularly. 
// returns true if it updates any LEDs.
bool Fader8bit::performFade()
{
  numExtinguishedLastFade = 0;

  bool retval = false;

  if (millis() >= nextMillis) {
    for (uint8_t idx = 0; idx < this->numPixels; idx++) {
      if (isFading(idx)) {
	uint32_t s = fadeStepForPixel(idx);
	if (s == 0) {
	  // FIXME: ERROR: handle this case
#ifdef DEBUG
	  Serial.println("ERROR: fadeStepForPixel is 0, so this pixel won't ever change");
	  while (1) ;
#endif
	  continue;
	}
	retval = true;
	if (isIncreasing(idx)) {
	  if (capColorValue(idx, true, s)) {
	    // If we reached the max, then change the direction
	    if (this->fadeInOnly) {
	      stopFading(idx);
	      numExtinguishedLastFade++;
	    } else {
	      setDirection(idx, false);
	    }
	  }
	} else {
	  if (capColorValue(idx, false, s)) {
	    // If we reached zero, then turn off isFading
	    stopFading(idx);
	    numExtinguishedLastFade++;
	  }
	}
      }
    }
    nextMillis = millis() + 10;
  }
  return retval;
}

uint8_t Fader8bit::howManyWentOut()
{
  return numExtinguishedLastFade;
}

uint8_t Fader8bit::get8bitTargetColor(uint8_t pixelNum)
{
  return targetColor[pixelNum];
}


