#include "bepio_console.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_console.h"
static esp_console_repl_t *repl;

void bepio_console_print(const bepio_console_context_t *ctx, const char *format, ...)
{
	va_list args;
	BEPIO_UNUSED(ctx);
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	putchar('\n');
}
void bepio_console_error(const bepio_console_context_t *ctx, const char *format, ...)
{
	va_list args;
	BEPIO_UNUSED(ctx);
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputc('\n', stderr);
}
int bepio_console_dispatch(const bepio_console_command_t *commands, int argc, char **argv)
{
	static const bepio_console_context_t context;
	if (argc < 2 || strcmp(argv[1], "help") == 0) {
		for (const bepio_console_command_t *cmd = commands; cmd->name; cmd++) {
			printf("  %s %s: %s\n", argv[0], cmd->name, cmd->help);
		}
		return 0;
	}
	for (const bepio_console_command_t *cmd = commands; cmd->name; cmd++) {
		if (strcmp(argv[1], cmd->name) != 0) { continue; }
		int count = argc - 1;
		if (count < cmd->required || count > cmd->required + cmd->optional) {
			fprintf(stderr, "%s %s: %s\n", argv[0], cmd->name, cmd->help);
			return -EINVAL;
		}
		return cmd->handler(&context, (size_t)count, argv + 1);
	}
	fprintf(stderr, "Unknown command; use %s help\n", argv[0]);
	return -ENOENT;
}
int bepio_console_register(const char *name, const char *help, int (*handler)(int, char **))
{
	const esp_console_cmd_t command = {.command = name, .help = help, .func = handler};
	return esp_console_cmd_register(&command);
}
int bepio_console_init(void)
{
#ifdef CONFIG_BEPIO_CONSOLE
	esp_console_repl_config_t config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	config.prompt = "bepio>";
	config.max_cmdline_length = 512;
	config.task_stack_size = 6144;
#if CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
	esp_console_dev_uart_config_t uart = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
	int ret = esp_console_new_repl_uart(&uart, &config, &repl);
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
	esp_console_dev_usb_serial_jtag_config_t usb = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
	int ret = esp_console_new_repl_usb_serial_jtag(&usb, &config, &repl);
#else
#error "Bepio console requires UART or USB Serial/JTAG"
#endif
	if (ret != ESP_OK) { return ret; }
	return esp_console_register_help_command();
#else
	return 0;
#endif
}
int bepio_console_start(void)
{
	return repl ? esp_console_start_repl(repl) : ESP_ERR_INVALID_STATE;
}
