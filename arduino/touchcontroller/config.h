#pragma once

// ---------------------------------------------------------------------------
// Waveshare ESP32-S3-Touch-LCD-7 (800x480, ST7262 rgb panel, GT911 touch,
// CH422G io expander)
// ---------------------------------------------------------------------------

#define LCD_H_RES 800
#define LCD_V_RES 480
#define LCD_PCLK_HZ (16 * 1000 * 1000)
#define LCD_HSYNC_PULSE 4
#define LCD_HSYNC_BACK_PORCH 8
#define LCD_HSYNC_FRONT_PORCH 8
#define LCD_VSYNC_PULSE 4
#define LCD_VSYNC_BACK_PORCH 8
#define LCD_VSYNC_FRONT_PORCH 8
#define LCD_PCLK_ACTIVE_NEG 1

#define LCD_PIN_HSYNC 46
#define LCD_PIN_VSYNC 3
#define LCD_PIN_DE 5
#define LCD_PIN_PCLK 7
// B3..B7, G2..G7, R3..R7 of the panel, in the order the rgb565 bits go out
#define LCD_PINS_DATA \
  { 14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40 }

// The panel is refreshed from a frame buffer in the psram. Without a bounce
// buffer the picture drifts whenever the wifi or the flash keeps the cache busy.
#define LCD_BOUNCE_BUFFER_LINES 10

#define I2C_PIN_SDA 8
#define I2C_PIN_SCL 9
#define I2C_FREQUENCY 400000
#define TOUCH_PIN_INT 4

// CH422G: every register is an i2c address of its own
#define CH422G_ADDR_MODE 0x24
#define CH422G_ADDR_OUT 0x38
#define CH422G_MODE_OUTPUT 0x01
#define EXIO_TOUCH_RESET 0x02  // EXIO1
#define EXIO_BACKLIGHT 0x04    // EXIO2
#define EXIO_LCD_RESET 0x08    // EXIO3
#define EXIO_SD_CS 0x10        // EXIO4
#define EXIO_USB_SEL 0x20      // EXIO5, high hands the usb pins to the can transceiver

// ---------------------------------------------------------------------------
// wall clock
// ---------------------------------------------------------------------------

// The displays keep UTC and know no time zone, the controller tells them the
// offset. POSIX notation, this one is Central European Time with summer time.
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// ---------------------------------------------------------------------------
// protocol - has to match the display firmware and the android app
// ---------------------------------------------------------------------------

// the radio channel the boards meet on is LINK_CHANNEL in espnow_link.h
#define MAX_DISPLAYS 12
