#include <Adafruit_Protomatter.h>
#include "AbcdFont.h"
#include "Digi11x17_2Font.h"
#include "Digi25x43Font.h"
#include "Font5x7Fixed.h"
#include "Matrix_iconsFont.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include "NTP.h"
#include <DS3231.h>
#include <DFRobot_DF1201S.h>

uint8_t rgbPins[] = { 42, 41, 40, 38, 39, 37 };
uint8_t addrPins[] = { 45, 36, 48, 35, 21 };
uint8_t clockPin = 2;
uint8_t latchPin = 47;
uint8_t oePin = 14;

enum View { Dummy = 100,
            IntroView,
            MainView,
            ShootInView,
            TournamentView,
            SetupView,
            StatusView,
            PauseView };
enum Phase { IdlePhase = 200,
             RunPhase,
             StopPhase };


View view = MainView;
Phase phase = IdlePhase;

bool hold = false;
bool prepare = true;
bool initialize = true;
bool wifiEnabled = true;
bool bluetoothActive = false;
bool wifiActive = false;
bool timeSynced = false;
bool soundActive = false;
bool isMaster = true;
bool reshootActive = false;
unsigned long currentTime;
unsigned long targetTime;
uint16_t prepareTime;
uint16_t shootingTime;
uint16_t arrowTime;
uint16_t arrowCount;
uint16_t reshootArrowCount;
uint16_t shootInTime;
uint16_t reshootTime;
uint16_t warnTime;
uint16_t mode;
uint16_t passes;
uint16_t initTimer;
uint16_t initTimer2;
uint16_t timer;
uint16_t timer2;
uint16_t currentPasses;
uint8_t groupCount;
uint8_t actionCount;
uint8_t displayNo = 0;

const char* controllerIp;
const char* CHANGE_VIEW = "change_view";
const char* TOURNAMENT_MODE = "tournament_mode";

const char* HOME = "home";
const char* SETUP = "setup";
const char* SHOOT_IN = "shoot_in";
const char* TOURNAMENT = "tournament";
const char* RE_SHOOT = "re_shoot";

const char* START_PAUSE_ACTION = "start_pause_action";
const char* RESET = "reset";
const char* STOP = "stop";
const char* EMERGENCY = "emergency";
const char* VOLUME = "volume";
const char* TEST_SIGNAL = "testsignal";
const char* STATE = "state";
const char* CONFIG = "config";
unsigned int localPort = 5005;

float r1, r2, r3, r4, r5;

char receiveBuffer[300];
char responseBuffer[300];

char ssid[] = "";
char pass[] = "";

Adafruit_Protomatter matrix(
  128,                        // Width of matrix (or matrix chain) in pixels
  2,                          // Bit depth, 1-6
  2, rgbPins,                 // # of matrix chains, array of 6 RGB pins for each
  5, addrPins,                // # of address pins (height is inferred), array of pins
  clockPin, latchPin, oePin,  // Other matrix control pins
  false);                     // No double-buffering here (see "doublebuffer" example)

DFRobot_DF1201S DF1201S;
RTClib rtc;
DS3231 uhr;
WiFiUDP udp;
WiFiUDP udpNtp;
NTP ntp(udpNtp);
JsonDocument doc;

#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
#include "SoftwareSerial.h"
SoftwareSerial DF1201SSerial(2, 3);  //RX  TX
#else
#define DF1201SSerial Serial1
#endif

const uint16_t Red = matrix.color565(255, 0, 0);
const uint16_t Green = matrix.color565(0, 255, 0);
const uint16_t Blue = matrix.color565(0, 0, 255);
const uint16_t Yellow = matrix.color565(200, 200, 0);
const uint16_t White = matrix.color565(255, 255, 255);
const uint16_t Black = matrix.color565(0, 0, 0);
const uint16_t Orange = matrix.color565(241, 196, 15);
const uint16_t Gray = matrix.color565(120, 10, 10);

