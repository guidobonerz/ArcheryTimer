#include "HX711.h"


#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Bounce2.h>


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64


#define OLED_MOSI 23
#define OLED_CLK 18
#define OLED_DC 19
#define OLED_CS 5
#define OLED_RESET 35

#define BUTTON_TARE 21
#define BUTTON_UNIT 22


const int LOADCELL_DOUT_PIN = 16;
const int LOADCELL_SCK_PIN = 4;

bool tareButton = false;
bool unitButton = false;

HX711 scale;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
Bounce bounce = Bounce();

float calibration_factor = 44300;  //48400;
unsigned long targetTime;
void setup() {

  bounce.attach(BUTTON_TARE, INPUT_PULLUP);
  bounce.attach(BUTTON_UNIT, INPUT_PULLUP);

  bounce.interval(5);  // interval in ms


  //pinMode(BUTTON_TARE, INPUT_PULLUP);
  //pinMode(BUTTON_UNIT, INPUT_PULLUP);

  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);


  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  tare();

  long zero_factor = scale.read_average();  //Get a baseline reading
  Serial.print("Zero factor: ");
  Serial.println(zero_factor);
  targetTime = millis() + 500;
}

void loop() {
  scale.set_scale(calibration_factor);

  if (millis() > targetTime) {
    targetTime = millis() + 500;
    float valueKG = scale.get_units();
    if (valueKG < 0) {
      
    } else if (valueKG >= 1) {
      float valueLBS = valueKG * 2.205;
      display.clearDisplay();

      if (digitalRead(BUTTON_UNIT) == HIGH) {
        display.setCursor(0, 10);
        display.printf("Kilograms");
        display.setCursor(30, 35);
        display.printf("%08.2f", valueKG);
      } else {
        display.setCursor(0, 10);
        display.printf("Pounds");
        display.setCursor(30, 35);
        display.printf("%08.2f", valueLBS);
      }
    } else {
      float valueGramm = valueKG * 1000;
      float valueGrain = valueGramm * 15.4324;
      display.clearDisplay();

      if (digitalRead(BUTTON_UNIT) == HIGH) {
        display.setCursor(0, 10);
        display.printf("Grams");
        display.setCursor(30, 35);
        display.printf("%08.2f", valueGramm);
      } else {
        display.setCursor(0, 10);
        display.printf("Grains");
        display.setCursor(30, 35);
        display.printf("%08.2f", valueGrain);
      }
    }
  }
  display.display();

  if (digitalRead(BUTTON_TARE) == LOW) {
    tare();
  }
}

void tare() {
  scale.set_scale();
  scale.tare();
}