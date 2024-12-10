#include "ESP.h"
#include "Arduino.h"
#include <Adafruit_Protomatter.h>
#include "AbcdFont.h"
#include "Digi11x17_2Font.h"
#include "Digi25x43Font.h"
#include "Font5x7Fixed.h"
#include "Matrix_iconsFont.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "DFRobotDFPlayerMini.h"
#include "Preferences.h"
#include "Auth.h"


#define RW_MODE false
#define RO_MODE true

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
            PauseView,
            FinalView };

enum Phase { IdlePhase = 200,
             RunPhase,
             StopPhase };


View view = MainView;
Phase phase = IdlePhase;

bool stop = false;
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
bool flashingPrepareLight = true;
bool trafficLightEnabled = true;
bool reverseMode = false;
bool synced = false;
bool initSync = true;
unsigned long currentTime;
unsigned long targetTime;
unsigned long flashingTargetTime;
const uint16_t flashingInterval = 500;
uint16_t prepareTime;
uint16_t shootingTime;
uint16_t arrowTime;
uint16_t arrowCount;
uint16_t reshootArrowCount;
uint16_t shootInTime;
uint16_t reshootTime;
uint16_t warnTime;
uint16_t groups;
uint16_t passes;
uint16_t initTimer;
uint16_t initTimer2;
uint16_t timer;
uint16_t timer2;
uint16_t currentPasses;
uint8_t currentGroup;
uint8_t groupSet;
uint8_t displayNo = 0;
uint16_t syncDelay;


const char* udpAddress = "255.255.255.255";
const char* controllerIp;
const char* CHANGE_VIEW = "change_view";
const char* TOURNAMENT_MODE = "tournament_mode";

const char* HOME = "home";
const char* SETUP = "setup";
const char* SHOOT_IN = "shoot_in";
const char* TOURNAMENT = "tournament";
const char* RE_SHOOT = "re_shoot";

const char* RUN = "run";
const char* PAUSE = "pause";
const char* STOP = "stop";
const char* RESET = "reset";
const char* EMERGENCY = "emergency";
const char* VOLUME = "volume";
const char* TEST_SIGNAL = "testsignal";
const char* STATE = "state";
const char* CONFIG = "config";
unsigned int localPort = 5005;

float r1, r2, r3, r4, r5;

char receiveBuffer[255];

//char ssid[] = "";
//char pass[] = "";


Adafruit_Protomatter matrix(
  128,                        // Width of matrix (or matrix chain) in pixels
  2,                          // Bit depth, 1-6
  2, rgbPins,                 // # of matrix chains, array of 6 RGB pins for each
  5, addrPins,                // # of address pins (height is inferred), array of pins
  clockPin, latchPin, oePin,  // Other matrix control pins
  false);                     // No double-buffering here (see "doublebuffer" example)

Preferences prefs;
WiFiUDP udp;
JsonDocument doc;

#define FPSerial Serial1

DFRobotDFPlayerMini myDFPlayer;


const uint16_t Red = matrix.color565(255, 0, 0);
const uint16_t Green = matrix.color565(0, 255, 0);
const uint16_t Blue = matrix.color565(0, 0, 255);
const uint16_t Yellow = matrix.color565(200, 200, 0);
const uint16_t White = matrix.color565(255, 255, 255);
const uint16_t Black = matrix.color565(0, 0, 0);
const uint16_t Orange = matrix.color565(243, 156, 18);
const uint16_t Orange2 = matrix.color565(230, 126, 34);
const uint16_t Gray = matrix.color565(120, 10, 10);


