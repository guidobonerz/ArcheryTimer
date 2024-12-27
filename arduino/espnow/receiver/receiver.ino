#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

uint8_t myAddress[] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x00};
#define CHANNEL 10

typedef struct message {
  char text[64];
  bool state;
} command;

command myCommand;

void commandReceived(const esp_now_recv_info* info, const uint8_t* incomingData, int len) {
  memcpy(&myCommand, incomingData, sizeof(myCommand));
  if (myCommand.state) {
    digitalWrite(1, LOW);
  } else {
    digitalWrite(1, HIGH);
  }
}
void setup() {
  pinMode(1, OUTPUT);
  digitalWrite(1, LOW);
  Serial.begin(9600);
  delay(1000);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_mac(WIFI_IF_STA, myAddress);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow Init success");
  } else {
    Serial.println("ESPNow Init fail");
    return;
  }
  esp_now_register_recv_cb(commandReceived);
}
void loop() {}