#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_board.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "esp_partition.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "model_path.h"

typedef struct { int id; const char *intent; const char *text; const char *pinyin; } phrase_t;
static const phrase_t phrases[] = {
	{1, "send_location", "发送位置", "fa song wei zhi"},
	{1, "send_location", "告诉妈妈我在哪里", "gao su ma ma wo zai na li"},
	{2, "request_pickup", "请来接我", "qing lai jie wo"},
	{2, "request_pickup", "来接我回家", "lai jie wo hui jia"},
	{3, "report_safe", "我已经到家了", "wo yi jing dao jia le"},
	{4, "play_messages", "播放消息", "bo fang xiao xi"},
	{4, "play_messages", "读一下消息", "du yi xia xiao xi"},
	{5, "cancel", "取消操作", "qu xiao cao zuo"},
};
static const char *TAG = "t_multinet";
static QueueHandle_t requests;
static atomic_bool busy, cancelled, ready;
static const esp_mn_iface_t *mn;
static model_iface_data_t *model;
static srmodel_list_t *models;
static int16_t *stereo, *mono;
static int chunk;
typedef struct { int seconds, slot, gain; bool digital; } request_t;

/* Reject erased/truncated model directories before the vendor loader parses them. */
static esp_err_t validate_models(void)
{
	const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
		ESP_PARTITION_SUBTYPE_ANY, "model");
	if (!p) { return ESP_ERR_NOT_FOUND; }
	uint32_t count, offset = 4;
	if (esp_partition_read(p, 0, &count, 4) != ESP_OK || count != 2) {
		return ESP_ERR_INVALID_RESPONSE;
	}
	for (uint32_t i = 0; i < count; i++) {
		char name[32];
		uint32_t files;
		if (offset > p->size - 36 || esp_partition_read(p, offset, name, 32) != ESP_OK ||
		    !memchr(name, 0, sizeof(name)) ||
		    esp_partition_read(p, offset + 32, &files, 4) != ESP_OK || files == 0 || files > 64) {
			return ESP_ERR_INVALID_RESPONSE;
		}
		offset += 36;
		for (uint32_t j = 0; j < files; j++) {
			uint32_t range[2];
			if (offset > p->size - 40 || esp_partition_read(p, offset, name, 32) != ESP_OK ||
			    !memchr(name, 0, sizeof(name)) ||
			    esp_partition_read(p, offset + 32, range, 8) != ESP_OK ||
			    range[0] > p->size || range[1] > p->size - range[0]) {
				return ESP_ERR_INVALID_RESPONSE;
			}
			offset += 40;
		}
	}
	return ESP_OK;
}

