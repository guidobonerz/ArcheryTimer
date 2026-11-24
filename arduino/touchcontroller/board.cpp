#include "board.h"

#include <Wire.h>
#include <lvgl.h>

#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"

#include "config.h"

namespace {

esp_lcd_panel_handle_t panel = NULL;

// what the outputs of the io expander are set to - it cannot be read back
uint8_t exio = 0;

uint8_t touchAddress = 0;
bool touchPressed = false;
int16_t touchX = 0;
int16_t touchY = 0;
unsigned long touchLastReport = 0;
unsigned long touchNextPoll = 0;

const uint16_t GT911_REG_STATUS = 0x814E;
const uint16_t GT911_REG_POINT1 = 0x8150;
const uint16_t GT911_REG_PRODUCT_ID = 0x8140;
const uint8_t TOUCH_POLL_INTERVAL = 10;
// The chip reports a held finger over and over. When the reports stop without
// a release - a glitch on the bus - the finger must not stay down forever.
const uint16_t TOUCH_RELEASE_TIMEOUT = 300;

void exioWrite(uint8_t value) {
  exio = value;
  Wire.beginTransmission(CH422G_ADDR_OUT);
  Wire.write(exio);
  Wire.endTransmission();
}

void exioSet(uint8_t mask, bool high) {
  exioWrite(high ? (exio | mask) : (exio & ~mask));
}

bool touchRead(uint16_t reg, uint8_t* buffer, uint8_t length) {
  Wire.beginTransmission(touchAddress);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(touchAddress, length) != length) {
    return false;
  }
  for (uint8_t i = 0; i < length; i++) {
    buffer[i] = Wire.read();
  }
  return true;
}

void touchWrite(uint16_t reg, uint8_t value) {
  Wire.beginTransmission(touchAddress);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.write(value);
  Wire.endTransmission();
}

bool touchProbe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

/**
 * The GT911 takes its i2c address from the level of the interrupt line at the
 * moment the reset is released: low gives 0x5D. The reset line hangs on the
 * io expander.
 */
bool touchBegin() {
  pinMode(TOUCH_PIN_INT, OUTPUT);
  digitalWrite(TOUCH_PIN_INT, LOW);
  exioSet(EXIO_TOUCH_RESET, false);
  delay(20);
  exioSet(EXIO_TOUCH_RESET, true);
  delay(10);
  pinMode(TOUCH_PIN_INT, INPUT);
  delay(100);

  if (touchProbe(0x5D)) {
    touchAddress = 0x5D;
  } else if (touchProbe(0x14)) {
    touchAddress = 0x14;
  } else {
    Serial.println("touch: no GT911 found");
    return false;
  }

  uint8_t id[4] = { 0, 0, 0, 0 };
  touchRead(GT911_REG_PRODUCT_ID, id, sizeof(id));
  Serial.printf("touch: GT%c%c%c%c at 0x%02X\n", id[0], id[1], id[2], id[3], touchAddress);
  return true;
}

void touchPoll() {
  unsigned long now = millis();
  if ((long)(now - touchNextPoll) < 0) {
    return;
  }
  touchNextPoll = now + TOUCH_POLL_INTERVAL;

  uint8_t status = 0;
  if (!touchRead(GT911_REG_STATUS, &status, 1)) {
    return;
  }
  if (status & 0x80) {
    uint8_t points = status & 0x0F;
    uint8_t data[4];
    if (points > 0 && points <= 5 && touchRead(GT911_REG_POINT1, data, sizeof(data))) {
      touchX = constrain((int16_t)(data[0] | (data[1] << 8)), 0, LCD_H_RES - 1);
      touchY = constrain((int16_t)(data[2] | (data[3] << 8)), 0, LCD_V_RES - 1);
      touchPressed = true;
    } else {
      touchPressed = false;
    }
    touchLastReport = now;
    // the chip holds the report until it is told that it was taken
    touchWrite(GT911_REG_STATUS, 0);
  } else if (touchPressed && now - touchLastReport > TOUCH_RELEASE_TIMEOUT) {
    touchPressed = false;
  }
}

