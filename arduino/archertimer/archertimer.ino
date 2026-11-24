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
#include <ArduinoJson.h>
#include <Wire.h>
#include <time.h>
#include <sys/time.h>
 #include <DFPlayerMini_Fast.h>
#include "Preferences.h"
#include "logo1.h"
// The displays and the controllers talk by ESP-NOW, straight from board to
// board without an access point. All of them have to be on the same wifi
// channel: LINK_CHANNEL in espnow_link.h.
#include "espnow_link.h"

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
            FinalView,
            LunchView };

enum Phase { IdlePhase = 200,
             RunPhase,
             StopPhase };


View view = MainView;
Phase phase = IdlePhase;

bool stop = false;
bool hold = false;
bool prepare = true;
// when the lunch break is over, "HH:MM" as set in the app
char lunchTime[6] = "13:00";
// the same instant in seconds since 1970 (UTC), 0 = unknown. The display has
// no time zone, so the app works out the instant and sends it along.
uint32_t lunchEnd = 0;
// true as long as the end of the break is still ahead - only then it is
// announced with a signal
bool lunchArmed = false;
bool lunchOver = false;
bool lunchBlinkOn = true;
unsigned long lunchNextCheck = 0;
// The blinking after the end of the break runs on a beat given by the master
// display. The wall clocks only agree to within two seconds - the app does not
// correct less - so they cannot keep a half second blink in step.
unsigned long lunchBlinkAnchor = 0;  // millis() of a moment the time went on
unsigned long lunchBeatNext = 0;     // when the master sends its next beat
// how long a display waits for the beat of the master before it declares the
// break over on its own clock
const uint32_t LUNCH_MASTER_GRACE = 3;
// set when the tournament leader ended the shooting time with the stop key
bool stopRequested = false;
// Time the triple signal of the last pass gets before the final view comes up
// with its own sound - the player can only play one at a time, so starting
// the next one cuts the signal off.
const unsigned long FINAL_VIEW_DELAY = 5000;
bool finalViewPending = false;
unsigned long finalViewAt = 0;
// breathing space between such a stop and the start of the next group
const unsigned long GROUP_CHANGE_DELAY = 500;
bool initialize = true;
bool wifiEnabled = true;
bool bluetoothActive = false;
bool wifiActive = false;
bool timeSynced = false;
// Local time minus UTC in seconds, as told by the app. The clock of the board
// runs in UTC and the board knows no time zone of its own.
int32_t tzOffset = 0;
// the second of the day the status view shows, -1 = none drawn yet
int32_t statusSecondShown = -1;
unsigned long statusNextCheck = 0;
bool soundActive = false;
bool isMaster = true;
bool reshootActive = false;
// The safety stop: the countdown is held and every display shows the warning
// until the controller releases it again.
bool emergency = false;
// whether the pass was already paused when the safety stop came
bool holdBeforeEmergency = false;
// The mp3 the player plays for the safety stop, 0006*.mp3 on the sd card. The
// warning tone itself is still to be chosen.
const uint8_t SOUND_EMERGENCY = 6;
// The warning blinks: red letters on black, then black letters on red. All
// displays count from the moment the key was pressed, so they blink in step.
const uint16_t EMERGENCY_BLINK_INTERVAL = 500;
unsigned long emergencyBlinkAnchor = 0;
bool emergencyBlinkOn = false;
unsigned long emergencyNextCheck = 0;
bool flashingPrepareLight = true;
bool trafficLightEnabled = true;
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
// the controller the last command came from - it gets the acknowledgement
uint8_t controllerMac[6];

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


const char* CHANGE_VIEW = "change_view";
const char* TOURNAMENT_MODE = "tournament_mode";

const char* HOME = "home";
const char* LUNCH = "lunch";
const char* BLINK = "blink";
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
// the packet "doc" was read from - the document points into its text
LinkPacket packet;


Adafruit_Protomatter matrix(
  128,                        // Width of matrix (or matrix chain) in pixels
  2,                          // Bit depth, 1-6
  2, rgbPins,                 // # of matrix chains, array of 6 RGB pins for each
  5, addrPins,                // # of address pins (height is inferred), array of pins
  clockPin, latchPin, oePin,  // Other matrix control pins
  false);                     // No double-buffering here (see "doublebuffer" example)