void setup(void) {

  currentPasses = 1;
  currentGroup = 1;

  ProtomatterStatus status = matrix.begin();
  if (status != PROTOMATTER_OK) {
    for (;;)
      ;
  }

  Serial.begin(115200);

  prefs.begin("archery_timer", RW_MODE);
  if (!prefs.getBool("wifi_set", false)) {
    prefs.putString("ssid", "wlan_ssid");
    prefs.putString("auth", "wlan_auth");
    prefs.putBool("wifi_set", true);

    Serial.println("Store new parameters to flash memory");
  } else {
    Serial.printf("Restore parameters from flash memory :%s / %s\n", prefs.getString("ssid"), prefs.getString("auth"));
  }
  prefs.end();



  if (wifiEnabled) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, pass);
    wifiActive = true;

    uint8_t check = 0;
    bool wasRestarted = false;
    prefs.begin("archery_timer", RW_MODE);
    wasRestarted = prefs.isKey("wasRestarted");
    prefs.end();
    while (WiFi.status() != WL_CONNECTED) {
      delay(200);
      Serial.print(".");
      if (check > 20) {
        prefs.begin("archery_timer", RW_MODE);
        prefs.putBool("wasRestarted", true);
        prefs.end();
        if (!wasRestarted) {
          ESP.restart();
        }
        wifiActive = false;
        break;
      }
      check++;
    }
    prefs.begin("archery_timer", RW_MODE);
    prefs.remove("wasRestarted");
    prefs.end();
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

  FPSerial.begin(9600, SERIAL_8N1, /*rx =*/RX, /*tx =*/TX);
  Serial.println();
  Serial.println(F("DFRobot DFPlayer Mini Demo"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  if (!myDFPlayer.begin(FPSerial, /*isACK = */ true, /*doReset = */ true)) {  //Use serial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while (true) {
      delay(0);  // Code to compatible with ESP8266 watch dog.
    }
  }
  Serial.println(F("DFPlayer Mini online."));

  myDFPlayer.volume(10);  //Set volume value. From 0 to 30

  soundActive = true;

  //showIntro();
  showMainView();

  matrix.setTextWrap(false);
}