void lvglRead(lv_indev_t* indev, lv_indev_data_t* data) {
  if (touchAddress != 0) {
    touchPoll();
  }
  data->point.x = touchX;
  data->point.y = touchY;
  data->state = touchPressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// what a pass of LVGL spent on copying to the panel, for the log in boardLoop()
uint32_t flushMicros = 0;
uint16_t flushCount = 0;

// given by the panel driver at the end of every frame it has sent
SemaphoreHandle_t frameDone = NULL;

bool IRAM_ATTR onVsync(esp_lcd_panel_handle_t handle, const esp_lcd_rgb_panel_event_data_t* event, void* context) {
  BaseType_t woken = pdFALSE;
  xSemaphoreGiveFromISR(frameDone, &woken);
  return woken == pdTRUE;
}

/**
 * The panel is refreshed from one of two frame buffers while LVGL draws into
 * the other one. Only when a picture is complete the two change places, at the
 * end of a frame - so a page appears at once instead of strip by strip, the
 * way it did when LVGL copied its strips into the one buffer that was on show.
 */
void lvglFlush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
  if (lv_display_flush_is_last(display)) {
    uint32_t start = micros();
    xSemaphoreTake(frameDone, 0);
    // handing the driver one of its own buffers makes it the one on show
    esp_lcd_panel_draw_bitmap(panel, 0, 0, LCD_H_RES, LCD_V_RES, lv_display_get_buf_active(display)->data);
    // the other buffer is still being sent until this frame is over - LVGL
    // must not draw into it before
    xSemaphoreTake(frameDone, pdMS_TO_TICKS(100));
    flushMicros += micros() - start;
    flushCount++;
  }
  lv_display_flush_ready(display);
}

uint32_t lvglTick() {
  return millis();
}

bool panelBegin() {
  const int dataPins[16] = LCD_PINS_DATA;

  esp_lcd_rgb_panel_config_t config = {};
  config.clk_src = LCD_CLK_SRC_DEFAULT;
  config.timings.pclk_hz = LCD_PCLK_HZ;
  config.timings.h_res = LCD_H_RES;
  config.timings.v_res = LCD_V_RES;
  config.timings.hsync_pulse_width = LCD_HSYNC_PULSE;
  config.timings.hsync_back_porch = LCD_HSYNC_BACK_PORCH;
  config.timings.hsync_front_porch = LCD_HSYNC_FRONT_PORCH;
  config.timings.vsync_pulse_width = LCD_VSYNC_PULSE;
  config.timings.vsync_back_porch = LCD_VSYNC_BACK_PORCH;
  config.timings.vsync_front_porch = LCD_VSYNC_FRONT_PORCH;
  config.timings.flags.pclk_active_neg = LCD_PCLK_ACTIVE_NEG;
  config.data_width = 16;
  // esp-idf 6 (esp32 core 4.x) names the pixel format instead of its width
#if ESP_IDF_VERSION_MAJOR >= 6
  config.in_color_format = LCD_COLOR_FMT_RGB565;
  config.out_color_format = LCD_COLOR_FMT_RGB565;
#else
  config.bits_per_pixel = 16;
#endif
  config.num_fbs = 2;
  config.bounce_buffer_size_px = LCD_H_RES * LCD_BOUNCE_BUFFER_LINES;
  config.hsync_gpio_num = (gpio_num_t)LCD_PIN_HSYNC;
  config.vsync_gpio_num = (gpio_num_t)LCD_PIN_VSYNC;
  config.de_gpio_num = (gpio_num_t)LCD_PIN_DE;
  config.pclk_gpio_num = (gpio_num_t)LCD_PIN_PCLK;
  config.disp_gpio_num = GPIO_NUM_NC;
  for (uint8_t i = 0; i < 16; i++) {
    config.data_gpio_nums[i] = (gpio_num_t)dataPins[i];
  }
  config.flags.fb_in_psram = 1;

  if (esp_lcd_new_rgb_panel(&config, &panel) != ESP_OK) {
    Serial.println("lcd: creating the rgb panel failed - is the psram enabled?");
    return false;
  }
  frameDone = xSemaphoreCreateBinary();
  esp_lcd_rgb_panel_event_callbacks_t callbacks = {};
  callbacks.on_vsync = onVsync;
  esp_lcd_rgb_panel_register_event_callbacks(panel, &callbacks, NULL);
  esp_lcd_panel_reset(panel);
  if (esp_lcd_panel_init(panel) != ESP_OK) {
    Serial.println("lcd: init failed");
    return false;
  }
  return true;
}

