#include <Adafruit_NeoPixel.h>
#include "SimpleStripLights.h"

// minimum size here is 61 (size of largest packet we can recv)
#define BUFFERSIZE 61

SimpleStripLights::SimpleStripLights(uint8_t pin, int numLights, runmode defaultMode, uint32_t defaultColor, uint32_t defaultColor2) : numLights(numLights)
{
  strip = new Adafruit_NeoPixel(numLights, pin, 
				NEO_GRB | NEO_KHZ800);
  strip->begin();
  strip->show();

  fader = new Fader8bit(strip);

  bufferedInput = new RingBuffer(BUFFERSIZE);

  currentCommandSize = 0;

  // Set some mode defaults: infinite repeat, default color, fading, mode
  modeData.repeat = -1;
  modeData.color = strip->Color((defaultColor & 0xFF0000) >> 16, // R
				(defaultColor & 0x00FF00) >>  8, // G
				(defaultColor & 0x0000FF) );     // B
  modeData.color2 = strip->Color((defaultColor2 & 0xFF0000) >> 16,
				 (defaultColor2 & 0x00FF00) >> 8,
				 (defaultColor2 & 0x0000FF) );
  modeData.wantFade = true;
  modeData.fadeMode = 0;

  resetMode(defaultMode);
}

SimpleStripLights::~SimpleStripLights()
{
  delete bufferedInput;
  delete fader;
  delete strip;
}

/* handleCommands is called from serial or radio data trying to change the 
 * state of the lights */
void SimpleStripLights::handleCommands(const uint8_t *data, int datalen)
{
  for (int i=0; i<datalen; i++) {
    bufferedInput->addByte(data[i]);
  }
}

// Ostensibly, we have enough data to perform the given command (or it's an 
// invalid command). Do our best.
bool SimpleStripLights::performCommand()
{
  bool retval = false; // assume no changes to the lights

  switch (pendingCommand[0]) {
  case 'f':
    modeData.wantFade = pendingCommand[1];
    break;
  case 'F':
    modeData.fadeMode = pendingCommand[1];
    break;
  case 'c':
    modeData.color = strip->Color(pendingCommand[1], pendingCommand[2], pendingCommand[3]);
    break;
  case 'x':
    modeData.color2 = strip->Color(pendingCommand[1], pendingCommand[2], pendingCommand[3]);
    break;
  case 'C':
    resetMode(ColorMode);
    break;
  case 'R':
    modeData.repeat = pendingCommand[1];
    break;
  case 'r':
    resetMode(RawMode);
    break;
  case '1':
    if (currentMode == RawMode) {
      uint16_t pixelNum = (pendingCommand[1] << 8) | pendingCommand[2];
      if (modeData.wantFade) {
	fader->setFading(pixelNum, modeData.color);
	retval = true;
      } else {
	fader->stopFading(pixelNum);
	strip->setPixelColor(pixelNum, modeData.color);
	retval = true;
      }
    }
    break;
  case 't':
    resetMode(TardisMode);
    break;
  case 'T':
    resetMode(TwinkleMode);
    break;
  case 'W':
    resetMode(WipeMode);
    break;
  case 'p':
    resetMode(PulseMode);
    break;
  case '!':
    resetMode(ChaseMode);
    break;
  case 'b': // brightness
    retval = true;
    strip->setBrightness(pendingCommand[1]);
    break;
  }

  return retval;
}

bool SimpleStripLights::handleInput(byte b)
{
  // This is an async data parser; it wastes some RAM to do so, because it 
  // has to have a buffer that's the maximum size of any command.
  bool retval = false; // assume no changes to the lights

  pendingCommand[currentCommandSize++] = b;

  // Determine whether or not we have enough data to proceed
  byte bytesNeeded = 1;
  switch (pendingCommand[0]) {

  case 'f': // set fade preference flag
  case 'F': // set fade mode preference flag
  case 'R': // set repeat
  case 'b': // set brightness (0-255)
    bytesNeeded = 2;
    break;
  case '1': // raw mode: set color/fade for a given pixel
    bytesNeeded = 3;
    break;
  case 'c': // set color preference
  case 'x':
    bytesNeeded = 4;
    break;

  default:
    // Otherwise assume it's 1 byte.
    break;
  }

  if (currentCommandSize == bytesNeeded) {
    retval = performCommand();
    currentCommandSize = 0;
  }

  if (currentCommandSize >= MAX_COMMAND_SIZE) {
    // Overflow happened in the command buffer; flush it and hope for a 
    // protocol resynchronization.
    currentCommandSize = 0;
  }
  return retval;
}

