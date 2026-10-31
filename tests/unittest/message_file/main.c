#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bepio_message_file.h"
#include "bepio_store.h"

static int initialized;
int bepio_store_read(const char *space, const char *key, void *data, size_t *size)
{
	assert(!strcmp(space, "bepio_msgs") && !strcmp(key, "fs_init"));
	if (!initialized) { return -ENOENT; }
	assert(*size == 1); *(uint8_t *)data = 1; return 0;
}
int bepio_store_write(const char *space, const char *key, const void *data, size_t size)
{
	assert(!strcmp(space, "bepio_msgs") && !strcmp(key, "fs_init"));
	assert(size == 1 && *(const uint8_t *)data == 1);
	initialized = 1; return 0;
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	assert(bepio_message_file_mount() == 0 && initialized);
	uint8_t input[256], output[256];
	for (unsigned i = 0; i < sizeof(input); i++) { input[i] = i; }
	assert(bepio_message_file_read(output, sizeof(output)) == -ENOENT);
	FILE *file = fopen(argv[1], "wb"); assert(file);
	assert(fputs("BEPIO_MESSAGES_V1\n", file) != EOF);
	uint32_t hash = 2166136261u;
	for (unsigned i = 0; i < sizeof(input); i++) {
		assert(fprintf(file, "%02x", input[i]) == 2);
		hash = (hash ^ input[i]) * 16777619u;
	}
	assert(fprintf(file, "\n%08x\n", (unsigned)hash) > 0);
	fclose(file);
	assert(bepio_message_file_read(output, sizeof(output)) == 0);
	assert(!memcmp(input, output, sizeof(input)));
	assert(bepio_message_file_write(input, sizeof(input)) == 0);
	assert(bepio_message_file_read(output, sizeof(output)) == 0);
	assert(!memcmp(input, output, sizeof(input)));
	input[0] = 42;
	assert(bepio_message_file_write(input, sizeof(input)) == 0);
	assert(bepio_message_file_read(output, sizeof(output)) == 0);
	assert(!memcmp(input, output, sizeof(input)));
	file = fopen(argv[1], "rb+"); assert(file);
	assert(fseek(file, strlen("BEPIO_MESSAGES_V2 2\n") + 8, SEEK_SET) == 0);
	assert(fputc('z', file) != EOF);
	fclose(file);
	assert(bepio_message_file_read(output, sizeof(output)) == 0);
	assert(output[0] == 0);
	puts("PASS: legacy read, repeated writes, and corrupt latest fallback");
	return 0;
}