void loop(void) {

  if (wifiActive) {

    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
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

      const char* source = doc["src"];
      if (strcmp(source, "controller") == 0) {
        const char* command = doc["cmd"];
        syncDelay = doc["sd"];
        if (strcmp(command, CHANGE_VIEW) == 0 && phase == IdlePhase) {
          const char* viewName = doc["v"];
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
        } else if (strcmp(command, RUN) == 0 && (phase == IdlePhase || phase == RunPhase)) {
          if (phase == IdlePhase) {
            phase = RunPhase;
            if (view == ShootInView) {
              timer2 = 0;
              timer = 0;
              initTimer = shootInTime;
            } else if (view == TournamentView) {
              timer = 0;
              initTimer = prepareTime;
            }
            initialize = true;
            prepare = true;
          }
          initSync = true;
          hold = doc["val"]["h"];
          Serial.println("RUN");
          sendRunState();
        } else if (strcmp(command, PAUSE) == 0 && phase == RunPhase) {
          initSync = true;
          hold = doc["val"]["h"];
          Serial.println("PAUSE");
          sendPauseState();
        } else if (strcmp(command, STOP) == 0 && phase == RunPhase) {
          initSync = true;
          phase = StopPhase;
          hold = doc["val"]["h"];
        } else if (strcmp(command, CONFIG) == 0) {
          configure();
        } else if (strcmp(command, RESET) == 0) {
          reset();
          if (view == ShootInView) {
            showShootInView();
          } else if (view == TournamentView) {
            showTournamentView();
          }
          sendResetState();
          sendStatus(1, 1, groups, 0, 0);
        } else if (strcmp(command, EMERGENCY) == 0) {
        } else if (strcmp(command, VOLUME) == 0) {
          myDFPlayer.volume(doc["val"]["val"]);
        } else if (strcmp(command, TEST_SIGNAL) == 0 && phase == IdlePhase) {
          myDFPlayer.play(1);
        }
        //Serial.printf("view:%d    phase:%d\n", view, phase);
      }
    }
  } else {
    // no wifi
    view = StatusView;
  }


  if (view == MainView) {
  } else if (view == ShootInView) {
    if (phase == RunPhase) {
      if (!synced) {
        if (initSync) {
          initSync = false;
          syncDelay = 50 - syncDelay;
          targetTime = millis() + syncDelay;
        }
        if (millis() >= targetTime) {
          synced = true;
        }
      } else {
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
      if (!synced) {
        if (initSync) {
          initSync = false;
          syncDelay = 50 - syncDelay;
          targetTime = millis() + syncDelay;
        }
        if (millis() >= targetTime) {
          synced = true;
        }
      } else {
        if (!hold) {
          if (initialize) {
            initialize = false;
            timer = initTimer;
            if (prepare) {
              sendStatus(currentGroup, currentPasses, groups, timer, shootingTime);
            } else {
              sendStatus(currentGroup, currentPasses, groups, 0, timer);
            }
            drawCountdown(46, 3, timer);
            drawPasses(75, 2, currentPasses);
            matrix.show();
            myDFPlayer.play(2);
            flashingTargetTime = millis() + flashingInterval;
            targetTime = millis() + 1000;
          } else {
            if (timer == warnTime && !prepare) {
              drawTrafficLight(Yellow, true);
              drawCountdown(46, 3, timer);
              matrix.show();
            } else if (timer == initTimer && !prepare) {
              drawTrafficLight(Green, true);
              drawCountdown(46, 3, timer);
              matrix.show();
            }
            if (millis() >= flashingTargetTime && prepare && flashingPrepareLight) {
              flashingTargetTime = millis() + flashingInterval;
              trafficLightEnabled = !trafficLightEnabled;
              drawTrafficLight(Red, trafficLightEnabled);
              matrix.show();
            }
            if (millis() >= targetTime) {
              timer -= 1;
              if (prepare) {
                sendStatus(currentGroup, currentPasses, groups, timer, shootingTime);
              } else {
                sendStatus(currentGroup, currentPasses, groups, 0, timer);
              }
              drawCountdown(46, 3, timer);
              matrix.show();
              targetTime = millis() + 1000;
            }

            if (timer == 0) {
              if (prepare) {
                myDFPlayer.play(1);
                initTimer = reshootActive ? reshootTime : shootingTime;
                timer = initTimer;
                drawCountdown(46, 3, timer);
                prepare = false;
              } else {
                phase = StopPhase;
              }
            }
          }
        } else {
          //view = PauseView;
        }
      }
    } else if (phase == StopPhase) {
      prepare = true;
      initialize = true;
      drawCountdown(46, 3, 0);
      drawTrafficLight(Red, true);
      if (groups > 1) {
        if (!reverseMode) {
          if (currentGroup == groups) {
            reverseMode = true;
            stop = true;
          } else {
            currentGroup += 1;
            groupSet++;
            initTimer = prepareTime;  // * 2;
            phase = RunPhase;
          }
        } else {
          if (currentGroup == 1) {
            reverseMode = false;
            stop = true;
          } else {
            currentGroup -= 1;
            groupSet++;
            initTimer = prepareTime;  // * 2;
            phase = RunPhase;
          }
        }
        drawGroup(currentGroup);
      } else {
        stop = true;
      }
      if (stop) {
        sendStop();
        stop = false;
        phase = IdlePhase;
        currentPasses += 1;
        groupSet = 0;
        if (currentPasses <= passes) {
          drawPasses(75, 2, currentPasses);
        }
        matrix.show();
        myDFPlayer.play(3);
        if (currentPasses > passes) {
          currentGroup = 1;
          currentPasses = 1;
          delay(2000);
          view = FinalView;
          myDFPlayer.play(5);
        }
      }

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
  } else if (view == FinalView) {
    showFinalView();
  }
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

void drawTrafficLight(uint16_t color, bool enabled) {
  if (enabled) {
    matrix.fillRect(0, 0, 45, 64, color);
  } else {
    matrix.fillRect(0, 0, 45, 64, Black);
  }
}

void drawGroup(uint8_t group) {
  matrix.setFont(&Abcd);

  matrix.fillRect(46, 44, 19, 20, 0);
  matrix.fillRect(108, 44, 19, 20, 0);

  matrix.setTextColor(0x5555FF);
  if (currentGroup == 1) {
    matrix.setCursor(46, 44);
    matrix.print("A");
    matrix.setCursor(109, 44);
    matrix.print("B");
  } else if (currentGroup == 2) {
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
  matrix.setTextColor(Orange2);
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
  drawTrafficLight(Red, true);
  drawCountdown(46, 3, initTimer);
  drawPasses(75, 2, currentPasses);
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

void showFinalView() {
  matrix.fillScreen(0);
  matrix.setFont(&Font5x7Fixed);
  matrix.setCursor(0, 16);
  matrix.setTextColor(0x5555FF);
  matrix.print("Das war es ...");
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
  matrix.setCursor(8, 14);
  matrix.print("A");
  showDisabledIcon(!wifiActive, 8);
  matrix.setTextColor(Blue);
  matrix.setCursor(40, 14);
  matrix.print("C");
  showDisabledIcon(!bluetoothActive, 40);
  matrix.setTextColor(Orange);
  matrix.setCursor(72, 14);
  matrix.print("B");
  showDisabledIcon(!timeSynced, 72);
  matrix.setTextColor(White);
  matrix.setCursor(104, 14);
  matrix.print("D");
  showDisabledIcon(!soundActive, 104);
  matrix.show();
}

void showDisabledIcon(bool disabled, int x) {
  if (disabled) {
    matrix.setTextColor(Red);
    matrix.setCursor(x, 14);
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
  //matrix.drawRGBBitmap()
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
  phase = IdlePhase;
  initialize = true;
  prepare = true;
  initSync = true;
  currentPasses = 1;
  currentGroup = 1;
  groupSet = 0;
  initTimer = prepareTime;
  timer = initTimer;
}

void configure() {
  prefs.begin("archery_timer", RW_MODE);
  prefs.putUShort("pt", doc["val"]["pt"]);
  prefs.putUShort("at", doc["val"]["at"]);
  prefs.putUShort("ac", doc["val"]["ac"]);
  prefs.putUShort("rac", doc["val"]["rac"]);
  prefs.putUShort("sit", doc["val"]["sit"]);
  prefs.putUShort("wt", doc["val"]["wt"]);
  prefs.putUShort("g", doc["val"]["g"]);
  prefs.putBool("fpl", doc["val"]["fpl"]);
  prefs.putUShort("p", doc["val"]["p"]);

  prepareTime = prefs.getUShort("pt");
  arrowTime = prefs.getUShort("at");
  arrowCount = prefs.getUShort("ac");
  reshootArrowCount = prefs.getUShort("rac");
  shootInTime = prefs.getUShort("sit");
  shootingTime = arrowCount * arrowTime;
  reshootTime = reshootArrowCount * arrowTime;
  warnTime = prefs.getUShort("wt");
  groups = prefs.getUShort("g");
  flashingPrepareLight = prefs.getBool("fpl");
  passes = prefs.getUShort("p");
  prefs.end();
}


void sendStop() {
  doc.clear();
  doc["src"] = "display";
  doc["cmd"] = STOP;
  doc["dn"] = displayNo;
  sendPacket(doc);
}

void sendRunState() {
  doc.clear();
  doc["src"] = "display";
  doc["cmd"] = RUN;
  doc["dn"] = displayNo;
  sendPacket(doc);
}

void sendPauseState() {
  doc.clear();
  doc["src"] = "display";
  doc["cmd"] = PAUSE;
  doc["dn"] = displayNo;
  sendPacket(doc);
}

void sendResetState() {
  doc.clear();
  doc["src"] = "display";
  doc["cmd"] = RESET;
  doc["dn"] = displayNo;
  sendPacket(doc);
}

void sendStatus(uint8_t groupCurrent, uint8_t passCurrent, uint8_t groupCount, uint8_t prepareCurrent, uint8_t actionCurrent) {
  doc["src"] = "display";
  doc["cmd"] = "state";
  doc["dn"] = displayNo;
  doc["cg"] = currentGroup;
  doc["cp"] = currentPasses;
  doc["g"] = groups;
  doc["cpt"] = prepareCurrent;
  doc["cat"] = actionCurrent;
  doc["gs"] = groupSet;
  sendPacket(doc);
  /*
  sprintf(responseBuffer, "src:%s|cmd:%s|dno:%d|cgp:%d|cps:%d|grp:%d|cpt:%d|cat:%d\0", "disp", "stat", displayNo, currentGroup, currentPasses, groups, prepareCurrent, actionCurrent);
  udp.beginPacket(controllerIp, localPort);
  int i = 0;
  while (responseBuffer[i] != 0) udp.write((uint8_t)responseBuffer[i++]);
  udp.endPacket();
*/
}

void sendPacket(JsonDocument d) {
  udp.beginPacket(udpAddress, localPort);
  serializeJson(doc, udp);
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
