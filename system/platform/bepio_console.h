#ifndef BEPIO_CONSOLE_H_
#define BEPIO_CONSOLE_H_
#include <stddef.h>
#define BEPIO_UNUSED(value) ((void)(value))
typedef struct { int reserved; } bepio_console_context_t;
typedef int (*bepio_console_handler_t)(const bepio_console_context_t *, size_t, char **);
typedef struct {
	const char *name;
	bepio_console_handler_t handler;
	int required;
	int optional;
	const char *help;
} bepio_console_command_t;
void bepio_console_print(const bepio_console_context_t *ctx, const char *format, ...)
	__attribute__((format(printf, 2, 3)));
void bepio_console_error(const bepio_console_context_t *ctx, const char *format, ...)
	__attribute__((format(printf, 2, 3)));
int bepio_console_dispatch(const bepio_console_command_t *commands, int argc, char **argv);
int bepio_console_register(const char *name, const char *help, int (*handler)(int, char **));
int bepio_console_init(void);
int bepio_console_start(void);
#define BEPIO_COMMAND(name, sub, help, handler, required, optional) \
	{#name, handler, required, optional, help}
#define BEPIO_COMMAND_END {NULL, NULL, 0, 0, NULL}
#define BEPIO_COMMANDS(name, ...) \
	static const bepio_console_command_t name[] = {__VA_ARGS__}
#define BEPIO_COMMAND_GROUP(name, sub, help, handler) \
	static int name##_dispatch(int argc, char **argv) \
	{ return bepio_console_dispatch(sub, argc, argv); } \
	int bepio_##name##_console_register(void) \
	{ return bepio_console_register(#name, help, name##_dispatch); }
#endif
