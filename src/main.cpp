#include <Arduino.h>
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

// use GPIO_NUM_0 for internal button
#define BUTTON_PIN GPIO_NUM_27

#define CAN0_INT GPIO_NUM_21

// experimental - don't enable
//#define USE_OTA
//#define USE_INTERRUPT
//#define USE_THREAD

#ifdef USE_OTA
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "wifi_creds.h"
#endif

const char *version = "v2.4";

#ifdef USE_THREAD
TaskHandle_t taskHandle;
#endif

MCP_CAN CAN_Instance(GPIO_NUM_5);

ESP_8_BIT_GFX videoOut(true, 8);

bool elmConnected = false;
#ifdef USE_OTA
bool wifiConnected = true;
#endif

volatile bool pressed = false;
byte mode = 0;

float vCurr;     // Voltage
float iCurr;     // Amperes
float pCurr;     // Power (kW)
float pMaxRecup; // Max Recup
float pMaxDrive; // Max Drive
float tMot;      // Motor Temp
float tBatt;     // Battery Temp
float tChg;      // Charger Temp
float tInv;      // Inverter Temp
float SOC;       // State of Charge
float SOH;       // Battery Health
float rpm;       // RPM of Motor
float kph;       // Speed (based on RPM)

// called by the button interrupt
void buttonPress() { pressed = true; }

// read data from the bus and extract the needed values
void readCan() {
#ifndef USE_INTERRUPT
  while (CAN_Instance.checkReceive() == CAN_MSGAVAIL) {
#endif
    long unsigned int rxId;
    unsigned char len = 0;
    unsigned char rxBuf[8];
    word tempWord;

    CAN_Instance.readMsgBuf(&rxId, &len, rxBuf);
    switch (rxId) {
    case 0x424:
      pMaxRecup = rxBuf[3] / -2.0f;
      pMaxDrive = rxBuf[4] / 2.0f;
      tBatt = (rxBuf[4] + rxBuf[7]) / 2 - 40;
      SOH = rxBuf[5];
      break;
    case 0x155:
      tempWord = ((rxBuf[1] & 0xF) << 8) + rxBuf[2];
      iCurr = (tempWord / -4.0f + 500.0);
      pCurr = (vCurr * iCurr) / 1000;
      tempWord = ((rxBuf[4] & 0xFF) << 8) + rxBuf[5];
      SOC = tempWord / 400.0f;
      break;
    case 0x196:
      tMot = rxBuf[5] - 40;
      break;
    case 0x597:
      tChg = rxBuf[7] - 40;
      break;
    case 0x59E:
      tInv = rxBuf[5] - 40;
      break;
    case 0x55F:
      tempWord = ((rxBuf[6] & 0xF) << 8) + rxBuf[7];
      vCurr = (tempWord / 10.0f);
      pCurr = (vCurr * iCurr) / 1000;
      break;
    case 0x19F:
      tempWord = ((rxBuf[2] & 0xFF) << 4) + ((rxBuf[3] & 0xF0) >> 4);
      rpm = ((tempWord - 2000.0f) * 10.0f);
      kph = (rpm / 7250.0f) * 80.0f;
      break;
    default:
      break;
    }
#ifndef USE_INTERRUPT
  }
#endif
}

#ifdef USE_THREAD
// read can bus continually on core 0
void readCanThread(void *threadid) {
  while (true) {
    readCan();
    // give the system a chance to use core 0
    vTaskDelay(1);
  }
}
#endif

// setup
void setup() {
  Serial.begin(115200);
#ifdef USE_OTA
  ArduinoOTA.setHostname("TwizyDisplay");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t notConnectedCounter = 0;
  while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.println("Wifi connecting...");
      notConnectedCounter++;
      if(notConnectedCounter > 50) { // Reset board if not connected after 15s
          Serial.println("Can't connect to wifi");
          wifiConnected = false;
          WiFi.disconnect(true);
      }
  }
  if (wifiConnected) {
    Serial.print("Wifi connected, IP address: ");
    Serial.println(WiFi.localIP());
    ArduinoOTA.begin();
  }
