#include "ESP.h"
#include "Arduino.h"
#include <Adafruit_Protomatter.h>
#include "AbcdFont.h"
#include "Digi11x17_2Font.h"
#include "Digi25x43Font.h"
#include "muMatrix8ptRegular.h"
#include "Matrix_iconsFont.h"
#include <WiFi.h>
#include <esp_system.h>
#include <esp_mac.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <time.h>
#include <sys/time.h>
 #include <DFPlayerMini_Fast.h>
#include "Preferences.h"
#include "logo1.h"

#if __has_include("Auth.h")
#include "Auth.h"
#else
#warning "Please create Auth.h with SSID & PASS defined"
#define WIFI_SSID "your-ssid"
#define WIFI_PASS "your-password"
#endif

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
            InfoView,
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
uint8_t signalOffset = 0;
uint32_t syncDelay;

// Every command carries how long it has been under way. A display starts the
// countdown at (moment of the key press + SYNC_LEAD), so a retransmission that
// arrives later still lands on the very same instant on every display.
const uint16_t SYNC_LEAD = 400;
unsigned long runAt = 0;

// Commands are acknowledged and repeated by the controller until every display
// has confirmed them. Sequence numbers only ever grow, so anything not newer
// than the last one carried out is a repeat - or a straggler that a newer
// command has already overtaken - and must not run again.
uint64_t lastCommandSeq = 0;
IPAddress controllerAddress;

// ---------------------------------------------------------------------------
// wall clock - a DS3231 on the i2c bus, kept in step by the app
// ---------------------------------------------------------------------------
// The chip keeps UTC through power cuts. At boot it seeds the system clock of
// the board; every heartbeat reports that clock and the app answers with
// "set_time" as soon as it is off. The countdown does not use any of this, it
// stays on millis().
#define DS3231_ADDRESS 0x68
#define DS3231_REG_TIME 0x00
#define DS3231_REG_STATUS 0x0F
#define DS3231_OSF 0x80  // oscillator stopped: the time in the chip is not valid
const uint64_t MIN_VALID_EPOCH_MS = 1700000000000ULL;  // anything before 2023 is not a real time
bool rtcPresent = false;
// The chip only takes whole seconds and restarts its second with the write, so
// the write is put off until the next full second begins.
bool rtcWritePending = false;
unsigned long rtcWriteAt = 0;
time_t rtcWriteSecond = 0;

// ---------------------------------------------------------------------------
// network state, identity and presence
// ---------------------------------------------------------------------------
enum NetState { NetOffline = 300,
                NetConnecting,
                NetOnline };

NetState netState = NetOffline;
unsigned long netNextAttempt = 0;
unsigned long netAttemptStarted = 0;
uint32_t netBackoff = 1000;
const uint32_t NET_BACKOFF_MIN = 1000;
const uint32_t NET_BACKOFF_MAX = 30000;
const uint32_t NET_CONNECT_TIMEOUT = 15000;
bool udpStarted = false;

// while the wifi is gone the status screen is shown, but only if nothing is
// running - a countdown must not be interrupted by a lost connection.
View viewBeforeOffline = MainView;
bool viewSuspendedByOffline = false;

// stable identity, derived from the wifi mac. displayNo is the number the user
// assigns from the app, the id stays the same for the life of the board.
char displayId[7] = "000000";
bool helloBootFlag = true;
uint8_t helloBurstsLeft = 0;
unsigned long helloNextTime = 0;
unsigned long heartbeatNextTime = 0;
const uint16_t HEARTBEAT_INTERVAL = 3000;

// every display announces itself; the lowest display number reports the
// tournament state back to the controller so n displays do not talk over
// each other.
#define MAX_PEERS 12
struct Peer {
  char id[7];
  uint8_t dn;
  unsigned long lastSeen;
};
Peer peers[MAX_PEERS];
uint8_t peerCount = 0;
const uint32_t PEER_TIMEOUT = 8000;
unsigned long masterCheckNextTime = 0;
const uint16_t MASTER_CHECK_INTERVAL = 500;


const char* udpAddress = "255.255.255.255";
const char* controllerIp;
const char* CHANGE_VIEW = "change_view";
const char* TOURNAMENT_MODE = "tournament_mode";

const char* HOME = "home";
const char* INFO = "info";
const char* SETUP = "setup";
const char* SHOOT_IN = "shoot_in";
const char* TOURNAMENT = "tournament";
const char* RE_SHOOT = "re_shoot";
const char* SIGNAL_SINE = "sine";
const char* SIGNAL_WHISTLE = "whistle";

