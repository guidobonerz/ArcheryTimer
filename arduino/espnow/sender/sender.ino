
//https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json
#include <FastLED.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <ArduinoJson.h>
char receiveBuffer[255];
#define NUM_LEDS 1
#define DATA_PIN 8
#define CHANNEL 10

CRGB leds[NUM_LEDS];
uint8_t c = 0;

uint8_t receiverAddress[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
esp_now_peer_info_t peerInfo;

typedef struct {
  uint8_t source               
	uint8_t command
	uint8_t view   
	uint8_t passes 
	uint8_t prepareTime          
	uint8_t arrowCount           
	uint8_t reshootArrowCount    
	uint8_t warnTime             
	uint8_t groups               
	bool flashingPrepareLight 
} settings;

typedef struct {
  uint8_t currentPass;
  uint8_t currentGroup;
  uint8_t currentPrepareTime;
  uint8_t currentActionTime;
  uint8_t currentGroupSet;
} status;


command myCommand;
void commandSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  Serial.print("Send status: ");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Success");
  } else {
    Serial.println("Error");
  }
}



void setup() {
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  Serial.begin(9600);
  leds[0] = 0x222222;
  FastLED.show();
  WiFi.mode(WIFI_STA);

  if (esp_now_init() == ESP_OK) {
    leds[0] = 0x220000;
    FastLED.show();
  } else {
    leds[0] = 0x002200;
    FastLED.show();
    return;
  }

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  esp_now_register_send_cb(commandSent);
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = CHANNEL;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
  clearBuffer();
}

void loop() {

  int bc = Serial.available();
  //Serial.printf("%d bytes received\n", bc);
  if (bc > 0) {
    c = 0;
    char b = 0;
    b = Serial.read();
    while (b != 10 && b != 0) {
      receiveBuffer[c] = b;
      b = Serial.read();
      c++;
    }

    //Serial.println(receiveBuffer);

    if (strcmp(receiveBuffer, "start") == 0) {

      char command[] = "start";
      memcpy(&myCommand.text, command, sizeof(command));
      myCommand.state = true;
      esp_err_t result = esp_now_send(receiverAddress, (uint8_t *)&myCommand, sizeof(myCommand));
      if (result != ESP_OK) {
        Serial.println("Sending error");
      }
      leds[0] = 0x220000;
      FastLED.show();
    } else if (strcmp(receiveBuffer, "stop") == 0) {

      char command[] = "stop";
      memcpy(&myCommand.text, command, sizeof(command));
      myCommand.state = false;
      esp_err_t result = esp_now_send(receiverAddress, (uint8_t *)&myCommand, sizeof(myCommand));
      if (result != ESP_OK) {
        Serial.println("Sending error");
      }
      leds[0] = 0x002200;
      FastLED.show();
    } else if (strcmp(receiveBuffer, "pause") == 0) {
      leds[0] = 0x222200;
      FastLED.show();
    } else if (strcmp(receiveBuffer, "reset") == 0) {
      leds[0] = 0x222222;
      FastLED.show();
    } else {
      leds[0] = 0x000000;
      FastLED.show();
    }
    clearBuffer();
  }
}

void clearBuffer() {
  for (int i = 0; i < 255; i++) {
    receiveBuffer[i] = 0;
  }
}
