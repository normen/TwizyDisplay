#include <Arduino.h>
#include <pthread.h>
#include <mcp_can.h>
#include <EEPROM.h>
#include <ESP_8_BIT_GFX.h>
#include "images.h"

#define C_BLACK 0x00
#define C_BLUE 0x03
#define C_GREEN 0x1C
#define C_CYAN 0x1F
#define C_RED 0xE0
#define C_MAGENTA 0xE3
#define C_YELLOW 0xFC
#define C_WHITE 0xFF

#define BUTTON_PIN GPIO_NUM_0

#define CAN0_INT GPIO_NUM_21

//#define USE_INTERRUPT
//#define USE_THREAD

const char *version = "v1.4";

#ifdef USE_THREAD
pthread_t can_thread;
#endif

MCP_CAN CAN_Instance(GPIO_NUM_5);

ESP_8_BIT_GFX videoOut(true, 8);

bool elmConnected = false;

volatile bool pressed = false;
byte mode = 0;

float vCurr;    //F:  0.2V, O:
float iCurr;    //F: 0.25A, O:-500
float pMaxRecup;//F:  500W, O:
float pCurr;    //F:     -, O:
float pMaxDrive;//F:  500W, O:
float tBatt;     //F:    1, O:-40
float tChg;      //F:    1, O: -40
float tInv;      //F:    1, O: -40
float tMot;      //F:    1, O: -40
float SOC;       //F:0.0025%, O:
float SOH;       //F:    1, O: 
float kph;       //F:    1, O: 

void buttonPress() { pressed = true; }

void readCan() {
  // TODO: while?
#ifndef USE_INTERRUPT
  while(CAN_Instance.checkReceive() == CAN_MSGAVAIL) {
  // If CAN0_INT pin is low, read receive buffer
  //while(!digitalRead(CAN0_INT)) {
#endif
    long unsigned int rxId;
    unsigned char len = 0;
    unsigned char rxBuf[8];

    word endianHelpi;

    // Read data: len = data length, buf = data byte(s)
    CAN_Instance.readMsgBuf(&rxId, &len, rxBuf);

    if(rxId == 0x424){
      pMaxRecup = rxBuf[3] / -2.0f;
      pMaxDrive = rxBuf[4] / 2.0f;
      tBatt = (rxBuf[4] + rxBuf[7] )/2 -40;
      SOH = rxBuf[5];
    }
    if(rxId == 0x155){
      endianHelpi = ((rxBuf[1] & 0xF) <<8) + rxBuf[2];
      iCurr = (endianHelpi/-4.0f+500.0);
      pCurr = (vCurr * iCurr)/1000;
      endianHelpi = ((rxBuf[4] & 0xFF) <<8) + rxBuf[5]; 
      SOC = endianHelpi / 400.0f;
    }
    if(rxId == 0x196){
      tMot = rxBuf[5] - 40;
    }
    if(rxId == 0x597){
      tChg = rxBuf[7] - 40;
    }
    if(rxId == 0x59E){
      tInv = rxBuf[5] - 40;
    }
    if(rxId == 0x55F){
      endianHelpi = ((rxBuf[6] & 0xF) <<8) + rxBuf[7]; 
      vCurr = (endianHelpi/10.0f);
      pCurr = (vCurr * iCurr)/1000;
    }
    if(rxId == 0x19F){
      endianHelpi = ((rxBuf[2] & 0xFF) <<4) + ((rxBuf[3] & 0xF0) >>4);
      kph = (((endianHelpi-2000.0f) * 10.0f) / 7250.0f) * 80.0f;
    }
#ifndef USE_INTERRUPT
  }
#endif
}

void *readCanThread(void *threadid) {
  while(true){
    readCan();
  }
}

void setup() {
  Serial.begin(115200);
  if (!EEPROM.begin(512)) {
    Serial.println("EEPROM init Error");
  }
  mode = EEPROM.readByte(0);
  // TODO: internal button for display switch
  //pinMode(BUTTON_PIN, INPUT);
  //attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonPress, ONLOW);

  // Initialize MCP2515 running at 8MHz with a baudrate of 500kb/s and the masks and filters disabled.
  if(CAN_Instance.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK){
    Serial.println("MCP2515 Initialized Successfully!");
    elmConnected = true;
  } else {
    Serial.println("Error Initializing MCP2515...");
  }
  // Set operation mode to normal so the MCP2515 sends acks to received data.
  CAN_Instance.setMode(MCP_NORMAL);

  pinMode(CAN0_INT, INPUT);
#ifdef USE_INTERRUPT
  attachInterrupt(digitalPinToInterrupt(CAN0_INT), readCan, ONLOW);
#endif
#ifdef USE_THREAD
  Serial.println("Starting CAN thread..");
  int returnValue = pthread_create(&can_thread, NULL, readCanThread, (void *)0);
  if (returnValue) {
    Serial.println("Can't create thread");
  } else {
    Serial.println("CAN thread started.");
  }
#endif
  // Initial setup of graphics library
  Serial.println("Starting video out..");
  videoOut.begin();
  Serial.println("TwizyDisplay started");
}

void checkButton() {
  if (pressed) {
    mode++;
    if (mode > 2)
      mode = 0;
    EEPROM.writeByte(0, mode);
    EEPROM.commit();
    pressed = false;
  }
}

void showChargeInfo() {
  videoOut.setTextSize(2);
  videoOut.setTextWrap(false);
  videoOut.setCursor(25, 30);
  if (pCurr > 0) {
    videoOut.setTextColor(C_RED);
  } else {
    videoOut.setTextColor(C_GREEN);
  }
  videoOut.printf("Power:  %.2fkW", pCurr);
  videoOut.setTextColor(C_WHITE);
  videoOut.setCursor(25, 60);
  videoOut.printf("Motor: %.1fdeg", tMot);
  videoOut.setCursor(25, 90);
  videoOut.printf("Batt.: %.1fdeg", tBatt);
  switch (mode) {
  case 0:
    break;
  case 1:
    videoOut.setTextSize(5);
    videoOut.setTextColor(C_WHITE);
    videoOut.setCursor(100, 120);
    videoOut.printf("%.0f", kph);
    videoOut.setTextSize(2);
    break;
  case 2:
    videoOut.setCursor(25, 120);
    videoOut.setTextColor(C_RED);
    videoOut.printf("Max  :  %.2fkW", pMaxDrive);
    videoOut.setCursor(25, 150);
    videoOut.setTextColor(C_GREEN);
    videoOut.printf("Min  :  %.2fkW", pMaxRecup);
    break;
  }
  // battery
  videoOut.setTextSize(1);
  videoOut.setCursor(25, 180);
  videoOut.setTextColor(C_YELLOW);
  videoOut.printf("Battery: %.1f%% (%.0f%%)", SOC, SOH);
}

void doDisplay() {
  videoOut.waitForFrame();
  videoOut.fillScreen(0);
  videoOut.drawBitmap(0, 0, epd_bitmap_eddie, 256, 240, C_BLUE);
  videoOut.flush();
  showChargeInfo();
  // copyright
  videoOut.setTextColor(C_WHITE);
  videoOut.setTextSize(1);
  videoOut.setCursor(10, 220);
  videoOut.printf("TwizyDisplay %s by Normen Hansen", version);
  if (!elmConnected) {
    videoOut.setTextColor(C_RED);
    videoOut.setCursor(10, 10);
    videoOut.setTextSize(1);
    videoOut.print("CAN Bus Error");
  }
}

void loop() {
  checkButton();
#if !defined(USE_THREAD) && !defined(USE_INTERRUPT)
  readCan();
#endif
  doDisplay();
}