const char* RUN = "run";
const char* PAUSE = "pause";
const char* STOP = "stop";
const char* RESET = "reset";
const char* EMERGENCY = "emergency";
const char* VOLUME = "volume";
const char* SIGNAL = "signal";
const char* TEST_SIGNAL = "testsignal";
const char* STATE = "state";
const char* CONFIG = "config";
const char* HELLO = "hello";
const char* HEARTBEAT = "heartbeat";
const char* DISCOVER = "discover";
const char* RESYNC = "resync";
const char* SET_DISPLAY_NO = "set_display_no";
const char* SET_TIME = "set_time";
const char* ACK = "ack";
unsigned int localPort = 5005;

float r1, r2, r3, r4, r5;

char receiveBuffer[512];

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
JsonDocument out;

#define FPSerial Serial1

DFPlayerMini_Fast myDFPlayer;


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



  randomSeed(esp_random());
  initDisplayIdentity();
  initClock();

  if (wifiEnabled) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);  // modem sleep makes the board miss udp broadcasts
    char hostname[32];
    snprintf(hostname, sizeof(hostname), "archerytimer-%s", displayId);
    WiFi.setHostname(hostname);
    // connecting is driven by networkLoop(), so a missing access point does
    // not block the boot and is retried forever.
    startWifiConnect();
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

  showIntro();
  showMainView();

  matrix.setTextWrap(false);
}