/* update() is to be called periodically to update any animations in play. */
void SimpleStripLights::update()
{
  bool changes = false;

  /* If we have input, then handle it */
  while (bufferedInput->hasData()) {
    changes |= handleInput(bufferedInput->consumeByte());
  }

  /* Deal with maintenance of the modes */
  switch (currentMode) {
  case InvalidMode:
    break;
  case RawMode:
    break;
  case TwinkleMode:
    changes |= twinkle();
    break;
  case WipeMode:
  case ChaseMode:
    changes |= wipe();
    break;
  case PulseMode:
    changes |= pulse();
    break;
  case TardisMode:
    changes |= tardis();
    break;
  }

  /* Deal with maintenance of the faders */
  changes |= fader->performFade();

  /* If there are changes, then update the strips */
  if (changes) {
    strip->show();
  }
}

void SimpleStripLights::resetMode(runmode newMode)
{
  currentMode = newMode;
  nextMillis = 0;

  /* Reset local variables for each mode */
  switch (newMode) {
  case TwinkleMode:
    strip->clear();
    break;
  case WipeMode:
  case ChaseMode:
    modeData.mode.wipe.pos = 0;
    break;
  case PulseMode:
    setupPulseMode();
    break;
  case ColorMode:
    {
        for (int i=0; i<numLights; i++) {
          strip->setPixelColor(i, modeData.color);
        }
        strip->show();
    }
  }

  // If we're going in to raw mode, let the fades finish as-was. Otherwise 
  // reset them.
  // Also don't touch faders for PulseMode, which just did that...
  if (newMode != RawMode && 
      newMode != PulseMode) {
    fader->reset();
    if (modeData.fadeMode == 0) {
      // default fade mode
      fader->setFadeMode(newMode == WipeMode);
    } else {
      // inverse of default fade mode
      fader->setFadeMode(newMode != WipeMode);
    }
  }
}

void SimpleStripLights::setupPulseMode()
{
  // Preload all the faders for pulse mode.
  uint8_t phase = 0;
  bool usingPrimaryColor = true;

  for (uint16_t i=0; i<numLights; i++) {
    phase++;

    fader->setFading(i, usingPrimaryColor ? modeData.color : modeData.color2);

    for (uint8_t j=0; j<=phase; j++) {
      fader->stepOnePixel(i);
      if (fader->isFading(i) == 0) {
	usingPrimaryColor =!usingPrimaryColor;
	fader->setFading(i, usingPrimaryColor ? modeData.color : modeData.color2);
	phase = 1;
      }
    }
  }

}

int SimpleStripLights::findRandomUnfadedPixel()
{
  // Try to find a random unfaded pixel 10 times. If we fail, then return -1.
  for (int i=0; i<10; i++) {
    uint8_t pixelNum = random(0, numLights-1);
    if (fader->isFading(pixelNum) == false) {
      return pixelNum;
    }
  }
  return -1;
}

bool SimpleStripLights::twinkle()
{
  bool didChangeAnything = false;

  if (millis() >= nextMillis) {
    for (int lightcount = 0; lightcount < 6; lightcount++) { // FIXME: constant. Light 6 lights per loop iteration.
      if (fader->countFading() < MAX_TWINKLE_LIT) {
        // Light another if we can!
        int idx = findRandomUnfadedPixel();
        if (idx != -1) {
          didChangeAnything = true;
          if (random(0,2) == 0) {
            // fade to white
            fader->setFading(idx, modeData.color2);
          } else {
            // Fade to the second color
	    fader->setFading(idx, modeData.color);
          }
        }
      }
    }
    nextMillis = millis() + 150;
  }
  return didChangeAnything;
}

bool SimpleStripLights::pulse()
{
  // If any pixel hit black, then swap its color.
  for (uint16_t i=0; i<numLights; i++) {
    if (fader->isFading(i) == 0) {
      if (fader->get8bitTargetColor(i) == 
	  fader->reduceColorTo8bit(modeData.color)) {
	fader->setFading(i, modeData.color2);
      } else {
	fader->setFading(i, modeData.color);
      }
    }
  }
}

bool SimpleStripLights::wipe()
{
  if (millis() >= nextMillis) {
    fader->setFading(modeData.mode.wipe.pos, modeData.color);
    if (modeData.mode.wipe.pos == numLights-1) {
      if ((currentMode == ChaseMode) && (modeData.repeat != 0)) {
        if (modeData.repeat > 0) {
          modeData.repeat--;
        }
        modeData.mode.wipe.pos = 0;
      } else {
        resetMode(RawMode);
      }
    } else {
      modeData.mode.wipe.pos++;
    }
    // Wipe on as fast as we can: don't reset nextMillis at all. But
    // in chase mode, we delay a little.
    if (currentMode == ChaseMode)
      nextMillis = millis() + 30;
    return true;
  }
  return false;
}

bool SimpleStripLights::tardis()
{
  // Whenever the fader finishes fading everything out, start it over again
  if (millis() >= nextMillis) {
    if (!fader->areAnyFading()) {
      for (int i=0; i<numLights; i++) {
	fader->setFading(i, modeData.color);
      }
    }
    nextMillis = millis() + 150;
  }
}