const uint16_t BATTERY_INTERVAL = 1000;
unsigned long batteryNextTime = 0;
// smoothed voltage of the battery, 0 as long as there is none
float batteryMillivolts = 0;

// open circuit voltage of a LiPo cell against its charge
const struct {
  uint16_t millivolts;
  uint8_t percent;
} BATTERY_CURVE[] = {
  { 4200, 100 }, { 4110, 90 }, { 4020, 80 }, { 3950, 70 }, { 3870, 60 }, { 3840, 50 },
  { 3800, 40 }, { 3770, 30 }, { 3730, 20 }, { 3690, 10 }, { 3610, 5 }, { 3300, 0 }
};

void batteryPoll() {
  unsigned long now = millis();
  if ((long)(now - batteryNextTime) < 0) {
    return;
  }
  batteryNextTime = now + BATTERY_INTERVAL;

  uint32_t sum = 0;
  for (uint8_t i = 0; i < 8; i++) {
    sum += analogReadMilliVolts(BATTERY_PIN_ADC);
  }
  float millivolts = sum / 8.0f * BATTERY_DIVIDER;
  if (millivolts < BATTERY_MIN_MILLIVOLTS) {
    batteryMillivolts = 0;
  } else if (batteryMillivolts == 0) {
    batteryMillivolts = millivolts;
  } else {
    // the radio and the backlight pull the voltage down for moments
    batteryMillivolts += (millivolts - batteryMillivolts) * 0.1f;
  }
}

}  // namespace

int8_t boardBatteryPercent() {
  if (batteryMillivolts == 0) {
    return -1;
  }
  const uint8_t count = sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]);
  if (batteryMillivolts >= BATTERY_CURVE[0].millivolts) {
    return 100;
  }
  for (uint8_t i = 1; i < count; i++) {
    if (batteryMillivolts >= BATTERY_CURVE[i].millivolts) {
      float span = BATTERY_CURVE[i - 1].millivolts - BATTERY_CURVE[i].millivolts;
      float part = (batteryMillivolts - BATTERY_CURVE[i].millivolts) / span;
      return BATTERY_CURVE[i].percent + (int8_t)(part * (BATTERY_CURVE[i - 1].percent - BATTERY_CURVE[i].percent) + 0.5f);
    }
  }
  return 0;
}

bool boardBegin() {
  Wire.begin(I2C_PIN_SDA, I2C_PIN_SCL, I2C_FREQUENCY);

  // all pins of the expander are outputs; backlight off until there is a
  // picture, sd card deselected, usb pins left to the usb socket
  Wire.beginTransmission(CH422G_ADDR_MODE);
  Wire.write(CH422G_MODE_OUTPUT);
  Wire.endTransmission();
  exioWrite(EXIO_SD_CS);
  delay(20);
  exioSet(EXIO_LCD_RESET, true);
  delay(100);

  bool ok = panelBegin();
  touchBegin();
  if (!ok) {
    return false;
  }

  lv_init();
  lv_tick_set_cb(lvglTick);

  // LVGL draws straight into the two frame buffers of the panel driver
  void* buffer1 = NULL;
  void* buffer2 = NULL;
  if (esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &buffer1, &buffer2) != ESP_OK) {
    Serial.println("lcd: no frame buffers");
    return false;
  }
  size_t bufferSize = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);

  lv_display_t* display = lv_display_create(LCD_H_RES, LCD_V_RES);
  lv_display_set_flush_cb(display, lvglFlush);
  lv_display_set_buffers(display, buffer1, buffer2, bufferSize, LV_DISPLAY_RENDER_MODE_DIRECT);

  lv_indev_t* indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lvglRead);
  return true;
}

void boardLoop() {
  batteryPoll();
  flushMicros = 0;
  flushCount = 0;
  uint32_t start = micros();
  lv_timer_handler();
  uint32_t total = micros() - start;
  // A whole page takes some 180 ms to draw. Anything well beyond that is felt
  // as a sluggish touch and worth a line in the log.
  if (total > 300000) {
    Serial.printf("lvgl: slow pass, %lu ms, of that %lu ms waiting for the frame in %u flushes\n",
                  (unsigned long)(total / 1000), (unsigned long)(flushMicros / 1000), flushCount);
  }
}

void boardBacklight(bool on) {
  exioSet(EXIO_BACKLIGHT, on);
}
