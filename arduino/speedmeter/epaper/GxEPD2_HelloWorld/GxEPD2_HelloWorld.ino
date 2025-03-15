
//#include <GFX.h>

#include <GxEPD2_BW.h>

#include <Fonts/FreeMonoBold9pt7b.h>
#include "Digi25x43Font.h"
#include "GxEPD2_display_selection_new_style.h"

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=5*/ SS, /*DC=*/17, /*RST=*/16, /*BUSY=*/4));  // GDEH0154D67 200x200, SSD1681


void setup() {
  display.init(115200, true, 2, false);  // USE THIS for Waveshare boards with "clever" reset circuit, 2ms reset pulse
  helloWorld();
  display.hibernate();
}


void helloWorld() {
  display.setRotation(1);

  display.setFullWindow();
  display.firstPage();
  
  do {

    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    display.setFont(&Digi25x43);
    display.setCursor(0, 0);
    display.print("0000");
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(10, 80);
    display.print("Speed");
  } while (display.nextPage());
}

void loop(){};
