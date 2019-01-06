#include <Arduino.h>

#include <RFM69.h>          //get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <SPIFlash.h>      //get it here: https://www.github.com/lowpowerlab/spiflash
#include <WirelessHEX69.h> //get it here: https://github.com/LowPowerLab/WirelessProgramming/tree/master/WirelessHEX69
#include <RingBuffer.h>
#include "SimpleStripLights.h"

// Node 3 is Jake's tree; 11-14 are the dining room (left-to-right)
#define NODEID             11
// #define NODEID           3
#define NETWORKID          212
#define FREQUENCY     RF69_915MHZ
#ifdef DEFAULTKEY
#define ENCRYPTKEY DEFAULTKEY
#else
#pragma message("Default encryption key not found; using compiled-in default instead")
#define ENCRYPTKEY "sampleEncryptKey"
#endif
#define FLASH_SS 8
//#define IS_RFM69HW

#define GATEWAYID 1

RFM69 radio;
SPIFlash flash(FLASH_SS, 0xEF30); //EF30 for windbond 4mbit flash

#define WS2812PIN 6
#define TOTAL_LEDS 150

SimpleStripLights *lights;

bool respondToBroadcast = true;

void setup() {
  Serial.begin(115200);
  Serial.println("Startup");

  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  radio.encrypt(ENCRYPTKEY);
#ifdef IS_RFM69HW
  radio.setHighPower(); //only for RFM69HW!
#endif

  flash.initialize();

  lights = new SimpleStripLights(WS2812PIN, TOTAL_LEDS, TwinkleMode, 0x000000F0, 0x00FFFFC4);
}

void loop() {
  // If we receive data on the radio, then look for a software update, or stash it.
  if (radio.receiveDone()){
    CheckForWirelessHEX(radio, flash, true); // checks for the header 'FLX?'

    bool handled = false;
    if (radio.DATALEN==2 && radio.DATA[0] == '^') {
      respondToBroadcast = radio.DATA[1];
      handled = true;
    }

    uint8_t target = radio.TARGETID; // save the target of the
				     // received packet before we
				     // destroy the data by sending an
				     // ACK
    if (radio.ACKRequested()) {
      radio.sendACK();
    }

    if (!handled) {
      // Only handle the packets if it was targeted directly at us, or
      // if we are configured to respond to broadcasts
      if (respondToBroadcast || (target != RF69_BROADCAST_ADDR)) {
	lights->handleCommands((const uint8_t *)radio.DATA, radio.DATALEN);
      }
    }

  }

  if (Serial.available() > 0) {
    byte b = Serial.read();
    lights->handleCommands(&b, 1);
  }


  lights->update();
}


