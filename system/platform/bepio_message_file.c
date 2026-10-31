#include "bepio_message_file.h"
#include "bepio_store.h"
#include "esp_spiffs.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#ifndef BEPIO_MESSAGE_FILE_PATH
#define BEPIO_MESSAGE_FILE_PATH "/storage/messages.txt"
#endif
#ifndef BEPIO_MESSAGE_TEMP_PATH
#define BEPIO_MESSAGE_TEMP_PATH "/storage/messages.tmp"
#endif
#define MESSAGE_PATH BEPIO_MESSAGE_FILE_PATH
#define TEMP_PATH BEPIO_MESSAGE_TEMP_PATH
static bool mounted;
static bool selection_ready;
static const char *active_path;
static uint32_t active_generation;

int bepio_message_file_mount(void)
{
	if (mounted) { return 0; }
	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/storage", .partition_label = "storage", .max_files = 3,
		.format_if_mount_failed = false,
	};
	uint8_t initialized = 0;
	size_t size = sizeof(initialized);
	int stored = bepio_store_read("bepio_msgs", "fs_init", &initialized, &size);
	if (stored && stored != -ENOENT) { return stored; }
	if (!stored && (size != sizeof(initialized) || initialized != 1)) { return -EIO; }
	esp_err_t ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK) {
		if (!stored) { return -EIO; }
		conf.format_if_mount_failed = true;
		if (esp_vfs_spiffs_register(&conf) != ESP_OK) { return -EIO; }
	}
	mounted = true;
	selection_ready = false;
	active_path = NULL;
	active_generation = 0;
	if (stored == -ENOENT) {
		initialized = 1;
		return bepio_store_write("bepio_msgs", "fs_init", &initialized, sizeof(initialized));
	}
	return 0;
}

static uint32_t checksum(uint32_t value, uint8_t byte)
{
	return (value ^ byte) * 16777619u;
}

static int hex_value(char c)
{
	if (c >= '0' && c <= '9') { return c - '0'; }
	if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
	return -1;
}

static int read_slot(const char *path, void *data, size_t size, uint32_t *generation)
{
	FILE *file = fopen(path, "rb");
	if (!file) { return errno == ENOENT ? -ENOENT : -EIO; }
	char header[32];
	int ret = -EIO;
	if (!fgets(header, sizeof(header), file)) { goto done; }
	uint32_t sequence = 0;
	if (strcmp(header, "BEPIO_MESSAGES_V1\n")) {
		unsigned parsed;
		int consumed = 0;
		if (sscanf(header, "BEPIO_MESSAGES_V2 %u%n", &parsed, &consumed) != 1 ||
		    !parsed || header[consumed] != '\n' || header[consumed + 1] != 0) { goto done; }
		sequence = parsed;
	}
	uint8_t *bytes = data;
	uint32_t hash = 2166136261u;
	for (size_t offset = 0; offset < size;) {
		char encoded[512];
		size_t chunk = size - offset < 256 ? size - offset : 256;
		if (fread(encoded, 1, chunk * 2, file) != chunk * 2) { goto done; }
		for (size_t i = 0; i < chunk; i++) {
			int high = hex_value(encoded[2 * i]);
			int low = hex_value(encoded[2 * i + 1]);
			if (high < 0 || low < 0) { goto done; }
			bytes[offset + i] = (uint8_t)((high << 4) | low);
			hash = checksum(hash, bytes[offset + i]);
		}
		offset += chunk;
	}
	unsigned stored;
	if (fscanf(file, "\n%8x\n", &stored) != 1 || stored != hash || fgetc(file) != EOF) { goto done; }
	ret = 0;
	*generation = sequence;
done:
	fclose(file);
	return ret;
}

static uint32_t slot_generation(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (!file) { return 0; }
	char header[32];
	uint32_t generation = 0;
	if (fgets(header, sizeof(header), file) && strcmp(header, "BEPIO_MESSAGES_V1\n")) {
		unsigned parsed;
		if (sscanf(header, "BEPIO_MESSAGES_V2 %u", &parsed) == 1) { generation = parsed; }
	}
	fclose(file);
	return generation;
}

int bepio_message_file_read(void *data, size_t size)
{
	if (!mounted || !data) { return -EINVAL; }
	const char *first = MESSAGE_PATH;
	const char *second = TEMP_PATH;
	if (slot_generation(second) > slot_generation(first)) {
		first = TEMP_PATH;
		second = MESSAGE_PATH;
	}
	uint32_t generation;
	int first_ret = read_slot(first, data, size, &generation);
	if (!first_ret) {
		active_path = first; active_generation = generation; selection_ready = true;
		return 0;
	}
	int second_ret = read_slot(second, data, size, &generation);
	if (!second_ret) {
		active_path = second; active_generation = generation; selection_ready = true;
		return 0;
	}
	if (first_ret == -ENOENT && second_ret == -ENOENT) {
		active_path = NULL; active_generation = 0; selection_ready = true;
		return -ENOENT;
	}
	return -EIO;
}

int bepio_message_file_write(const void *data, size_t size)
{
	if (!mounted || !data || !selection_ready) { return -EINVAL; }
	const char *target = active_path && !strcmp(active_path, MESSAGE_PATH) ? TEMP_PATH : MESSAGE_PATH;
	uint32_t generation = active_generation + 1;
	if (!generation) { generation = 1; }
	FILE *file = fopen(target, "wb");
	if (!file) { return -EIO; }
	const uint8_t *bytes = data;
	uint32_t hash = 2166136261u;
	bool failed = fprintf(file, "BEPIO_MESSAGES_V2 %u\n", (unsigned)generation) < 0;
	static const char hex[] = "0123456789abcdef";
	for (size_t offset = 0; offset < size && !failed;) {
		char encoded[512];
		size_t chunk = size - offset < 256 ? size - offset : 256;
		for (size_t i = 0; i < chunk; i++) {
			uint8_t byte = bytes[offset + i];
			encoded[2 * i] = hex[byte >> 4];
			encoded[2 * i + 1] = hex[byte & 0x0f];
			hash = checksum(hash, byte);
		}
		failed = fwrite(encoded, 1, chunk * 2, file) != chunk * 2;
		offset += chunk;
	}
	if (!failed) { failed = fprintf(file, "\n%08x\n", (unsigned)hash) < 0; }
	if (fflush(file) != 0) { failed = true; }
	if (fsync(fileno(file)) != 0) { failed = true; }
	if (fclose(file) != 0) { failed = true; }
	if (failed) { return -EIO; }
	active_path = target;
	active_generation = generation;
	return 0;
}
