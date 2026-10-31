#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "bepio_console.h"
#include "esp_console.h"

static esp_console_cmd_t registered;
static unsigned calls;

int esp_console_cmd_register(const esp_console_cmd_t *command)
{
	registered = *command;
	return 0;
}

int esp_console_start_repl(esp_console_repl_t *repl)
{
	(void)repl;
	assert(0 && "No USB REPL in host tests");
	return 0;
}

static int compose(const bepio_console_context_t *ctx, size_t argc, char **argv)
{
	assert(ctx != NULL);
	assert(argc == 2 || argc == 3);
	assert(strcmp(argv[0], "new") == 0);
	assert(strcmp(argv[1], "+8613800000000") == 0);
	if (argc == 3) {
		assert(strcmp(argv[2], "Mom Smith") == 0);
	}
	calls++;
	return -EBUSY; /* Business errors must pass through unchanged. */
}

BEPIO_COMMANDS(commands,
	BEPIO_COMMAND(new, NULL, "Compose <phone> [name]", compose, 2, 1),
	BEPIO_COMMAND_END);
BEPIO_COMMAND_GROUP(sample, commands, "Sample group", NULL);

int main(void)
{
	char *valid[] = {"sample", "new", "+8613800000000", "Mom Smith", "extra"};
	char *help[] = {"sample", "help"};
	char *unknown[] = {"sample", "unknown"};

	assert(bepio_sample_console_register() == 0);
	assert(strcmp(registered.command, "sample") == 0);
	assert(registered.func(3, valid) == -EBUSY);
	assert(registered.func(4, valid) == -EBUSY);
	assert(calls == 2);
	assert(registered.func(2, valid) == -EINVAL);
	assert(registered.func(5, valid) == -EINVAL);
	assert(registered.func(2, unknown) == -ENOENT);
	assert(registered.func(1, valid) == 0);
	assert(registered.func(2, help) == 0);
	assert(calls == 2);
	assert(bepio_console_start() == ESP_ERR_INVALID_STATE);
	puts("PASS: native console registration, argv, optional arguments, arity, help, errors");
	return 0;
}