void setup(void) {
  ProtomatterStatus status = matrix.begin();
  if (status != PROTOMATTER_OK) {
    for (;;)
      ;
  }

  Serial.begin(115200);
  /*
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  */

  //showIntro();
  showMainView();

  if (wifiEnabled) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
    wifiActive = true;
    uint8_t check = 10;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
      if (check == 0) {
        wifiActive = false;
        break;
      }
      check--;
    }
    if (wifiActive) {
      udp.begin(localPort);
      Serial.printf("Connected to WiFi\n");
      printWifiStatus();
    } else {
      Serial.printf("WiFi connection could not be established\n");
    }
  } else {
    wifiActive = false;
    Serial.println("Wifi not enabled");
  }

  // sync time via ntp
  if (WiFi.status() == WL_CONNECTED) {
    Wire.begin();
    ntp.begin();
    ntp.ruleDST("CST", Last, Sun, Mar, 2, 60);
    ntp.ruleSTD("CET", Last, Sun, Oct, 3, 60);
    uhr.setEpoch(ntp.epoch(), true);
    uhr.setClockMode(false);
    Serial.println("sync date time from ntp to rtc");
    timeSynced = true;
  } else {
    timeSynced = false;
    Serial.println("sync date time not possible. no wifi");
  }
  DateTime now;
  time_t t;
  struct tm* lt;
  char str[32];
  now = rtc.now();
  t = now.unixtime();
  lt = localtime(&t);
  strftime(str, sizeof str, "%Y.%m.%d:%H.%M.%S", lt);
  Serial.println(str);

  // MP3 Sound Modul
  soundActive = true;
  DF1201SSerial.begin(115200, SERIAL_8N1, RX, TX);
  if (!DF1201S.begin(DF1201SSerial)) {
    Serial.println("mp3 player could not be initialized");
    delay(1000);
    soundActive = false;
  }
  matrix.setTextWrap(false);
}




void loop(void) {

  if (wifiActive) {
    int packetSize = udp.parsePacket();

    if (packetSize) {
      IPAddress remoteIp = udp.remoteIP();
      Serial.print(remoteIp);
      Serial.print(", port ");
      Serial.println(udp.remotePort());
      int len = udp.read(receiveBuffer, 300);
      if (len > 0) {
        receiveBuffer[len] = 0;
      }

      String s = String(receiveBuffer);
      Serial.println(s);

      DeserializationError error = deserializeJson(doc, s);

      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      const char* source = doc["source"];
      if (strcmp(source, "controller") == 0) {
        const char* command = doc["command"];
        controllerIp = doc["ip"];

        if (strcmp(command, CHANGE_VIEW) == 0 && phase == IdlePhase) {
          const char* viewName = doc["view"];
          if (strcmp(viewName, HOME) == 0) {
            view = MainView;
            showMainView();
          } else if ((strcmp(viewName, TOURNAMENT) == 0 || strcmp(viewName, RE_SHOOT) == 0)) {
            view = TournamentView;
            initTimer = prepareTime;
            reshootActive = strcmp(viewName, RE_SHOOT) == 0;
            showTournamentView();
          } else if (strcmp(viewName, SHOOT_IN) == 0) {
            view = ShootInView;
            prepare = true;
            initTimer = shootInTime;
            showShootInView();
          } else if (strcmp(viewName, SETUP) == 0) {
            view = SetupView;
            showSetupView();
          } else if (strcmp(viewName, STATE) == 0) {
            view = StatusView;
            showStatusView();
          }
        } else if (strcmp(command, START_PAUSE_ACTION) == 0) {
          if (phase == IdlePhase) {
            phase = RunPhase;
            initialize = true;
            prepare = true;
            groupCount = 1;
            passes = 1;
          }
          hold = doc["values"]["hold"];
          Serial.printf("hold: %s\n", (hold ? "true" : "false"));
          sendStartPauseState(hold);
        } else if (strcmp(command, STOP) == 0 && phase == RunPhase) {
          hold = doc["values"]["hold"];
          phase = StopPhase;
        } else if (strcmp(command, CONFIG) == 0) {
          reset();
        } else if (strcmp(command, RESET) == 0) {
          reset();
        } else if (strcmp(command, EMERGENCY) == 0) {
        } else if (strcmp(command, VOLUME) == 0) {
        } else if (strcmp(command, TEST_SIGNAL) == 0 && phase == IdlePhase) {
        } else if (strcmp(command, STATE) == 0 && phase == IdlePhase) {
        }
      }
    }
  } else {
    // no wifi
    view = StatusView;
  }

  if (view == MainView) {
  } else if (view == ShootInView) {
    if (phase == RunPhase) {
      if (!hold) {
        if (initialize) {
          initialize = false;
          timer = initTimer;
          targetTime = millis() + 1000;
          timer2 = 60;
          drawCountdown(38, 2, timer);
          matrix.show();
        } else {
          if (millis() >= targetTime) {
            timer2 -= 1;
            drawCountdown(38, 2, timer);
            drawProgressBar(timer2);
            matrix.show();
            targetTime = millis() + 1000;
          }
          if (timer2 == 0) {
            timer2 = 60;
            timer -= 1;
            drawCountdown(38, 2, timer);
            drawProgressBar(timer2);
            matrix.show();
          }
          if (timer == 0) {
            phase = StopPhase;
          }
        }
      } else {
        //view = PauseView;
      }
    } else if (phase == StopPhase) {
      phase = IdlePhase;
      drawCountdown(38, 2, timer);
      drawProgressBar(timer2);
      matrix.show();
    } else {
    }

  } else if (view == TournamentView) {
    if (phase == RunPhase) {
      if (!hold) {
        if (initialize) {
          initialize = false;
          timer = initTimer;
          drawPasses(75, 2, passes);
          matrix.show();
          targetTime = millis() + 1000;
        } else {
          if (timer == 90) {
            drawTrafficLight(Green);
            drawCountdown(46, 3, timer);
            matrix.show();
          } else if (timer == 30) {
            drawTrafficLight(Yellow);
            drawCountdown(46, 3, timer);
            matrix.show();
          }
          if (millis() >= targetTime) {
            timer -= 1;
            drawCountdown(46, 3, timer);
            matrix.show();
            targetTime = millis() + 1000;
          }
          if (timer == 0) {
            if (prepare) {
              timer = reshootActive ? reshootTime : shootingTime;
              prepare = false;
            } else {
              phase = StopPhase;
            }
          }
        }
      } else {
        //view = PauseView;
      }
    } else if (phase == StopPhase) {
      phase = IdlePhase;
      prepare = true;
      drawCountdown(46, 3, timer);
      drawTrafficLight(Red);
      matrix.show();
    } else {
    }
  } else if (view == SetupView) {
    // nothing to do
  } else if (view == StatusView) {
    showStatusView();
    view = Dummy;
  } else if (view == PauseView) {
    //showPauseView();
  }


  // if the server's disconnected, stop the client:
  //if (!client.connected()) {
  //Serial.println();
  //Serial.println("disconnecting from server.");
  //client.stop();
}