static esp_err_t initialize(void)
{
	if (atomic_load(&ready)) { return ESP_OK; }
	if (!esp_psram_is_initialized()) { return ESP_ERR_NO_MEM; }
	ESP_LOGI(TAG, "PSRAM free: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
	esp_err_t ret = validate_models();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Invalid model partition; use full flash (not app-flash)");
		return ret;
	}
	models = esp_srmodel_init("model");
	char *name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_CHINESE);
	if (!name || !(mn = esp_mn_handle_from_name(name))) { ret = ESP_ERR_NOT_FOUND; goto fail; }
	model = mn->create(name, 8000);
	if (!model) { ret = ESP_ERR_NO_MEM; goto fail; }
	if (mn->get_samp_rate(model) != BEPIO_AUDIO_TEST_RATE) { ret = ESP_ERR_NOT_SUPPORTED; goto fail; }
	chunk = mn->get_samp_chunksize(model);
	if (chunk <= 0 || chunk > 4096) { ret = ESP_ERR_INVALID_SIZE; goto fail; }
	stereo = heap_caps_malloc(chunk * 2 * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	mono = heap_caps_malloc(chunk * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
	if (!stereo || !mono) { ret = ESP_ERR_NO_MEM; goto fail; }
	ret = esp_mn_commands_alloc(mn, model);
	if (ret != ESP_OK) { goto fail; }
	for (size_t i = 0; i < sizeof(phrases) / sizeof(phrases[0]); i++) {
		ret = esp_mn_commands_add(phrases[i].id, phrases[i].pinyin);
		if (ret != ESP_OK) { goto fail; }
	}
	esp_mn_error_t *errors = esp_mn_commands_update();
	if (errors && errors->num) { ret = ESP_ERR_INVALID_ARG; goto fail; }
	ESP_LOGI(TAG, "Ready: %s, %d samples/chunk, 16 kHz mono", name, chunk);
	atomic_store(&ready, true);
	return ESP_OK;
fail:
	esp_mn_commands_free();
	free(stereo); stereo = NULL;
	free(mono); mono = NULL;
	if (model) { mn->destroy(model); model = NULL; }
	if (models) { esp_srmodel_deinit(models); models = NULL; }
	return ret;
}

static esp_err_t listen(const request_t *r)
{
	esp_err_t ret = bepio_audio_board_init(r->digital, 0, r->gain);
	if (ret != ESP_OK) { return ret; }
	ret = bepio_audio_board_reset_input();
	if (ret != ESP_OK) { goto out; }
	mn->clean(model);
	ESP_LOGI(TAG, "Listening %d seconds, %s mic, %s slot, gain %d dB; speak now",
		r->seconds, r->digital ? "digital" : "analog", r->slot ? "right" : "left", r->gain);
	int64_t deadline = esp_timer_get_time() + (int64_t)r->seconds * 1000000;
	bool detected = false;
	int peak = 0;
	while (!atomic_load(&cancelled) && esp_timer_get_time() < deadline) {
		ret = bepio_audio_board_read(stereo, chunk);
		if (ret != ESP_OK) { break; }
		for (int i = 0; i < chunk; i++) {
			mono[i] = stereo[2 * i + r->slot];
			int amplitude = abs((int)mono[i]);
			if (amplitude > peak) { peak = amplitude; }
		}
		esp_mn_state_t state = mn->detect(model, mono);
		if (state == ESP_MN_STATE_DETECTED) {
			esp_mn_results_t *result = mn->get_results(model);
			if (result && result->num > 0 && result->num <= ESP_MN_RESULT_MAX_NUM) {
				for (size_t i = 0; i < sizeof(phrases) / sizeof(phrases[0]); i++) {
					if (phrases[i].id == result->command_id[0]) {
						printf("intent=%s command_id=%d confidence=%.3f canonical_text=%s\n",
							phrases[i].intent, phrases[i].id, result->prob[0], phrases[i].text);
						detected = true;
						break;
					}
				}
			}
			break;
		}
		if (state == ESP_MN_STATE_TIMEOUT) { break; }
	}
	ESP_LOGI(TAG, "Session ended: %s, peak=%d/32768", atomic_load(&cancelled) ? "cancelled" :
		detected ? "matched" : "no match", peak);
	mn->clean(model);
out:
	bepio_audio_board_deinit();
	return ret;
}

static void worker(void *arg)
{
	(void)arg;
	request_t request;
	for (;;) {
		xQueueReceive(requests, &request, portMAX_DELAY);
		esp_err_t ret = initialize();
		if (ret == ESP_OK && request.seconds) { ret = listen(&request); }
		if (ret != ESP_OK) { ESP_LOGE(TAG, "%s", esp_err_to_name(ret)); }
		atomic_store(&busy, false);
	}
}

static bool number(const char *s, int min, int max, int *value)
{
	char *end;
	errno = 0;
	long n = strtol(s, &end, 10);
	if (errno || end == s || *end || n < min || n > max) { return false; }
	*value = (int)n;
	return true;
}

static int command(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "stop")) { atomic_store(&cancelled, true); return 0; }
	if (argc == 2 && !strcmp(argv[1], "status")) {
		printf("ready=%d busy=%d psram_free=%u\n", atomic_load(&ready), atomic_load(&busy),
			(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
		return 0;
	}
	if (argc == 2 && !strcmp(argv[1], "commands")) {
		for (size_t i = 0; i < sizeof(phrases) / sizeof(phrases[0]); i++) {
			printf("%d %-16s %s [%s]\n", phrases[i].id, phrases[i].intent, phrases[i].text, phrases[i].pinyin);
		}
		return 0;
	}
	request_t r = {.seconds = 8, .gain = 24};
	if (argc == 2 && !strcmp(argv[1], "init")) { r.seconds = 0; }
	else if (argc >= 2 && argc <= 6 && !strcmp(argv[1], "listen")) {
		if (argc > 2 && !number(argv[2], 1, 8, &r.seconds)) { goto usage; }
		if (argc > 3) {
			if (!strcmp(argv[3], "right")) { r.slot = 1; }
			else if (strcmp(argv[3], "left")) { goto usage; }
		}
		if (argc > 4) {
			if (!strcmp(argv[4], "digital")) { r.digital = true; }
			else if (strcmp(argv[4], "analog")) { goto usage; }
		}
		if (argc > 5 && (!number(argv[5], 0, 42, &r.gain) || r.gain % 6)) { goto usage; }
	} else { goto usage; }
	if (atomic_exchange(&busy, true)) { puts("Busy; mn stop first"); return 1; }
	atomic_store(&cancelled, false);
	if (xQueueSend(requests, &r, 0) != pdTRUE) { atomic_store(&busy, false); return 1; }
	return 0;
usage:
	puts("mn init | status | commands | stop\nmn listen [1..8 seconds] [left|right] [analog|digital] [gain 0..42 step 6]");
	return 1;
}

void app_main(void)
{
	requests = xQueueCreate(1, sizeof(request_t));
	ESP_ERROR_CHECK(requests ? ESP_OK : ESP_ERR_NO_MEM);
	ESP_ERROR_CHECK(xTaskCreatePinnedToCore(worker, "multinet", 12288, NULL, 5, NULL, 1) == pdPASS
		? ESP_OK : ESP_ERR_NO_MEM);
	esp_console_repl_t *repl;
	esp_console_repl_config_t config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
	config.prompt = "mn>";
	config.task_stack_size = 4096;
	esp_console_dev_usb_serial_jtag_config_t usb = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usb, &config, &repl));
	ESP_ERROR_CHECK(esp_console_register_help_command());
	const esp_console_cmd_t cmd = {.command = "mn", .help = "Chinese intent test: mn commands; mn listen", .func = command};
	ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
	ESP_ERROR_CHECK(esp_console_start_repl(repl));
	ESP_LOGI(TAG, "N16R8 MultiNet7 command test; run mn commands, then mn listen");
}
