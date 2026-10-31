#ifndef BEPIO_DEVKIT_H_
#define BEPIO_DEVKIT_H_
/* Verified 16 MB flash; PSRAM remains unverified and disabled. */
#define BEPIO_BOARD_NAME "Bepio ESP32-S3 DevKitC"
#define BEPIO_FLASH_SIZE_MB 16
#define BEPIO_USB_DM_GPIO 19
#define BEPIO_USB_DP_GPIO 20
#define BEPIO_MN316_UART_PORT 1
#define BEPIO_MN316_TX_GPIO 17
#define BEPIO_MN316_RX_GPIO 18
#define BEPIO_MN316_BAUD_RATE 9600
#include "bepio_audio.h"
#endif
