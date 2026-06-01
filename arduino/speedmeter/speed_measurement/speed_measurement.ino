

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "Digi25x43Font.h"
#include "Digi11x17_2Font.h"
#include "Units2Font.h"
#include <Fonts/FreeMono12pt7b.h>
#include <Bounce2.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define OLED_RESET -1
Adafruit_SH1107 display = Adafruit_SH1107(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 1000000, 100000);

#define READ_IN_PORT 36
#define READ_OUT_PORT 39

Bounce2::Button resetButton = Bounce2::Button();
Bounce2::Button unitButton = Bounce2::Button();


int measurementCount = 0;
int unitIndex = 0;
unsigned long inTime = 0;
unsigned long outTime = 0;
unsigned long diffTime = 0;
unsigned long timer = 0;
unsigned long speedMs = 0;
unsigned long speedMsList[100];
unsigned long speed = 0;
unsigned long speedAvg = 0;
bool started = false;
bool timerInitialzed = false;
char unitSynbol[1];

float diffSeconds = 0;
enum Phase { IdlePhase = 10,
             AnalysePhase
};
Phase phase = IdlePhase;

void IRAM_ATTR startMeasurement() {
  if (!started) {
    inTime = micros();
    started = true;
  }
}

void IRAM_ATTR stopMeasurement() {
  if (started) {
    outTime = micros();
    phase = AnalysePhase;
  }
}


void setup() {
  Serial.begin(115200);

  unitButton.attach(16, INPUT_PULLUP);
  unitButton.interval(50);
  unitButton.setPressedState(LOW);

  resetButton.attach(17, INPUT_PULLUP);
  resetButton.interval(50);
  resetButton.setPressedState(LOW);

  display.begin(0x3C, true);           // Address 0x3D default
  display.setTextColor(SH110X_WHITE);  // Draw white text
  display.clearDisplay();


  pinMode(READ_IN_PORT, INPUT);
  pinMode(READ_OUT_PORT, INPUT);
  attachInterrupt(READ_IN_PORT, startMeasurement, RISING);
  attachInterrupt(READ_OUT_PORT, stopMeasurement, RISING);
  drawSpeed();
}

void drawSpeed() {
  display.clearDisplay();

  if (unitIndex == 0) {
    speed = speedMs;
    speedAvg = getSpeedMsAvg(measurementCount);
  } else if (unitIndex == 1) {
    speed = speedMs * 3.6;
    speedAvg = getSpeedMsAvg(measurementCount) * 3.6;
  } else if (unitIndex == 2) {
    speed = speedMs * 3.28084;
    speedAvg = getSpeedMsAvg(measurementCount) * 3.28084;
  } else if (unitIndex == 3) {
  } else {
  }

  display.setFont(&Digi25x43);
  display.setCursor(12, 2);
  display.printf("%04d", speed);

  display.drawFastHLine(0, 50, 128, SH110X_WHITE);
  display.setFont(&Digi25x43);
  display.setCursor(12, 55);
  display.printf("%04d", speedAvg);

  display.setFont(&Units2);
  display.setCursor(0, 110);
  itoa(unitIndex, unitSynbol, 10);
  display.printf(unitSynbol);

  display.setFont(&Digi11x17_2);
  display.setCursor(80, 110);
  display.printf("%03d", measurementCount);

  display.display();
}

void loop() {

  if (phase == IdlePhase) {
    unitButton.update();
    resetButton.update();
    if (unitButton.pressed()) {
      unitIndex++;
      if (unitIndex > 3) {
        unitIndex = 0;
      }
      drawSpeed();
    }

    if (resetButton.pressed()) {
      unitIndex = 0;
      measurementCount = 0;
      speedMs = 0;
      speed = 0;
      speedAvg = 0;
      drawSpeed();
    }
  } else if (phase = AnalysePhase) {
    diffTime = outTime - inTime;
    diffSeconds = diffTime / 1E6;
    speedMs = (0.09 / diffSeconds);
    speedMsList[measurementCount] = speedMs;
    measurementCount++;
    drawSpeed();
    phase = IdlePhase;
    started = false;
  }
}

unsigned long getSpeedMsAvg(int shots) {
  unsigned long sum = 0;
  for (int i = 0; i < shots; i++) {
    sum += speedMsList[i];
  }
  return (int)(sum / (shots + 1));
}
