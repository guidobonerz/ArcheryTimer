#include "espnow_link.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace {

const uint8_t BROADCAST_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Packets arrive in the wifi task. They wait here until the loop of the
// sketch fetches them, so nothing of the sketch runs in that task.
const uint8_t QUEUE_LENGTH = 16;
QueueHandle_t queue = NULL;
bool ready = false;

void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int length) {
  if (queue == NULL || length <= 0 || length > LINK_MAX_PAYLOAD) {
    return;
  }
  LinkPacket packet;
  memcpy(packet.mac, info->src_addr, sizeof(packet.mac));
  packet.rssi = info->rx_ctrl != NULL ? info->rx_ctrl->rssi : 0;
  packet.length = length;
  memcpy(packet.data, data, length);
  packet.data[length] = 0;
  // a full queue drops the packet - everything that matters is repeated
  xQueueSend(queue, &packet, 0);
}

bool addPeer(const uint8_t mac[6]) {
  if (esp_now_is_peer_exist(mac)) {
    return true;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, sizeof(peer.peer_addr));
  peer.channel = 0;  // the channel the radio is on
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

}  // namespace

bool linkBegin() {
  if (ready) {
    return true;
  }
  queue = xQueueCreate(QUEUE_LENGTH, sizeof(LinkPacket));
  if (queue == NULL) {
    return false;
  }

  // the radio runs as a station that never joins a network
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  WiFi.setSleep(false);  // modem sleep makes the board miss packets
  if (esp_wifi_set_channel(LINK_CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    Serial.println("esp-now: setting the channel failed");
    return false;
  }
  if (esp_now_init() != ESP_OK) {
    Serial.println("esp-now: init failed");
    return false;
  }
  if (esp_now_register_recv_cb(onReceive) != ESP_OK || !addPeer(BROADCAST_MAC)) {
    Serial.println("esp-now: setup failed");
    return false;
  }
  ready = true;
  Serial.printf("esp-now up on channel %d, mac %s\n", LINK_CHANNEL, WiFi.macAddress().c_str());
  return true;
}

bool linkReceive(LinkPacket& packet) {
  return queue != NULL && xQueueReceive(queue, &packet, 0) == pdTRUE;
}

bool linkBroadcast(const char* data, size_t length) {
  if (!ready || length == 0 || length > LINK_MAX_PAYLOAD) {
    return false;
  }
  return esp_now_send(BROADCAST_MAC, (const uint8_t*)data, length) == ESP_OK;
}

bool linkSend(const uint8_t mac[6], const char* data, size_t length) {
  if (!ready || length == 0 || length > LINK_MAX_PAYLOAD) {
    return false;
  }
  if (!addPeer(mac)) {
    // The table of the radio is full. Every message says whom it is meant
    // for, so it can go out to all as well.
    return linkBroadcast(data, length);
  }
  return esp_now_send(mac, (const uint8_t*)data, length) == ESP_OK;
}

void linkMacToText(const uint8_t mac[6], char* text) {
  snprintf(text, 13, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
