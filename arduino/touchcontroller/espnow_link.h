#pragma once

#include <Arduino.h>

/*
 * The radio link between the controllers and the displays: ESP-NOW.
 *
 * No access point is involved. Every board sends straight to the others, to
 * all of them (broadcast) or to one (by its mac). Everybody has to be on the
 * same wifi channel, which is why it is a constant of the firmware.
 *
 * This file and espnow_link.cpp lie in ../archertimer and ../touchcontroller
 * as the very same copy - a change belongs into both.
 */

// The wifi channel all boards meet on, 1..13. Has to be the same in every
// firmware that takes part.
#define LINK_CHANNEL 1

// what one ESP-NOW frame carries at most
#define LINK_MAX_PAYLOAD 250

struct LinkPacket {
  // the board the packet came from
  uint8_t mac[6];
  // how well it was heard, in dBm
  int8_t rssi;
  uint8_t length;
  // the payload, closed with a 0 so it can be read as text
  char data[LINK_MAX_PAYLOAD + 1];
};

/** brings the radio up, false when that failed */
bool linkBegin();

/** the next packet that came in, false when none is waiting */
bool linkReceive(LinkPacket& packet);

/** to every board in reach - nobody confirms it */
bool linkBroadcast(const char* data, size_t length);

/**
 * To one board. The radio repeats it a few times by itself until that board
 * has confirmed it; false only tells that it could not be handed over.
 */
bool linkSend(const uint8_t mac[6], const char* data, size_t length);

/** "a1b2c3d4e5f6" - the buffer has to hold 13 characters */
void linkMacToText(const uint8_t mac[6], char* text);
