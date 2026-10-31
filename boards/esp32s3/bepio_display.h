#ifndef BEPIO_DISPLAY_H_
#define BEPIO_DISPLAY_H_

/* 2.8-inch ST7789T3 SPI LCD with I2C touch (FT6336 module reference; verify on board). */
#define BEPIO_DISPLAY_WIDTH 240
#define BEPIO_DISPLAY_HEIGHT 320

/* Main UI landscape; raw panel/touch coordinates remain 240x320. */
#define BEPIO_SCREEN_WIDTH BEPIO_DISPLAY_HEIGHT
#define BEPIO_SCREEN_HEIGHT BEPIO_DISPLAY_WIDTH
#define BEPIO_DISPLAY_SPI_HZ 40000000
#define BEPIO_DISPLAY_BUFFER_LINES 16
#define BEPIO_DISPLAY_MISO_GPIO 13
#define BEPIO_DISPLAY_LED_GPIO 8
#define BEPIO_DISPLAY_SCK_GPIO 21
#define BEPIO_DISPLAY_MOSI_GPIO 14
#define BEPIO_DISPLAY_DC_GPIO 47
#define BEPIO_DISPLAY_RESET_GPIO 40
#define BEPIO_DISPLAY_CS_GPIO 48

/* Separate I2C controller from audio (I2C0). Address/polarities need datasheets. */
#define BEPIO_DISPLAY_TP_I2C_PORT 1
#define BEPIO_DISPLAY_TP_INT_GPIO 39
#define BEPIO_DISPLAY_TP_SDA_GPIO 38
#define BEPIO_DISPLAY_TP_RST_GPIO 3
#define BEPIO_DISPLAY_TP_SCL_GPIO 45

/* GPIO3/45 are strapping pins; check module pulls before connecting.
 * GPIO48 may share the DevKitC RGB LED. No driver or GPIO init is performed here.
 */

#endif