void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void drawTrafficLight(uint16_t color) {
  matrix.fillRect(0, 0, 45, 64, color);
}

void drawGroup(uint8_t group) {
  matrix.setFont(&Abcd);
  matrix.setTextColor(0x5555FF);
  if (group == 0) {
    matrix.setCursor(46, 44);
    matrix.print("A");
    matrix.setCursor(109, 44);
    matrix.print("B");
  } else if (group = 1) {
    matrix.setCursor(46, 44);
    matrix.print("C");
    matrix.setCursor(109, 44);
    matrix.print("D");
  }
}

void drawCountdown(uint8_t xPos, uint8_t mask, uint16_t value) {
  matrix.fillRect(xPos, 0, 81, 44, 0);
  matrix.setFont(&Digi25x43);
  matrix.setTextColor(White);
  String num = String(value);
  uint8_t diff = mask - num.length();
  xPos = xPos + diff * 28;
  matrix.setCursor(xPos, 0);
  matrix.print(num);
}

void drawPasses(uint8_t xPos, uint8_t mask, uint16_t value) {
  matrix.fillRect(xPos, 46, 25, 17, 0);
  matrix.setFont(&Digi11x17_2);
  matrix.setTextColor(Orange);
  String num = String(value);
  uint8_t diff = mask - num.length();
  xPos = xPos + diff * 12;
  matrix.setCursor(xPos, 46);
  matrix.print(num);
}

void drawProgressBar(uint8_t sec) {
  matrix.drawRoundRect(33, 45, 62, 5, 2, White);
  matrix.fillRect(34, 46, 60, 3, 0);
  matrix.fillRect(34, 46, 60 - (60 - sec), 3, Red);
}

void showTournamentView() {
  matrix.fillScreen(0);
  drawTrafficLight(Red);
  drawCountdown(46, 3, initTimer);
  drawPasses(75, 2, 0);
  drawGroup(0);
  matrix.show();
}

void showShootInView() {
  matrix.fillScreen(0);
  drawCountdown(38, 2, initTimer);
  drawProgressBar(60);
  matrix.show();
}

void showSetupView() {
  matrix.fillScreen(0);
  matrix.setFont(&Font5x7Fixed);
  matrix.setCursor(0, 16);
  matrix.setTextColor(0x5555FF);
  matrix.print("Setup");
  matrix.show();
}

void showPauseView() {
  matrix.fillScreen(0);
  matrix.setFont(&Font5x7Fixed);
  matrix.setCursor(0, 16);
  matrix.setTextColor(0x5555FF);
  matrix.print("Pause");
  matrix.show();
}

