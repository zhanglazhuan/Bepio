/* Host command adapter; does not model USB UART transport or tokenization. */
#ifndef HOST_ZEPHYR_SHELL_H_
#define HOST_ZEPHYR_SHELL_H_
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define BEPIO_UNUSED(value) ((void)(value))
typedef struct { int unused; } bepio_console_context_t;
typedef int (*host_shell_handler_t)(const bepio_console_context_t *, size_t, char **);
typedef struct {
	const char *name;
	host_shell_handler_t handler;
	int required;
	int optional;
} bepio_console_command_t;
extern char host_shell_output[4096];
extern int host_shell_errors;
static inline void host_shell_write(bool error, const char *format, va_list args)
{
	size_t used = strlen(host_shell_output);

	if (error) { host_shell_errors++; }
	vsnprintf(host_shell_output + used, sizeof(host_shell_output) - used, format, args);
	used = strlen(host_shell_output);
	if (used < sizeof(host_shell_output) - 1) {
		host_shell_output[used] = '\n';
		host_shell_output[used + 1] = '\0';
	}
}
static inline void bepio_console_print(const bepio_console_context_t *shell, const char *format, ...)
{
	va_list args;
	(void)shell;
	va_start(args, format);
	host_shell_write(false, format, args);
	va_end(args);
}
static inline void bepio_console_error(const bepio_console_context_t *shell, const char *format, ...)
{
	va_list args;
	(void)shell;
	va_start(args, format);
	host_shell_write(true, format, args);
	va_end(args);
}
#define BEPIO_COMMAND(name, sub, help, handler, required, optional) {#name, handler, required, optional}
#define BEPIO_COMMAND_END {NULL, NULL, 0, 0}
#define BEPIO_COMMANDS(name, ...) const bepio_console_command_t name[] = {__VA_ARGS__}
#define BEPIO_COMMAND_GROUP(name, sub, help, handler) const bepio_console_command_t *host_commands_##name = (const bepio_console_command_t *)(sub)
#endif
