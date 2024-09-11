#include <Adafruit_Protomatter.h>
#include "AbcdFont.h"
#include "Digi10x16Font.h"
#include "Digi25x43Font.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <DS3231.h>
#include <DFRobot_DF1201S.h>

uint8_t rgbPins[] = { 42, 41, 40, 38, 39, 37 };
uint8_t addrPins[] = { 45, 36, 48, 35, 21 };
uint8_t clockPin = 2;
uint8_t latchPin = 47;
uint8_t oePin = 14;

enum View { Main = 100,
            ShootIn,
            Tournament,
            Setup };
enum Phase { Run = 200,
             Stop,
             Idle };

View view = Main;
Phase phase = Idle;

bool pauseAction = false;
bool initAction = true;
uint16_t currentTime;
uint16_t targetTime;

const char* HOME = "home";
const char* SETUP = "setup";
const char* TOURNAMENT = "tournament";
const char* TOURNAMENT_MODE = "change_topic";
const char* TOGGLE_PHASE = "toggle_action";
const char* RESET = "reset";
const char* STOP = "stop";
const char* EMERGENCY = "emergency";
const char* VOLUME = "volume";
const char* TEST_SIGNAL = "testsignal";
String command;
String jsonString;




unsigned int localPort = 2390;  // local port to listen on

char packetBuffer[512];               //buffer to hold incoming packet
char ReplyBuffer[] = "acknowledged";  // a string to send back

char ssid[] = "eclipse";
char pass[] = "oiu&54Kjh=)(/kjh34598IUGh9873jkhRED67r76f76f";

Adafruit_Protomatter matrix(
  128,                        // Width of matrix (or matrix chain) in pixels
  3,                          // Bit depth, 1-6
  2, rgbPins,                 // # of matrix chains, array of 6 RGB pins for each
  5, addrPins,                // # of address pins (height is inferred), array of pins
  clockPin, latchPin, oePin,  // Other matrix control pins
  false);                      // No double-buffering here (see "doublebuffer" example)

WiFiClient client;
#if defined(ARDUINO_AVR_UNO) || defined(ESP8266)
#include "SoftwareSerial.h"
SoftwareSerial DF1201SSerial(2, 3);  //RX  TX
#else
#define DF1201SSerial Serial1
#endif

DFRobot_DF1201S DF1201S;


void setup(void) {


  ProtomatterStatus status = matrix.begin();
  if (status != PROTOMATTER_OK) {
    for (;;)
      ;
  }
  

  Serial.begin(115200);
  /*while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
*/

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //Udp.begin(localPort);
  Serial.printf("Connected to WiFi\n");
  printWifiStatus();

  Wire.begin();
  RTClib rtcTimer;
  DateTime now = rtcTimer.now();
  Serial.printf("Current year %d\n", now.year());

  /*
  DF1201SSerial.begin(115200, SERIAL_8N1, RX, TX);
  while (!DF1201S.begin(DF1201SSerial)) {
    Serial.println("Init failed, please check the wire connection!");
    delay(1000);
  }

*/


  //sprintf(str, "Archery Timer / C++ beta", matrix.width(), matrix.height());
  matrix.setFont(&Digi25x43);
  //matrix.setTextWrap(false);
  //matrix.setTextColor(0xFFFF);
  //matrix.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  //matrix.setCursor(20, 10 + h);
  //matrix.print(str);
  //matrix.drawRoundRect(0, 0, 128, 64, 2, matrix.color565(0, 255, 0));
  //matrix.drawRoundRect(1, 1, 126, 62, 2, matrix.color565(0, 255, 0));
  matrix.setTextColor(0xFFFFFF);
  matrix.setCursor(0,0);
  matrix.println("489");
  //drawTrafficLight(matrix.color565(255, 0, 0));
  matrix.show();
}

void drawTrafficLight(u_int16_t color) {
  matrix.fillRect(0, 0, 40, 64, color);
}

void loop(void) {
  /*
  if (client.available()) {
    char json[] = char[300];
    client.readBytes();
  }
*/
  if (command.equalsIgnoreCase(HOME)) {
  } else if (command.equalsIgnoreCase(TOURNAMENT)) {
  } else if (command.equalsIgnoreCase(TOURNAMENT_MODE)) {
  } else if (command.equalsIgnoreCase(SETUP)) {
  } else if (command.equalsIgnoreCase(TOGGLE_PHASE)) {
  } else if (command.equalsIgnoreCase(STOP)) {
  } else if (command.equalsIgnoreCase(RESET)) {
  } else if (command.equalsIgnoreCase(EMERGENCY)) {
  } else if (command.equalsIgnoreCase(VOLUME)) {
  } else if (command.equalsIgnoreCase(TEST_SIGNAL)) {
  }



  if (view == Main) {
  } else if (view == ShootIn) {
    if (phase == Run) {
      if (!pauseAction) {
        if (initAction) {
          targetTime = millis() + 1000;
          initAction = false;
        } else {
          if (millis() >= targetTime) {
          }
        }
      } else {
      }
    } else if (phase == Stop) {
    } else {
    }

  } else if (view == Tournament) {
    if (phase == Run) {
      if (!pauseAction) {
        if (initAction) {
          targetTime = millis() + 1000;
          initAction = false;
        } else {
          if (millis() >= targetTime) {
          }
        }
      } else {
      }
    } else if (phase == Stop) {
    } else {
    }
  } else if (view == Setup) {
  }

  // if the server's disconnected, stop the client:
  if (!client.connected()) {
    //Serial.println();
    //Serial.println("disconnecting from server.");
    client.stop();

    // do nothing forevermore:
    /*
    while (true) {
      delay(100);
    }*/
  }
}

void sendResponse() {
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
