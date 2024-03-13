#include <Adafruit_Protomatter.h>
#include <Fonts/FreeSansBold18pt7b.h>


uint8_t rgbPins[] = { 42, 41, 40, 38, 39, 37 };
uint8_t addrPins[] = { 45, 36, 48, 35, 21 };
uint8_t clockPin = 2;
uint8_t latchPin = 47;
uint8_t oePin = 14;

int16_t textX;
int16_t textY;
int16_t textMin;
char str[64];

Adafruit_Protomatter matrix(
  128,                        // Width of matrix (or matrix chain) in pixels
  4,                          // Bit depth, 1-6
  2, rgbPins,                 // # of matrix chains, array of 6 RGB pins for each
  5, addrPins,                // # of address pins (height is inferred), array of pins
  clockPin, latchPin, oePin,  // Other matrix control pins
  false);                     // No double-buffering here (see "doublebuffer" example)



void setup(void) {
  int16_t  x1, y1;
  uint16_t w, h;
  ProtomatterStatus status = matrix.begin();
  if (status != PROTOMATTER_OK) {
    for (;;)
      ;
  }
  sprintf(str, "Archery Timer / C++ beta", matrix.width(), matrix.height());
  matrix.setFont(&FreeSansBold18pt7b);
  matrix.setTextWrap(false);
  matrix.setTextColor(0xFFFF);
  matrix.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  matrix.setCursor(20, 10+h);
  matrix.print(str);
  matrix.drawRoundRect(0, 0, 128, 64, 2,matrix.color565(0, 255, 0));
  matrix.drawRoundRect(1, 1, 126, 62, 2,matrix.color565(0, 255, 0));
  matrix.show();
}


void loop(void) {
  delay(1000);
}