void showStatusView() {
  matrix.fillScreen(0);
  matrix.setFont(&Matrix_icons);
  matrix.setTextColor(Green);
  matrix.setCursor(8, 24);
  matrix.print("A");
  showDisabledIcon(!wifiActive, 8);
  matrix.setTextColor(Blue);
  matrix.setCursor(40, 24);
  matrix.print("C");
  showDisabledIcon(!bluetoothActive, 40);
  matrix.setTextColor(Orange);
  matrix.setCursor(72, 24);
  matrix.print("B");
  showDisabledIcon(!timeSynced, 72);
  matrix.setTextColor(White);
  matrix.setCursor(104, 24);
  matrix.print("D");
  showDisabledIcon(!soundActive, 104);
  matrix.show();
}

void showDisabledIcon(bool disabled, int x) {
  if (disabled) {
    matrix.setTextColor(Red);
    matrix.setCursor(x, 24);
    matrix.print("E");
  }
}
void setActiveColor(bool active) {
  if (active) {
    matrix.setTextColor(Green);
  } else {
    matrix.setTextColor(Red);
  }
}

void showMainView() {
  matrix.fillScreen(0);
  matrix.setFont(&Font5x7Fixed);
  matrix.setCursor(5, 16);
  matrix.setTextColor(0x5555FF);
  matrix.print("BSV Eppinghofen 1743 e.V.");
  matrix.setCursor(33, 30);
  matrix.print("Archery Timer");
  matrix.show();
}

void reset() {
  prepareTime = doc["values"]["prepareTime"];
  arrowTime = doc["values"]["arrowTime"];
  arrowCount = doc["values"]["arrowCount"];
  reshootArrowCount = doc["values"]["reshootArrowCount"];
  shootingTime = arrowCount * arrowTime;
  reshootTime = reshootArrowCount * arrowTime;
  shootInTime = doc["values"]["shootInTime"];
  warnTime = doc["values"]["warnTime"];
  mode = doc["values"]["mode"];
  passes = doc["values"]["passes"];
}

/*
void sendResponse(char* command) {
  doc.clear();
  doc["command"] = command;
  doc["displayNo"] = displayNo;
  serializeJson(doc, responseBuffer);
  udp.beginPacket(controllerIp, udp.remotePort());
  udp.write(responseBuffer);
  udp.endPacket();
}
*/
void sendStartPauseState(bool hold) {
  doc.clear();
  doc["source"] = "display";
  doc["command"] = START_PAUSE_ACTION;
  doc["displayNo"] = displayNo;
  //Serial.printf("send to %s", controllerIp);
  //serializeJson(doc, Serial);
  udp.beginPacket(controllerIp, localPort);
  serializeJson(doc, udp);
  udp.println();
  udp.endPacket();
}

void showIntro() {
  r1 = 5;
  r2 = 4;
  r3 = 3;
  r4 = 2;
  r5 = 1;
  uint16_t c = 15;
  //while (r5 < 150) {
  matrix.fillCircle(64, 32, (uint16_t)r1, White);
  matrix.fillCircle(64, 32, (uint16_t)r2, Black);
  matrix.fillCircle(64, 32, (uint16_t)r3, Blue);
  matrix.fillCircle(64, 32, (uint16_t)r4, Red);
  matrix.fillCircle(64, 32, (uint16_t)r5, matrix.color565(c, c, 0));
  matrix.show();
  /*
    c -= 0.3;
    delay(40);
    r1 = r1 * 1.3;
    r2 = r2 * 1.3;
    r3 = r3 * 1.3;
    r4 = r4 * 1.3;
    r5 = r5 * 1.3;
  }
  */
  //matrix.fillScreen(0);
  //matrix.show();

  /*
  while (c > 0) {
    matrix.fillScreen(matrix.color565(c, c, 0));
    matrix.show();
    c -= 2;
    //delay(10);
  }
  */
  //showMainView();
}

/*
void sendResponse(uint8_t groupCurrent, uint8_t passCurrent, uint8_t groupCount, uint8_t prepareCurrent, uint8_t actionCurrent) {
  doc.clear();
  doc["command"] = "state";
  doc["displayNo"] = displayNo;
  doc["groupCurrent"] = groupCurrent;
  doc["passCurrent"] = passCurrent;
  doc["groupCount"] = groupCount;
  doc["prepareCurrent"] = prepareCurrent;
  doc["actionCurrent"] = actionCurrent;
  serializeJson(doc, responseBuffer);
  Udp.beginPacket(controllerIp, Udp.remotePort());
  Udp.write(responseBuffer);
  Udp.endPacket();
}
*/