#ifndef TEST_UART_H_
#define TEST_UART_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int uart_port_t;
typedef struct {
	int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk;
} uart_config_t;
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE (-1)
#define ESP_OK 0
bool uart_is_driver_installed(uart_port_t port);
int uart_driver_install(uart_port_t port, int rx, int tx, int queue,
	void *handle, int flags);
int uart_param_config(uart_port_t port, const uart_config_t *config);
int uart_set_pin(uart_port_t port, int tx, int rx, int rts, int cts);
int uart_flush_input(uart_port_t port);
int uart_write_bytes(uart_port_t port, const void *data, size_t size);
int uart_wait_tx_done(uart_port_t port, int ticks);
int uart_read_bytes(uart_port_t port, void *data, size_t size, int ticks);
int uart_set_baudrate(uart_port_t port, int baud);
#endif