Preferences prefs;
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

  randomSeed(esp_random());
  initDisplayIdentity();
  initClock();

  if (wifiEnabled) {
    wifiActive = linkBegin();
    if (wifiActive) {
      // tells the controllers that this display is there and has just started
      scheduleHelloBurst(false);
      heartbeatNextTime = millis() + HEARTBEAT_INTERVAL;
    } else {
      Serial.println("radio did not come up - no controller can reach this display");
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

  showIntro();
  showMainView();

  matrix.setTextWrap(false);
}

void loop(void) {

  clockLoop();

  if (wifiActive) {
    presenceLoop();
    // a few per pass, so a burst of packets cannot push the countdown tick
    // around
    for (uint8_t i = 0; i < 4 && linkReceive(packet); i++) {
      handleRadioPacket();
    }
  }

  // the peer table changes at heartbeat speed, so there is nothing to gain
  // from walking it on every pass of the loop - and a shorter loop keeps the
  // countdown tick from being pushed around.
  if ((long)(millis() - masterCheckNextTime) >= 0) {
    masterCheckNextTime = millis() + MASTER_CHECK_INTERVAL;
    updateMasterRole();
  }


  // a start or a change of view in the meantime: the final view is off
  if (finalViewPending && (view != TournamentView || phase != IdlePhase)) {
    finalViewPending = false;
  }

  if (emergency) {
    // the warning stays on the screen and blinks, nothing counts
    emergencyLoop();
  } else if (view == MainView) {
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
              sendStatus(currentGroup, currentPasses, groups, timer, passShootingTime());
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
              // a re-shoot of a single arrow can be shorter than the warn time
              drawTrafficLight(initTimer <= warnTime ? Yellow : Green, true);
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
                sendStatus(currentGroup, currentPasses, groups, timer, passShootingTime());
              } else {
                sendStatus(currentGroup, currentPasses, groups, 0, timer);
              }
              drawCountdown(46, 3, timer);
              matrix.show();
            }

            if (timer == 0) {
              if (prepare) {
                myDFPlayer.play(1);
                initTimer = passShootingTime();
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
      // The change of group is not the end of the pass: no triple signal, the
      // display goes straight on into the preparation time of the next group,
      // which opens with the double signal. Only when every group has shot
      // does the pass end with the triple signal.
      bool groupChanged = false;
      if (reshootActive) {
        // A re-shoot is a single run for the archers who have arrows to make
        // up: the groups do not take turns and it ends with the triple signal.
        stop = true;
      } else if (groups > 1) {
        // the groups take turns in who shoots first, see firstGroupOfPass()
        if (currentPasses % 2 == 1) {
          if (currentGroup >= groups) {
            stop = true;
          } else {
            currentGroup += 1;
            groupChanged = true;
          }
        } else {
          if (currentGroup <= 1) {
            stop = true;
          } else {
            currentGroup -= 1;
            groupChanged = true;
          }
        }
        if (groupChanged) {
          groupSet++;
          initTimer = prepareTime;
          timer = initTimer;
          // Keep running: the initialize step of the run phase plays the
          // double signal, shows the preparation time and reports the new
          // group to the controller.
          hold = false;
          phase = RunPhase;
          // Stay on the one second grid all displays share, but drop the
          // ticks that are already over - after a stop out of a pause, and
          // for the flashing light, which stood still during the shooting
          // time. Otherwise they would all be caught up in a burst.
          if (stopRequested) {
            // stopped by hand: wait half a second before the next group
            // starts. The run phase waits for runAt and then anchors the
            // one second grid anew, the same as after a start.
            synced = false;
            runAt = millis() + GROUP_CHANGE_DELAY;
          } else if (synced) {
            while ((long)(millis() - targetTime) >= 0) {
              targetTime += 1000;
            }
            while ((long)(millis() - flashingTargetTime) >= 0) {
              flashingTargetTime += flashingInterval;
            }
          }
          drawCountdown(46, 3, timer);
        }
        drawGroup(currentGroup);
      } else {
        stop = true;
      }
      stopRequested = false;
      if (stop) {
        sendStop();
        stop = false;
        phase = IdlePhase;
        // the arrows of a re-shoot belong to the pass that was just shot, so
        // it does not count as a pass of its own
        if (!reshootActive) {
          currentPasses += 1;
          groupSet = 0;
        }
        if (currentPasses <= passes) {
          drawPasses(75, 2, currentPasses);
        }
        matrix.show();
        myDFPlayer.play(3);
        if (currentPasses > passes) {
          currentGroup = 1;
          currentPasses = 1;
          // not waited out here: the board has to go on answering the
          // controller and the other displays in the meantime
          finalViewPending = true;
          finalViewAt = millis() + FINAL_VIEW_DELAY;
        }
      }

      matrix.show();
    } else if (finalViewPending && (long)(millis() - finalViewAt) >= 0) {
      finalViewPending = false;
      view = FinalView;
      myDFPlayer.play(5);
    }
  } else if (view == SetupView) {
    // nothing to do
  } else if (view == StatusView) {
    statusLoop();
  } else if (view == PauseView) {
    //showPauseView();
  } else if (view == FinalView) {
    showFinalView();
  } else if (view == LunchView) {
    lunchLoop();
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

/** one frame of the warning: the red screen, or the red letters on black */
void drawEmergencyFrame(bool lit) {
  matrix.fillScreen(lit ? Red : Black);
  matrix.setFont(&muMatrix8ptRegular);
  matrix.setTextColor(lit ? Black : Red);
  printCentered("Sicherheit", 24);
  printCentered("Emergency", 48);
  matrix.show();
}

/**
 * Puts the warning up. The anchor is the millis() of the moment the blinking
 * is counted from - the key press for a command, so every display is in step.
 */
void showEmergencyView(unsigned long anchor) {
  emergencyBlinkAnchor = anchor;
  emergencyBlinkOn = true;
  drawEmergencyFrame(true);
}

/** called on every pass of the loop while the safety stop is set */
void emergencyLoop() {
  if ((long)(millis() - emergencyNextCheck) < 0) {
    return;
  }
  emergencyNextCheck = millis() + 20;
  bool lit = ((millis() - emergencyBlinkAnchor) / EMERGENCY_BLINK_INTERVAL) % 2 == 0;
  if (lit != emergencyBlinkOn) {
    emergencyBlinkOn = lit;
    drawEmergencyFrame(lit);
  }
}

void showSetupView() {
  matrix.fillScreen(0);
  matrix.setFont(&muMatrix8ptRegular);
  matrix.setTextColor(0x5555FF);
  printCentered("Setup", 36);
  matrix.show();
}

// ---------------------------------------------------------------------------
// the fireworks of the final view
// ---------------------------------------------------------------------------

#define FIREWORK_COUNT 3
#define FIREWORK_SPARKS 14
#define FIREWORK_FRAME_MS 40
// the row the rockets start from - the bottom row of what all views draw on
#define FIREWORK_GROUND 63
// every rocket climbs at least this many pixels before it bursts
#define FIREWORK_MIN_CLIMB 10
// the highest row a rocket bursts on, just below the line of text
#define FIREWORK_TOP 22

struct Firework {
  uint8_t state;  // 0 = waiting, 1 = rocket rising, 2 = burst
  uint8_t r, g, b;
  float x, y, vy;
  float burstY;
  uint8_t life;  // frames the burst still lasts
  unsigned long launchAt;
  float sx[FIREWORK_SPARKS], sy[FIREWORK_SPARKS];
  float svx[FIREWORK_SPARKS], svy[FIREWORK_SPARKS];
};

Firework fireworks[FIREWORK_COUNT];
unsigned long fireworkNextFrame = 0;

void launchFirework(uint8_t n) {
  Firework& f = fireworks[n];
  static const uint8_t palette[][3] = {
    { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 }, { 255, 255, 0 }, { 255, 0, 255 }, { 0, 255, 255 }, { 255, 255, 255 }
  };
  uint8_t c = random(7);
  f.r = palette[c][0];
  f.g = palette[c][1];
  f.b = palette[c][2];
  f.x = random(8, matrix.width() - 8);
  f.y = FIREWORK_GROUND;
  f.vy = -1.6;
  // burst anywhere between the line of text and the minimum climb
  f.burstY = random(FIREWORK_TOP, FIREWORK_GROUND - FIREWORK_MIN_CLIMB + 1);
  f.state = 1;
}

void burstFirework(uint8_t n) {
  Firework& f = fireworks[n];
  for (uint8_t i = 0; i < FIREWORK_SPARKS; i++) {
    float angle = TWO_PI * i / FIREWORK_SPARKS;
    float speed = 0.5 + random(0, 80) / 100.0;
    f.sx[i] = f.x;
    f.sy[i] = f.y;
    f.svx[i] = cos(angle) * speed;
    f.svy[i] = sin(angle) * speed;
  }
  f.life = 30;
  f.state = 2;
}

void stepFireworks() {
  for (uint8_t n = 0; n < FIREWORK_COUNT; n++) {
    Firework& f = fireworks[n];
    if (f.state == 0) {
      if ((long)(millis() - f.launchAt) >= 0) {
        launchFirework(n);
      }
    } else if (f.state == 1) {
      f.y += f.vy;
      matrix.drawPixel((int16_t)f.x, (int16_t)f.y, matrix.color565(255, 255, 255));
      matrix.drawPixel((int16_t)f.x, (int16_t)f.y + 2, matrix.color565(255, 170, 0));
      if (f.y <= f.burstY) {
        burstFirework(n);
      }
    } else {
      // the panel only knows four levels per colour, so fade in three steps
      uint8_t level = f.life > 18 ? 255 : (f.life > 8 ? 170 : 85);
      uint16_t color = matrix.color565(f.r * level / 255, f.g * level / 255, f.b * level / 255);
      for (uint8_t i = 0; i < FIREWORK_SPARKS; i++) {
        f.sx[i] += f.svx[i];
        f.sy[i] += f.svy[i];
        f.svx[i] *= 0.96;
        f.svy[i] = f.svy[i] * 0.96 + 0.04;  // slowed by the air, pulled down
        matrix.drawPixel((int16_t)f.sx[i], (int16_t)f.sy[i], color);
      }
      f.life--;
      if (f.life == 0) {
        f.state = 0;
        f.launchAt = millis() + random(200, 1500);
      }
    }
  }
}

/**
 * Draws one frame. Called on every pass of the loop while the final view is
 * up, so it must not block: it only draws when the next frame is due.
 */
void showFinalView() {
  if ((long)(millis() - fireworkNextFrame) < 0) {
    return;
  }
  fireworkNextFrame = millis() + FIREWORK_FRAME_MS;
  matrix.fillScreen(0);
  stepFireworks();
  // the text last, so no spark covers it
  matrix.setFont(&muMatrix8ptRegular);
  matrix.setTextColor(0x5555FF);
  printCentered("Das war es ...", 16);
  matrix.show();
}

/**
 * Takes over the end of the lunch break. Anything that is not "HH:MM" is
 * ignored and the time shown so far stays - the big font only has digits and
 * the colon.
 */
void setLunchTime(const char* value, uint32_t end) {
  if (value == NULL || strlen(value) != 5 || value[2] != ':') {
    return;
  }
  for (uint8_t i = 0; i < 5; i++) {
    if (i != 2 && (value[i] < '0' || value[i] > '9')) {
      return;
    }
  }
  strcpy(lunchTime, value);
  lunchEnd = end;
  // A time that is already over when it arrives - the view is opened again
  // after the break, a display joins late - blinks but stays silent. The
  // signal is only for the end that is reached while the view is up.
  lunchOver = lunchEnd != 0 && timeSynced && (uint32_t)(clockNowMs() / 1000) >= lunchEnd;
  lunchArmed = lunchEnd != 0 && !lunchOver;
  lunchBlinkOn = true;
  lunchBlinkAnchor = millis();
  lunchBeatNext = millis();
}

/** the end of the break is reached: signal, and the time starts to blink */
void startLunchOver(unsigned long anchor) {
  lunchOver = true;
  if (lunchArmed) {
    myDFPlayer.play(3);
  }
  lunchArmed = false;
  lunchBlinkAnchor = anchor;
  lunchBlinkOn = false;  // forces the first red frame
  lunchBeatNext = millis();
}

/** the beat of the master display - the others fall in step with it */
void handleLunchBeat() {
  uint32_t end = doc["e"] | (uint32_t)0;
  if (isMaster || view != LunchView || lunchEnd == 0 || end != lunchEnd) {
    return;
  }
  // "o" is how far the master was into its blink second when it sent this
  unsigned long anchor = millis() - (doc["o"] | (uint16_t)0);
  if (!lunchOver) {
    startLunchOver(anchor);
  } else {
    lunchBlinkAnchor = anchor;
  }
}

void printCentered(const char* text, int16_t y) {
  int16_t x1, y1;
  uint16_t w, h;
  matrix.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  matrix.setCursor((matrix.width() - (int16_t)w) / 2, y);
  matrix.print(text);
}

/** lunch break and the time shooting goes on */
void showLunchView() {
  matrix.fillScreen(0);
  matrix.setFont(&muMatrix8ptRegular);
  matrix.setTextColor(0x5555FF);
  printCentered("Mittagspause", 14);
  matrix.setTextColor(White);
  printCentered("bis", 32);
  drawLunchTime(lunchOver ? Red : White, true);
  matrix.show();
}

/**
 * Called on every pass of the loop while the lunch view is up. Sounds the
 * signal when the end of the break is reached and lets the time blink in red
 * from then on.
 */
void lunchLoop() {
  if ((long)(millis() - lunchNextCheck) < 0) {
    return;
  }
  lunchNextCheck = millis() + 20;
  if (lunchEnd == 0) {
    return;
  }
  if (!lunchOver && timeSynced) {
    // The master goes by its clock, the others wait for its beat so signal
    // and blinking start together - and only fall back on their own clock
    // when no beat arrives.
    uint32_t now = (uint32_t)(clockNowMs() / 1000);
    if (now >= lunchEnd && (isMaster || now >= lunchEnd + LUNCH_MASTER_GRACE)) {
      startLunchOver(millis());
    }
  }
  if (!lunchOver) {
    return;
  }
  unsigned long elapsed = millis() - lunchBlinkAnchor;
  bool on = (elapsed / 500) % 2 == 0;
  if (on != lunchBlinkOn) {
    lunchBlinkOn = on;
    drawLunchTime(Red, on);
    matrix.show();
  }
  if (isMaster && (long)(millis() - lunchBeatNext) >= 0) {
    lunchBeatNext = millis() + 1000;
    beginStateMessage(BLINK);
    out["e"] = lunchEnd;
    out["o"] = (uint16_t)((millis() - lunchBlinkAnchor) % 1000);
    sendPacket(out);
  }
}

void drawLunchTime(uint16_t color, bool visible) {
  if (visible) {
    drawClockTime(lunchTime, 42, color);
  } else {
    matrix.fillRect((matrix.width() - 54) / 2, 42, 54, 17, 0);
  }
}

/** "HH:MM" in the big digits, centred, with its top edge on row y */
void drawClockTime(const char* hhmm, int16_t y, uint16_t color) {
  // The digit font announces a colon but only holds the ten digits, so
  // printing "HH:MM" as a string drew whatever follows the font in memory.
  // The digits are set one by one and the colon is drawn by hand:
  // 24 px hours + 6 px colon + 24 px minutes = 54 px, centred on 128.
  const int16_t x = (matrix.width() - 54) / 2;
  matrix.fillRect(x, y, 54, 17, 0);
  matrix.setFont(&Digi11x17_2);
  matrix.setTextColor(color);
  matrix.setCursor(x, y);
  matrix.print(hhmm[0]);
  matrix.print(hhmm[1]);
  matrix.fillRect(x + 25, y + 4, 3, 3, color);
  matrix.fillRect(x + 25, y + 11, 3, 3, color);
  matrix.setCursor(x + 30, y);
  matrix.print(hhmm[3]);
  matrix.print(hhmm[4]);
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
  statusSecondShown = -1;
  drawStatusClock();
  matrix.show();
}

/**
 * The time of day with seconds, centred below the icons. Returns true when
 * it drew something new.
 *
 * The digit font has no colon, so the digits are set in pairs and the colons
 * are drawn by hand: 3 x 24 px digits + 2 x 6 px colon = 84 px.
 */
bool drawStatusClock() {
  if (!timeSynced) {
    return false;  // nothing to show, the crossed out clock icon says why
  }
  int32_t second = (int32_t)((clockNowMs() / 1000 + tzOffset) % 86400);
  if (second == statusSecondShown) {
    return false;
  }
  statusSecondShown = second;
  const uint8_t parts[3] = { (uint8_t)(second / 3600), (uint8_t)((second / 60) % 60), (uint8_t)(second % 60) };
  const int16_t x = (matrix.width() - 84) / 2;
  const int16_t y = 36;
  matrix.fillRect(x, y, 84, 17, 0);
  matrix.setFont(&Digi11x17_2);
  matrix.setTextColor(White);
  for (uint8_t i = 0; i < 3; i++) {
    const int16_t px = x + i * 30;
    matrix.setCursor(px, y);
    matrix.print((char)('0' + parts[i] / 10));
    matrix.print((char)('0' + parts[i] % 10));
    if (i < 2) {
      matrix.fillRect(px + 25, y + 4, 3, 3, White);
      matrix.fillRect(px + 25, y + 11, 3, 3, White);
    }
  }
  return true;
}

/** called on every pass of the loop while the status view is up */
void statusLoop() {
  if ((long)(millis() - statusNextCheck) < 0) {
    return;
  }
  statusNextCheck = millis() + 20;
  if (drawStatusClock()) {
    matrix.show();
  }
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
  finalViewPending = false;
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

/**
 * The group that opens a pass. The groups take turns: AB CD in the first pass,
 * CD AB in the second and so on.
 *
 * The order follows from the number of the pass alone. It used to be kept in a
 * flag of its own that a reset, the end of the tournament and a resync left as
 * it was - a reset after the first pass then gave AB, AB CD, CD AB.
 */
uint8_t firstGroupOfPass() {
  if (groups <= 1) {
    return 1;
  }
  return currentPasses % 2 == 1 ? 1 : groups;
}

/** the shooting time of the pass at hand: a full pass or the arrows of a re-shoot */
uint16_t passShootingTime() {
  return reshootActive ? reshootTime : shootingTime;
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
  // at least one arrow and never more than a pass has - a re-shoot of no
  // arrows would be over the moment the preparation time ends
  reshootArrowCount = prefs.getUShort("rac");
  if (reshootArrowCount > arrowCount) {
    reshootArrowCount = arrowCount;
  }
  if (reshootArrowCount == 0) {
    reshootArrowCount = 1;
  }
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

// tells the controllers - there may be an app and a touch controller - that
// the safety stop was set or released
void sendEmergencyState() {
  if (!isMaster) {
    return;
  }
  beginStateMessage(EMERGENCY);
  out["e"] = emergency;
  sendPacket(out);
}

void sendResetState() {
  if (!isMaster) {
    return;
  }
  beginStateMessage(RESET);
  sendPacket(out);
}

void sendStatus(uint8_t groupCurrent, uint8_t passCurrent, uint8_t groupCount, uint16_t prepareCurrent, uint16_t actionCurrent) {
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
  out["sat"] = passShootingTime();
  out["swt"] = warnTime;
  out["ph"] = (uint8_t)(phase == RunPhase ? 1 : 0);
  out["pr"] = prepare;
  out["h"] = hold;
  out["em"] = emergency;
  out["v"] = currentViewName();
  sendPacket(out);
}

// acknowledgements go straight back to the controller, they are of no
// interest to the other displays
void sendAck(uint64_t seq) {
  if (!wifiActive) {
    return;
  }
  out.clear();
  out["src"] = "display";
  out["cmd"] = ACK;
  out["id"] = displayId;
  out["dn"] = displayNo;
  out["seq"] = seq;
  char text[LINK_MAX_PAYLOAD + 1];
  size_t length = serializeJson(out, text, sizeof(text));
  linkSend(controllerMac, text, length);
}

// everything else goes out to all: the controllers follow the state, the
// other displays the presence
void sendPacket(JsonDocument& d) {
  if (!wifiActive) {
    return;
  }
  if (measureJson(d) > LINK_MAX_PAYLOAD) {
    Serial.println("message too long for one frame, not sent");
    return;
  }
  char text[LINK_MAX_PAYLOAD + 1];
  size_t length = serializeJson(d, text, sizeof(text));
  linkBroadcast(text, length);
}

/** one packet off the radio: the presence of a display or a command */
void handleRadioPacket() {
  DeserializationError error = deserializeJson(doc, packet.data);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  const char* source = doc["src"];
  if (source == NULL) {
    return;
  }
  if (strcmp(source, "display") == 0) {
    handlePeerPacket();
  } else if (strcmp(source, "controller") == 0 && isAddressedToMe()) {
    memcpy(controllerMac, packet.mac, sizeof(controllerMac));
    syncDelay = doc["sd"] | 0;
    Serial.printf("%s (%d dBm)\n", doc["cmd"] | "", packet.rssi);
    handleControllerCommand();
  }
}

/**
 * The power-on animation: the flight of an arrow into the target, seen from
 * the arrow. The target starts as a dot in the distance and grows the way
 * things do when one flies at them - slowly first, then all at once. The
 * arrow sways in flight, so the target swings around the middle of the
 * screen and settles as it comes close, until the gold fills everything.
 */
void showIntro() {
  // the five colours of the target from the outside in, and how much of the
  // full radius each one reaches
  const uint16_t ringColors[5] = { White, Black, Blue, Red, matrix.color565(255, 255, 0) };
  const float ringSizes[5] = { 1.0, 0.8, 0.6, 0.4, 0.2 };
  const uint16_t grass = matrix.color565(0, 85, 0);
  // The sky: light blue at the horizon, a little darker at the top. The panel
  // only knows four levels per colour, so there is no shade in between - the
  // two blues are mixed pixel by pixel in a fixed pattern, more of the light
  // one the further down.
  const uint16_t sky = matrix.color565(85, 170, 255);
  const uint16_t skyTop = matrix.color565(0, 85, 255);
  const uint8_t dither[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
  const int16_t skyRows = 40;  // from here down the sky is all light blue
  // where the clouds hang at the start: x, y and size
  const int8_t clouds[3][3] = { { 20, 13, 4 }, { 100, 10, 5 }, { 52, 6, 3 } };
  const uint16_t steel = matrix.color565(85, 85, 85);
  const int16_t midX = matrix.width() / 2;
  const int16_t midY = 32;
  const uint8_t frames = 60;

  for (uint8_t frame = 0; frame < frames; frame++) {
    // 1 = just released, 0 = in the target
    float distance = 1.0 - (float)frame / frames;
    // the radius of the whole target: 6 px at the start; in the last frame the
    // gold alone reaches into every corner
    float radius = 6.5 / distance;
    // the swaying of the arrow, dying down on the way
    float cx = midX + sin(frame * 0.22) * 6.0 * distance;
    float cy = midY + cos(frame * 0.17) * 3.0 * distance - 4.0 * distance;

    matrix.fillScreen(sky);
    for (int16_t y = 0; y < skyRows; y++) {
      // 0..16: how much of the light blue this row gets
      uint8_t light = (uint8_t)(y * 17 / skyRows);
      for (int16_t x = 0; x < matrix.width(); x++) {
        if (dither[y & 3][x & 3] >= light) {
          matrix.drawPixel(x, y, skyTop);
        }
      }
    }
    // The clouds sway with everything else and drift apart a little as the
    // arrow comes closer. They are far away, so they hardly grow.
    float spread = 1.0 + (1.0 - distance) * 0.5;
    for (uint8_t i = 0; i < 3; i++) {
      int16_t x = (int16_t)(cx + (clouds[i][0] - midX) * spread);
      int16_t y = (int16_t)(cy + (clouds[i][1] - midY) * spread);
      int16_t size = clouds[i][2];
      matrix.fillCircle(x - size, y, size - 1, White);
      matrix.fillCircle(x, y - 1, size, White);
      matrix.fillCircle(x + size + 1, y, size - 1, White);
      matrix.fillRect(x - size, y, 2 * size + 2, size, White);
    }
    // the field the target stands on, its far edge just below the target
    int16_t horizon = (int16_t)(cy + radius * 1.2);
    if (horizon < matrix.height()) {
      matrix.fillRect(0, max((int16_t)0, horizon), matrix.width(), matrix.height(), grass);
    }
    for (uint8_t i = 0; i < 5; i++) {
      float r = radius * ringSizes[i];
      if (r < 0.5) {
        continue;
      }
      if (r > 100) {
        matrix.fillScreen(ringColors[i]);  // reaches beyond every corner
      } else {
        matrix.fillCircle((int16_t)cx, (int16_t)cy, (int16_t)r, ringColors[i]);
      }
    }
    // the point of the arrow at the lower edge, aiming at the target
    int16_t tipX = midX + (int16_t)((cx - midX) * 0.4);
    matrix.fillTriangle(midX - 2, 63, midX + 2, 63, tipX, 54, steel);
    matrix.show();
    delay(35);
  }

  // the hit
  matrix.fillScreen(White);
  matrix.show();
  delay(80);
  matrix.fillScreen(ringColors[4]);
  matrix.show();
  delay(350);
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

/**
 * The safety stop. Setting it holds the countdown like a pause and puts the
 * warning on the screen; releasing it brings back whatever was shown and lets
 * a pass that was running go on.
 */
void setEmergency(bool on) {
  if (on != emergency) {
    emergency = on;
    if (on) {
      holdBeforeEmergency = hold;
      hold = true;
      // "sd" is how long the command has been under way since the key press
      showEmergencyView(millis() - syncDelay);
      myDFPlayer.play(SOUND_EMERGENCY);
      Serial.println("EMERGENCY");
    } else {
      hold = holdBeforeEmergency;
      redrawCurrentView();
      if (phase == RunPhase) {
        if (view == TournamentView) {
          if (!prepare) {
            drawTrafficLight(timer <= warnTime ? Yellow : Green, true);
          }
          drawCountdown(46, 3, initialize ? initTimer : timer);
          drawPasses(75, 2, currentPasses);
          matrix.show();
        } else if (view == ShootInView && !initialize) {
          drawCountdown(38, 2, timer);
          drawProgressBar(timer2);
          matrix.show();
        }
        if (!hold) {
          // goes on like after a pause: all displays wait for the same
          // instant and anchor the one second grid anew
          synced = false;
          runAt = millis() + (syncDelay < SYNC_LEAD ? SYNC_LEAD - syncDelay : 0);
          sendRunState();
        }
      }
      Serial.println("EMERGENCY released");
    }
  }
  sendEmergencyState();
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
  if (strcmp(command, EMERGENCY) == 0) {
    // without a value the command toggles, as a controller of old sent it
    setEmergency(doc["val"]["e"] | !emergency);
    return;
  }
  // Only releasing the safety stop lets the tournament go on - nothing else
  // may start, stop or change what the displays show in the meantime.
  if (emergency && (strcmp(command, CHANGE_VIEW) == 0 || strcmp(command, RUN) == 0 || strcmp(command, PAUSE) == 0
                    || strcmp(command, STOP) == 0 || strcmp(command, RESET) == 0)) {
    return;
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
      tzOffset = doc["val"]["tz"] | tzOffset;
      showStatusView();
    } else if (strcmp(viewName, LUNCH) == 0) {
      view = LunchView;
      setLunchTime(doc["val"]["t"], doc["val"]["e"] | (uint32_t)0);
      showLunchView();
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
        // a re-shoot stays with the group that is on the screen
        if (!reshootActive) {
          currentGroup = firstGroupOfPass();
          drawGroup(currentGroup);
        }
      }
      initialize = true;
      prepare = true;
      stopRequested = false;
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
    stopRequested = true;
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
  if (view == LunchView) {
    return LUNCH;
  }
  return HOME;
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
  } else if (view == LunchView) {
    showLunchView();
  } else {
    showMainView();
  }
}

// ---------------------------------------------------------------------------
// presence - hello at the start and when asked, heartbeat all the time
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
  d["em"] = emergency;
  d["cg"] = currentGroup;
  d["cp"] = currentPasses;
  d["gs"] = groupSet;
  d["cpt"] = (uint16_t)(phase == RunPhase && prepare ? timer : 0);
  d["cat"] = (uint16_t)(phase == RunPhase && !prepare ? timer : 0);
  d["spt"] = prepareTime;
  d["sat"] = passShootingTime();
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
  if (strcmp(command, BLINK) == 0) {
    handleLunchBeat();
    return;
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
  } else if (strcmp(wantedView, LUNCH) == 0) {
    view = LunchView;
  } else {
    view = MainView;
  }
  tzOffset = val["tz"] | tzOffset;
  // taken over whatever the view, so it is there when the break is put up
  setLunchTime(val["lt"], val["le"] | (uint32_t)0);

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
      initTimer = prepare ? prepareTime : passShootingTime();
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
  // a display that joins during a safety stop shows the warning as well
  emergency = val["em"] | false;
  if (emergency) {
    holdBeforeEmergency = false;
    hold = true;
    showEmergencyView(millis());
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