#endif
  // initialize eeprom with enough space for later
  if (!EEPROM.begin(512)) {
    Serial.println("EEPROM init Error");
  }
  mode = EEPROM.readByte(0);
  // internal button for display switch
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonPress, ONLOW);
  // Initialize MCP2515 running at 8MHz with a baudrate of 500kb/s and the masks
  // and filters disabled.
  if (CAN_Instance.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("MCP2515 Initialized Successfully!");
    elmConnected = true;
  } else {
    Serial.println("Error Initializing MCP2515...");
  }
  // Set operation mode to normal so the MCP2515 sends acks to received data.
  CAN_Instance.setMode(MCP_NORMAL);
  // Set pin mode for CAN INT line
  pinMode(CAN0_INT, INPUT);
#ifdef USE_INTERRUPT
  attachInterrupt(digitalPinToInterrupt(CAN0_INT), readCan, ONLOW);
#endif
#ifdef USE_THREAD
  xTaskCreatePinnedToCore(readCanThread,"CanThread",1000,NULL,1,&taskHandle,0);
#endif
  // Initial setup of graphics library
  Serial.println("Starting video out..");
  videoOut.begin();
  Serial.println("TwizyDisplay started");
}

// check if button was pressed, change mode
void checkButton() {
  if (pressed) {
    mode++;
    if (mode > 4)
      mode = 0;
    EEPROM.writeByte(0, mode);
    EEPROM.commit();
    pressed = false;
  }
}

// display all the info based on selected mode
void showChargeInfo() {
  videoOut.setTextSize(2);
  videoOut.setTextWrap(false);
  videoOut.setCursor(25, 30);
  if (pCurr > 0) {
    videoOut.setTextColor(C_RED);
  } else {
    videoOut.setTextColor(C_GREEN);
  }
  videoOut.printf("Power: %6.2fkW", pCurr);
  videoOut.setTextColor(C_YELLOW);
  videoOut.setCursor(25, 60);
  videoOut.printf("Motor: %6.1fC", tMot);
  videoOut.setCursor(25, 90);
  videoOut.printf("Batt.: %6.1fC", tBatt);
  switch (mode) {
  case 0:
    break;
  case 1:
    videoOut.setTextSize(5);
    videoOut.setTextColor(C_WHITE);
    videoOut.setCursor(100, 120);
    videoOut.printf("%02.0f", kph);
    videoOut.setTextSize(2);
    break;
  case 2:
    videoOut.setTextSize(5);
    videoOut.setTextColor(C_WHITE);
    videoOut.setCursor(70, 120);
    videoOut.printf("%04.0f", rpm);
    videoOut.setTextSize(2);
    break;
  case 3:
    videoOut.setCursor(25, 120);
    videoOut.setTextColor(C_RED);
    videoOut.printf("Max  : %6.2fkW", pMaxDrive);
    videoOut.setCursor(25, 150);
    videoOut.setTextColor(C_GREEN);
    videoOut.printf("Min  : %6.2fkW", pMaxRecup);
    break;
  case 4:
    videoOut.setCursor(25, 120);
    videoOut.setTextColor(C_YELLOW);
    videoOut.printf("Inv. : %6.1fC", tInv);
    videoOut.setCursor(25, 150);
    videoOut.printf("Charg: %6.1fC", tChg);
    break;
  default:
    break;
  }
  // battery
  videoOut.setTextSize(1);
  videoOut.setCursor(25, 180);
  videoOut.setTextColor(C_YELLOW);
  videoOut.printf("Battery: %04.1f%% (%02.0f%%)", SOC, SOH);
}

// draw the display, including background, info and header / footer
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
#ifdef USE_OTA
  if (wifiConnected) {
    if (WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      WiFi.disconnect(true);
    } else {
      ArduinoOTA.handle();
    }
  }
#endif
  checkButton();
#if !defined(USE_THREAD) && !defined(USE_INTERRUPT)
  //if(!digitalRead(CAN0_INT))
  readCan();
#endif
  doDisplay();
}
