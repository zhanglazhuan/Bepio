#ifndef HOST_ESP_CONSOLE_H_
#define HOST_ESP_CONSOLE_H_

#define ESP_ERR_INVALID_STATE 0x103
typedef struct { int unused; } esp_console_repl_t;
typedef struct {
	const char *command;
	const char *help;
	int (*func)(int, char **);
} esp_console_cmd_t;
int esp_console_cmd_register(const esp_console_cmd_t *command);
int esp_console_start_repl(esp_console_repl_t *repl);

#endif