void loop(void) {

  networkLoop();
  clockLoop();

  if (wifiActive) {
    presenceLoop();

    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
      // never read more than the buffer holds - an oversized packet used to
      // corrupt memory here and took the board down.
      int len = udp.read(receiveBuffer, sizeof(receiveBuffer) - 1);
      if (len < 0) {
        len = 0;
      }
      receiveBuffer[len] = 0;

      DeserializationError error = deserializeJson(doc, receiveBuffer);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
      } else {
        const char* source = doc["src"];
        if (source != NULL) {
          if (strcmp(source, "display") == 0) {
            handlePeerPacket();
          } else if (strcmp(source, "controller") == 0 && isAddressedToMe()) {
            Serial.println(receiveBuffer);
            controllerAddress = udp.remoteIP();
            syncDelay = doc["sd"] | 0;
            handleControllerCommand();
          }
        }
      }
    }
  }

  // the peer table changes at heartbeat speed, so there is nothing to gain
  // from walking it on every pass of the loop - and a shorter loop keeps the
  // countdown tick from being pushed around.
  if ((long)(millis() - masterCheckNextTime) >= 0) {
    masterCheckNextTime = millis() + MASTER_CHECK_INTERVAL;
    updateMasterRole();
  }


  if (view == MainView) {
  } else if (view == ShootInView) {
    if (phase == RunPhase) {
      if (!synced) {
        // wait out the rest of the lead time, then all displays are in step
        if ((long)(millis() - runAt) >= 0) {
          synced = true;
          anchorCountdown();
        }
      } else {
        if (!hold) {
          if (initialize) {
            initialize = false;
            timer = initTimer;
            timer2 = 60;
            drawCountdown(38, 2, timer);
            matrix.show();
          } else {
            if (secondElapsed()) {
              timer2 -= 1;
              drawCountdown(38, 2, timer);
              drawProgressBar(timer2);
              matrix.show();
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
        // wait out the rest of the lead time, then all displays are in step
        if ((long)(millis() - runAt) >= 0) {
          synced = true;
          anchorCountdown();
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
            if ((long)(millis() - flashingTargetTime) >= 0 && prepare && flashingPrepareLight) {
              // Lit in the first half of every second, dark in the second half -
              // worked out from the common start instant instead of flipping
              // whatever this board happened to show last. Flipping kept two
              // displays in opposite phase for good once one of them had got
              // a single flip ahead (a pause, a late join, a restart).
              trafficLightEnabled = ((flashingTargetTime - runAt) / flashingInterval) % 2 == 0;
              flashingTargetTime += flashingInterval;
              drawTrafficLight(Red, trafficLightEnabled);
              matrix.show();
            }
            if (secondElapsed()) {
              timer -= 1;
              if (prepare) {
                sendStatus(currentGroup, currentPasses, groups, timer, shootingTime);
              } else {
                sendStatus(currentGroup, currentPasses, groups, 0, timer);
              }
              drawCountdown(46, 3, timer);
              matrix.show();
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
      // The change of group is not a change of pass: the next group is put up
      // and the display waits for the start of the tournament leader. It used
      // to run straight on into the preparation of the next group, which left
      // no room to call the archers up and change the target line.
      bool groupChanged = false;
      if (groups > 1) {
        if (!reverseMode) {
          if (currentGroup == groups) {
            reverseMode = true;
            stop = true;
          } else {
            currentGroup += 1;
            groupChanged = true;
          }
        } else {
          if (currentGroup == 1) {
            reverseMode = false;
            stop = true;
          } else {
            currentGroup -= 1;
            groupChanged = true;
          }
        }
        if (groupChanged) {
          groupSet++;
          initTimer = prepareTime;
          // the countdown of the group that is next, not the 0 the group
          // before it ended on - the line has to read this as "ready", not
          // as "over"
          timer = initTimer;
          synced = false;
          runAt = 0;
          phase = IdlePhase;
          drawCountdown(46, 3, timer);
        }
        drawGroup(currentGroup);
      } else {
        stop = true;
      }
      if (groupChanged) {
        // the shooting time of this group is over - the same signal the end of
        // a pass gets, otherwise the line would be left without one
        myDFPlayer.play(3);
        // the controller has to know that nothing is running any more, so its
        // start button comes back and the new group is shown. The times are
        // those of the group that is about to shoot, not zeroes - the progress
        // bar has to show a pass that is still ahead, the same as the display
        // showing its preparation time.
        sendStop();
        sendStatus(currentGroup, currentPasses, groups, timer, reshootActive ? reshootTime : shootingTime);
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



// ---------------------------------------------------------------------------
// the countdown clock
// ---------------------------------------------------------------------------

/**
 * Starts the one second grid at the instant all displays agreed on.
 *
 * Called the moment the lead time has run out - for the first second of a pass
 * and again after every pause, so a resumed countdown ticks on the same grid
 * as everybody else instead of on the grid of whenever this board happened to
 * notice.
 */
void anchorCountdown() {
  targetTime = runAt + 1000;
  flashingTargetTime = runAt + flashingInterval;
}

/**
 * True once per second of the running pass.
 *
 * The deadline is moved on by exactly one second, never to "one second from
 * now". Everything the board does between two ticks - sending a status packet,
 * answering a discover, taking in the heartbeats of the other displays,
 * driving the mp3 player - delays the moment this is reached by a few
 * milliseconds, and computing the next deadline from the current time added
 * that delay to every single second. The board doing the most work drifted the
 * most: the display that reports state sends a packet per second, the others
 * do not, and each one answers presence traffic on its own random schedule, so
 * after a few minutes the displays no longer showed the same number. Anchored
 * to the previous deadline the delays cannot pile up - a late tick is still a
 * late tick, but the one after it is back on time.
 */
bool secondElapsed() {
  if ((long)(millis() - targetTime) < 0) {
    return false;
  }
  targetTime += 1000;
  return true;
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
  matrix.setFont(&muMatrix8ptRegular);
  matrix.setCursor(0, 16);
  matrix.setTextColor(0x5555FF);
  matrix.print("Setup");
  matrix.show();
}

void showFinalView() {
  matrix.fillScreen(0);
  matrix.setFont(&muMatrix8ptRegular);
  matrix.setCursor(0, 16);
  matrix.setTextColor(0x5555FF);
  matrix.print("Das war es ...");
  matrix.show();
}

void showPauseView() {
  matrix.fillScreen(0);
  matrix.setFont(&muMatrix8ptRegular);
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
  matrix.drawRGBBitmap(0, 4, logo, 58, 55);
  matrix.setFont(&muMatrix8ptRegular);
  matrix.setTextColor(0x5555FF);
  matrix.setCursor(46, 8);
  matrix.print("BSV Eppinghoven");
  matrix.setCursor(67, 20);
  matrix.print("1743 e.V.");
  matrix.setTextColor(Orange2);
  matrix.setCursor(65, 40);
  matrix.print("Archery");
  matrix.setCursor(73, 52);
  matrix.print("Timer");
  matrix.show();
}

void reset() {
  phase = IdlePhase;
  initialize = true;
  prepare = true;
  synced = false;
  runAt = 0;
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


// only the master reports back, otherwise n displays would each drive the
// controller ui and overwrite each other.
void beginStateMessage(const char* command) {
  out.clear();
  out["src"] = "display";
  out["cmd"] = command;
  out["id"] = displayId;
  out["dn"] = displayNo;
}

void sendStop() {
  if (!isMaster) {
    return;
  }
  beginStateMessage(STOP);
  sendPacket(out);
}

void sendRunState() {
  if (!isMaster) {
    return;
  }
  beginStateMessage(RUN);
  sendPacket(out);
}

void sendPauseState() {
  if (!isMaster) {
    return;
  }
  beginStateMessage(PAUSE);
  sendPacket(out);
}

void sendResetState() {
  if (!isMaster) {
    return;
  }
  beginStateMessage(RESET);
  sendPacket(out);
}

void sendStatus(uint8_t groupCurrent, uint8_t passCurrent, uint8_t groupCount, uint8_t prepareCurrent, uint8_t actionCurrent) {
  if (!isMaster) {
    return;
  }
  beginStateMessage(STATE);
  out["cg"] = currentGroup;
  out["cp"] = currentPasses;
  out["g"] = groups;
  out["cpt"] = prepareCurrent;
  out["cat"] = actionCurrent;
  out["gs"] = groupSet;
  // the segment lengths of the controller progress bar - the display is the
  // only place that knows whether a re-shoot or a full pass is running
  out["spt"] = prepareTime;
  out["sat"] = reshootActive ? reshootTime : shootingTime;
  out["swt"] = warnTime;
  out["ph"] = (uint8_t)(phase == RunPhase ? 1 : 0);
  out["pr"] = prepare;
  out["h"] = hold;
  out["v"] = currentViewName();
  sendPacket(out);
  /*
  sprintf(responseBuffer, "src:%s|cmd:%s|dno:%d|cgp:%d|cps:%d|grp:%d|cpt:%d|cat:%d\0", "disp", "stat", displayNo, currentGroup, currentPasses, groups, prepareCurrent, actionCurrent);
  udp.beginPacket(controllerIp, localPort);
  int i = 0;
  while (responseBuffer[i] != 0) udp.write((uint8_t)responseBuffer[i++]);
  udp.endPacket();
*/
}

// acknowledgements go straight back to the controller, they are of no
// interest to the other displays
void sendAck(uint64_t seq) {
  if (!wifiActive || !udpStarted) {
    return;
  }
  out.clear();
  out["src"] = "display";
  out["cmd"] = ACK;
  out["id"] = displayId;
  out["dn"] = displayNo;
  out["seq"] = seq;
  udp.beginPacket(controllerAddress, localPort);
  serializeJson(out, udp);
  udp.endPacket();
}

void sendPacket(JsonDocument& d) {
  if (!wifiActive || !udpStarted) {
    return;
  }
  udp.beginPacket(udpAddress, localPort);
  serializeJson(d, udp);
  udp.endPacket();
}

void showIntro() {
  r1 = 5;
  r2 = 4;
  r3 = 3;
  r4 = 2;
  r5 = 1;
  uint16_t c = 140;
  while (r5 < 138) {
    matrix.fillCircle(64, 32, (uint16_t)r1, White);
    matrix.fillCircle(64, 32, (uint16_t)r2, Black);
    matrix.fillCircle(64, 32, (uint16_t)r3, Blue);
    matrix.fillCircle(64, 32, (uint16_t)r4, Red);
    matrix.fillCircle(64, 32, (uint16_t)r5, matrix.color565(c, c, 0));
    matrix.show();
    if (r5 > 10) {
      c = c - 9;
    }

    delay(40);
    r1 = r1 * 1.2;
    r2 = r2 * 1.2;
    r3 = r3 * 1.2;
    r4 = r4 * 1.2;
    r5 = r5 * 1.2;
  }
  Serial.printf("%d\n", c);
  matrix.fillScreen(0);
  matrix.show();
}


// ---------------------------------------------------------------------------
// controller commands
// ---------------------------------------------------------------------------

// a command without "dst" is meant for every display, a command carrying our
// id is meant for this one alone.
bool isAddressedToMe() {
  const char* dst = doc["dst"];
  return dst == NULL || strcmp(dst, displayId) == 0;
}

void handleControllerCommand() {
  const char* command = doc["cmd"];
  if (command == NULL) {
    return;
  }

  // The controller repeats a command until every display has confirmed it.
  // Confirm first - a lost acknowledgement must not stop the repeats - and
  // then skip a command that has already been carried out.
  uint64_t seq = doc["seq"] | (uint64_t)0;
  if (seq != 0) {
    sendAck(seq);
    if (seq <= lastCommandSeq) {
      return;
    }
    lastCommandSeq = seq;
  }
  if (strcmp(command, CHANGE_VIEW) == 0 && phase == IdlePhase) {
    const char* viewName = doc["v"];
    if (viewName == NULL) {
      return;
    }
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
    synced = false;
    runAt = millis() + (syncDelay < SYNC_LEAD ? SYNC_LEAD - syncDelay : 0);
    hold = doc["val"]["h"];
    Serial.println("RUN");
    sendRunState();
  } else if (strcmp(command, PAUSE) == 0 && phase == RunPhase) {
    hold = doc["val"]["h"];
    Serial.println("PAUSE");
    sendPauseState();
  } else if (strcmp(command, STOP) == 0 && phase == RunPhase) {
    phase = StopPhase;
    hold = doc["val"]["h"];
  } else if (strcmp(command, CONFIG) == 0) {
    configure();
  } else if (strcmp(command, INFO) == 0) {
    // INFO SCROLL TEXT
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
  } else if (strcmp(command, SIGNAL) == 0 && phase == IdlePhase) {
    const char* signalName = doc["val"]["val"];
    if (signalName == NULL) {
      return;
    }
    if (strcmp(signalName, SIGNAL_SINE) == 0) {
      signalOffset = 0;
    } else if (strcmp(signalName, SIGNAL_WHISTLE) == 0) {
      signalOffset = 3;
    } else {
      signalOffset = 0;
    }
  } else if (strcmp(command, TEST_SIGNAL) == 0 && phase == IdlePhase) {
    myDFPlayer.play(1);
  } else if (strcmp(command, DISCOVER) == 0) {
    scheduleHelloBurst(false);
  } else if (strcmp(command, RESYNC) == 0) {
    applyResync();
  } else if (strcmp(command, SET_DISPLAY_NO) == 0) {
    setDisplayNo(doc["val"]["dn"] | displayNo);
  } else if (strcmp(command, SET_TIME) == 0) {
    applySetTime();
  }
}


// ---------------------------------------------------------------------------
// identity
// ---------------------------------------------------------------------------

// the id is derived from the wifi mac and is stable for the life of the board;
// the display number can be changed from the app and is kept in flash.
void initDisplayIdentity() {
  // esp_read_mac() reads the efuse and works before the wifi driver is up.
  // WiFi.macAddress() does not - it leaves the buffer untouched, which gave
  // every board the same id and made the app see them as one display.
  uint8_t mac[6] = { 0, 0, 0, 0, 0, 0 };
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
    WiFi.macAddress(mac);
  }
  snprintf(displayId, sizeof(displayId), "%02x%02x%02x", mac[3], mac[4], mac[5]);

  prefs.begin("archery_timer", RW_MODE);
  displayNo = prefs.getUChar("dn", 1);
  prefs.end();

  Serial.printf("display id %s, display no %d\n", displayId, displayNo);
}

void setDisplayNo(uint8_t no) {
  if (no == 0 || no == displayNo) {
    return;
  }
  displayNo = no;
  prefs.begin("archery_timer", RW_MODE);
  prefs.putUChar("dn", displayNo);
  prefs.end();
  Serial.printf("display no set to %d\n", displayNo);
  scheduleHelloBurst(false);
}

const char* currentViewName() {
  if (view == ShootInView) {
    return SHOOT_IN;
  }
  if (view == TournamentView) {
    return reshootActive ? RE_SHOOT : TOURNAMENT;
  }
  if (view == SetupView) {
    return SETUP;
  }
  if (view == StatusView) {
    return STATE;
  }
  return HOME;
}

// ---------------------------------------------------------------------------
// wifi state machine - reconnects for as long as it takes
// ---------------------------------------------------------------------------

void startWifiConnect() {
  Serial.printf("connecting to ssid %s\n", WIFI_SSID);
  netState = NetConnecting;
  netAttemptStarted = millis();
  WiFi.disconnect(false, false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void networkLoop() {
  if (!wifiEnabled) {
    return;
  }

  unsigned long now = millis();

  switch (netState) {
    case NetOffline:
      if ((long)(now - netNextAttempt) >= 0) {
        startWifiConnect();
      }
      break;

    case NetConnecting:
      if (WiFi.status() == WL_CONNECTED) {
        goOnline();
      } else if (now - netAttemptStarted > NET_CONNECT_TIMEOUT) {
        Serial.printf("wifi connect timed out, next try in %lu ms\n", (unsigned long)netBackoff);
        WiFi.disconnect(false, false);
        netState = NetOffline;
        netNextAttempt = now + netBackoff;
        netBackoff = netBackoff < NET_BACKOFF_MAX ? netBackoff * 2 : NET_BACKOFF_MAX;
      }
      break;

    case NetOnline:
      if (WiFi.status() != WL_CONNECTED) {
        goOffline();
      }
      break;
  }
}

void goOnline() {
  netState = NetOnline;
  netBackoff = NET_BACKOFF_MIN;
  wifiActive = true;

  // the socket still holds the old interface address, so it has to be reopened
  // for every fresh lease - without this nothing arrives after a reconnect.
  if (udpStarted) {
    udp.stop();
  }
  udp.begin(localPort);
  udpStarted = true;

  Serial.println("wifi connected");
  printWifiStatus();

  scheduleHelloBurst(false);
  heartbeatNextTime = millis() + HEARTBEAT_INTERVAL;

  restoreViewAfterOffline();
}

void goOffline() {
  Serial.println("wifi connection lost");
  netState = NetOffline;
  wifiActive = false;
  netNextAttempt = millis();  // try again at once, then back off
  peerCount = 0;
  suspendViewWhileOffline();
}

void suspendViewWhileOffline() {
  // a running countdown must keep running, the status screen is only shown
  // when nothing is going on.
  if (phase == IdlePhase && !viewSuspendedByOffline) {
    viewBeforeOffline = view;
    viewSuspendedByOffline = true;
    view = StatusView;
  }
}

void restoreViewAfterOffline() {
  if (viewSuspendedByOffline) {
    viewSuspendedByOffline = false;
    view = viewBeforeOffline;
    redrawCurrentView();
  }
}

void redrawCurrentView() {
  if (view == ShootInView) {
    showShootInView();
  } else if (view == TournamentView) {
    showTournamentView();
  } else if (view == SetupView) {
    showSetupView();
  } else if (view == StatusView) {
    showStatusView();
  } else if (view == FinalView) {
    showFinalView();
  } else {
    showMainView();
  }
}

// ---------------------------------------------------------------------------
// presence - hello on every (re)connect, heartbeat while online
// ---------------------------------------------------------------------------

void scheduleHelloBurst(bool boot) {
  helloBootFlag = helloBootFlag || boot;
  helloBurstsLeft = 3;
  // stagger the answers of n displays so they do not collide on air
  helloNextTime = millis() + random(0, 400);
}

void presenceLoop() {
  unsigned long now = millis();

  if (helloBurstsLeft > 0 && (long)(now - helloNextTime) >= 0) {
    sendHello(helloBootFlag);
    helloBurstsLeft--;
    helloNextTime = now + 150;
    if (helloBurstsLeft == 0) {
      helloBootFlag = false;
    }
  }

  if ((long)(now - heartbeatNextTime) >= 0) {
    sendHeartbeat();
    heartbeatNextTime = now + HEARTBEAT_INTERVAL + random(0, 300);
  }
}

void fillPresence(JsonDocument& d, const char* command) {
  d.clear();
  d["src"] = "display";
  d["cmd"] = command;
  d["id"] = displayId;
  d["dn"] = displayNo;
  d["ip"] = WiFi.localIP().toString();
  d["rssi"] = WiFi.RSSI();
  d["up"] = (uint32_t)(millis() / 1000);
  d["m"] = isMaster;
  // the wall clock in ms since 1970 (UTC), 0 as long as it was never set - the
  // app compares it with its own and sends the time when the two disagree
  d["t"] = timeSynced ? clockNowMs() : (uint64_t)0;

  // What this display is doing, in the same fields a state packet uses. The
  // app receives nothing while it is in the background, so after a wake-up it
  // rebuilds its picture from this - instead of pushing what it remembers from
  // before onto displays that have been counting correctly all along.
  d["v"] = currentViewName();
  d["ph"] = (uint8_t)(phase == RunPhase ? 1 : 0);
  d["pr"] = prepare;
  d["h"] = hold;
  d["cg"] = currentGroup;
  d["cp"] = currentPasses;
  d["gs"] = groupSet;
  d["cpt"] = (uint16_t)(phase == RunPhase && prepare ? timer : 0);
  d["cat"] = (uint16_t)(phase == RunPhase && !prepare ? timer : 0);
  d["spt"] = prepareTime;
  d["sat"] = reshootActive ? reshootTime : shootingTime;
  d["swt"] = warnTime;
}

void sendHello(bool boot) {
  fillPresence(out, HELLO);
  out["boot"] = boot;
  sendPacket(out);
  Serial.printf("hello sent (boot=%d)\n", boot ? 1 : 0);
}

void sendHeartbeat() {
  fillPresence(out, HEARTBEAT);
  sendPacket(out);
}

// ---------------------------------------------------------------------------
// peers and master election
// ---------------------------------------------------------------------------

void handlePeerPacket() {
  const char* id = doc["id"];
  const char* command = doc["cmd"];
  if (id == NULL || command == NULL) {
    return;
  }
  if (strcmp(id, displayId) == 0) {
    return;  // our own broadcast comes back to us
  }
  if (strcmp(command, HELLO) != 0 && strcmp(command, HEARTBEAT) != 0) {
    return;
  }
  touchPeer(id, doc["dn"] | 1);
}

void touchPeer(const char* id, uint8_t dn) {
  unsigned long now = millis();
  for (uint8_t i = 0; i < peerCount; i++) {
    if (strcmp(peers[i].id, id) == 0) {
      peers[i].dn = dn;
      peers[i].lastSeen = now;
      return;
    }
  }
  if (peerCount < MAX_PEERS) {
    strncpy(peers[peerCount].id, id, sizeof(peers[peerCount].id) - 1);
    peers[peerCount].id[sizeof(peers[peerCount].id) - 1] = 0;
    peers[peerCount].dn = dn;
    peers[peerCount].lastSeen = now;
    peerCount++;
    Serial.printf("peer %s (display %d) seen\n", id, dn);
  }
}

// the display with the lowest display number reports the state; equal numbers
// are resolved by the unique display id, so exactly one master remains.
void updateMasterRole() {
  unsigned long now = millis();

  for (uint8_t i = 0; i < peerCount;) {
    if (now - peers[i].lastSeen > PEER_TIMEOUT) {
      Serial.printf("peer %s timed out\n", peers[i].id);
      peers[i] = peers[peerCount - 1];
      peerCount--;
    } else {
      i++;
    }
  }

  bool master = true;
  for (uint8_t i = 0; i < peerCount; i++) {
    if (peers[i].dn < displayNo || (peers[i].dn == displayNo && strcmp(peers[i].id, displayId) < 0)) {
      master = false;
      break;
    }
  }

  if (master != isMaster) {
    isMaster = master;
    Serial.printf("master role: %s\n", isMaster ? "yes" : "no");
  }
}

// ---------------------------------------------------------------------------
// late join - a display that was out of range picks up the running pass
// ---------------------------------------------------------------------------

void applyResync() {
  JsonObject val = doc["val"];
  if (val.isNull()) {
    return;
  }

  bool run = val["r"] | false;
  uint16_t remaining = val["t"] | 0;

  // A display that is running keeps its own clock and cannot drift, so it is
  // the authority on its own countdown. A resync that would drop it into a
  // running pass is therefore refused while it is already running - only one
  // that stops it, or one that arrives while nothing is going on, is applied.
  // This is what used to take the displays out of step: a controller that had
  // been asleep answered its own discover with the state of the moment the
  // phone was put away and rewound every display to it.
  if (phase == RunPhase && run && remaining > 0) {
    Serial.println("resync ignored - this display is already running");
    return;
  }

  const char* wantedView = val["v"] | HOME;
  reshootActive = strcmp(wantedView, RE_SHOOT) == 0;

  if (strcmp(wantedView, SHOOT_IN) == 0) {
    view = ShootInView;
  } else if (strcmp(wantedView, TOURNAMENT) == 0 || reshootActive) {
    view = TournamentView;
  } else if (strcmp(wantedView, SETUP) == 0) {
    view = SetupView;
  } else if (strcmp(wantedView, STATE) == 0) {
    view = StatusView;
  } else {
    view = MainView;
  }
  viewSuspendedByOffline = false;

  hold = val["h"] | false;
  currentGroup = val["cg"] | 1;
  currentPasses = val["cp"] | 1;
  groupSet = val["gs"] | 0;
  prepare = val["pr"] | true;

  if (run && remaining > 0) {
    phase = RunPhase;
    initialize = false;
    synced = true;
    runAt = millis();
    timer = remaining;
    if (view == ShootInView) {
      initTimer = shootInTime;
      timer2 = 60;
    } else {
      initTimer = prepare ? prepareTime : (reshootActive ? reshootTime : shootingTime);
    }
    anchorCountdown();
  } else {
    phase = IdlePhase;
    initialize = true;
    prepare = true;
    synced = false;
    initTimer = (view == ShootInView) ? shootInTime : prepareTime;
    timer = initTimer;
  }

  redrawCurrentView();
  if (phase == RunPhase) {
    if (view == TournamentView) {
      drawCountdown(46, 3, timer);
      drawPasses(75, 2, currentPasses);
      matrix.show();
    } else if (view == ShootInView) {
      drawCountdown(38, 2, timer);
      drawProgressBar(timer2);
      matrix.show();
    }
  }
  Serial.printf("resync: view=%s run=%d remaining=%d\n", wantedView, run ? 1 : 0, remaining);
}

// ---------------------------------------------------------------------------
// wall clock - DS3231
// ---------------------------------------------------------------------------

uint8_t bcdToBin(uint8_t value) {
  return (value >> 4) * 10 + (value & 0x0F);
}

uint8_t binToBcd(uint8_t value) {
  return ((value / 10) << 4) | (value % 10);
}

bool rtcRead(uint8_t reg, uint8_t* buffer, uint8_t length) {
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  if (Wire.requestFrom((uint8_t)DS3231_ADDRESS, length) != length) {
    return false;
  }
  for (uint8_t i = 0; i < length; i++) {
    buffer[i] = Wire.read();
  }
  return true;
}

bool rtcWrite(uint8_t reg, const uint8_t* buffer, uint8_t length) {
  Wire.beginTransmission(DS3231_ADDRESS);
  Wire.write(reg);
  Wire.write(buffer, length);
  return Wire.endTransmission() == 0;
}

// days from 1970-01-01 to the given date of the gregorian calendar
int32_t daysFromCivil(int32_t year, uint8_t month, uint8_t day) {
  if (month <= 2) {
    year -= 1;
  }
  int32_t era = year / 400;
  uint32_t yearOfEra = year - era * 400;
  uint32_t dayOfYear = (153 * (month > 2 ? month - 3 : month + 9) + 2) / 5 + day - 1;
  uint32_t dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + (int32_t)dayOfEra - 719468;
}

// the time in the chip as seconds since 1970 (UTC)
bool rtcReadEpoch(time_t* epoch) {
  uint8_t r[7];
  if (!rtcRead(DS3231_REG_TIME, r, sizeof(r))) {
    return false;
  }
  uint8_t second = bcdToBin(r[0] & 0x7F);
  uint8_t minute = bcdToBin(r[1] & 0x7F);
  uint8_t hour = bcdToBin(r[2] & 0x3F);  // always written in 24 hour mode
  uint8_t day = bcdToBin(r[4] & 0x3F);
  uint8_t month = bcdToBin(r[5] & 0x1F);
  int32_t year = 2000 + bcdToBin(r[6]);
  *epoch = (time_t)daysFromCivil(year, month, day) * 86400 + hour * 3600L + minute * 60L + second;
  return true;
}

bool rtcWriteEpoch(time_t epoch) {
  struct tm t;
  gmtime_r(&epoch, &t);
  uint8_t r[7] = {
    binToBcd(t.tm_sec),
    binToBcd(t.tm_min),
    binToBcd(t.tm_hour),
    (uint8_t)(t.tm_wday + 1),
    binToBcd(t.tm_mday),
    binToBcd(t.tm_mon + 1),
    binToBcd(t.tm_year % 100)
  };
  if (!rtcWrite(DS3231_REG_TIME, r, sizeof(r))) {
    return false;
  }
  // the time is valid from here on - take back the "oscillator stopped" mark
  uint8_t status;
  if (rtcRead(DS3231_REG_STATUS, &status, 1)) {
    status &= ~DS3231_OSF;
    rtcWrite(DS3231_REG_STATUS, &status, 1);
  }
  return true;
}

uint64_t clockNowMs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
}

/**
 * Takes the time over from the clock chip.
 *
 * A missing chip does not stop the boot: the display then simply has no time
 * until the app sends one, and loses it again with the next power cut.
 */
void initClock() {
  Wire.begin();
  Wire.setTimeOut(50);

  uint8_t status;
  if (!rtcRead(DS3231_REG_STATUS, &status, 1)) {
    Serial.println("DS3231 not found");
    return;
  }
  rtcPresent = true;

  if (status & DS3231_OSF) {
    Serial.println("DS3231 lost power, waiting for the time from the app");
    return;
  }

  // The chip only tells whole seconds. Waiting for the second to change gives
  // the exact start of one, otherwise the board would be up to a second late.
  time_t first;
  time_t epoch;
  if (!rtcReadEpoch(&first)) {
    return;
  }
  epoch = first;
  unsigned long waitStarted = millis();
  while (epoch == first && millis() - waitStarted < 1100) {
    delay(2);
    if (!rtcReadEpoch(&epoch)) {
      return;
    }
  }

  if ((uint64_t)epoch * 1000ULL < MIN_VALID_EPOCH_MS) {
    Serial.println("DS3231 holds no valid time, waiting for the time from the app");
    return;
  }

  struct timeval tv = { epoch, 0 };
  settimeofday(&tv, NULL);
  timeSynced = true;
  Serial.printf("time taken from DS3231: %lu\n", (unsigned long)epoch);
}

// the app found the clock of this display to be off and sent its own
void applySetTime() {
  uint64_t now = doc["val"]["t"] | (uint64_t)0;
  if (now < MIN_VALID_EPOCH_MS) {
    return;
  }
  // "t" is the time the command was created, "sd" how long it has been under
  // way since - a repeat that arrives late still sets the right time
  now += syncDelay;

  struct timeval tv = { (time_t)(now / 1000), (suseconds_t)((now % 1000) * 1000) };
  settimeofday(&tv, NULL);
  timeSynced = true;

  rtcWriteSecond = (time_t)(now / 1000) + 1;
  rtcWriteAt = millis() + (1000 - (uint16_t)(now % 1000));
  rtcWritePending = rtcPresent;
  Serial.printf("time set by the app: %lu\n", (unsigned long)(now / 1000));
}

void clockLoop() {
  if (!rtcWritePending || (long)(millis() - rtcWriteAt) < 0) {
    return;
  }
  rtcWritePending = false;
  if (!rtcWriteEpoch(rtcWriteSecond)) {
    Serial.println("writing the time to the DS3231 failed");
  }
}
