#include "pager_message.h"
#include "../../modules/pager_id.h"
#include "../../modules/message_uuid.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"
#include "mbedtls/sha256.h"

static bool datetime_valid(const char *text)
{
	size_t n = strlen(text);
	if ((n != 20 && n != 25) || text[4] != '-' || text[7] != '-' ||
	    text[10] != 'T' || text[13] != ':' || text[16] != ':') return false;
	for (size_t i = 0; i < 19; i++) {
		if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 &&
		    !isdigit((unsigned char)text[i])) return false;
	}
	int y, mo, d, h, mi, s;
	if (sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &s) != 6 ||
	    y < 1970 || mo < 1 || mo > 12 || d < 1 || h > 23 || mi > 59 || s > 59) return false;
	static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	int max_days = days[mo - 1] + (mo == 2 && y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
	if (d > max_days) return false;
	if (n == 20) return text[19] == 'Z';
	if ((text[19] != '+' && text[19] != '-') || text[22] != ':' ||
	    !isdigit((unsigned char)text[20]) || !isdigit((unsigned char)text[21]) ||
	    !isdigit((unsigned char)text[23]) || !isdigit((unsigned char)text[24])) return false;
	int offset_h = (text[20] - '0') * 10 + text[21] - '0';
	int offset_m = (text[23] - '0') * 10 + text[24] - '0';
	return offset_h <= 14 && offset_m <= 59 && (offset_h != 14 || offset_m == 0);
}

static bool text_copy(const cJSON *object, const char *key, char *target, size_t capacity,
		      bool required)
{
	const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, key);
	if (!cJSON_IsString(value)) return false;
	size_t size = strlen(value->valuestring);
	if (size >= capacity || (required && !size)) return false;
	memcpy(target, value->valuestring, size + 1);
	return true;
}

static bool message_id_valid(const char *id)
{
	if (bepio_message_uuid_valid(id)) return true;
	if (strlen(id) != 39 || strncmp(id, "legacy:", 7)) return false;
	for (size_t i = 7; i < 39; i++) {
		if (!((id[i] >= '0' && id[i] <= '9') || (id[i] >= 'a' && id[i] <= 'f'))) return false;
	}
	return true;
}

/* Hash decoded fields in a fixed order, independent of input JSON formatting.
 * Location and reception time intentionally do not affect compatibility IDs. */
static int assign_legacy_id(struct bepio_pager_message *message)
{
	const char *fields[] = {message->from, message->to, message->text, message->datetime};
	const char *notification[] = {"notification", message->from, message->to, message->app,
		message->title, message->text, message->datetime};
	cJSON *array = message->is_notification ? cJSON_CreateStringArray(notification, 7) :
		cJSON_CreateStringArray(fields, 4);
	if (!array) return -1;
	char *canonical = cJSON_PrintUnformatted(array);
	cJSON_Delete(array);
	if (!canonical) return -1;
	unsigned char digest[32];
	int ret = mbedtls_sha256((const unsigned char *)canonical, strlen(canonical), digest, 0);
	cJSON_free(canonical);
	if (ret) return -1;
	memcpy(message->id, "legacy:", 7);
	static const char hex[] = "0123456789abcdef";
	for (size_t i = 0; i < 16; i++) {
		message->id[7 + i * 2] = hex[digest[i] >> 4];
		message->id[8 + i * 2] = hex[digest[i] & 15];
	}
	message->id[39] = '\0';
	return 0;
}

static bool message_valid(const struct bepio_pager_message *message)
{
	if (!message || strnlen(message->id, sizeof(message->id)) == sizeof(message->id) ||
	    !message_id_valid(message->id) || strnlen(message->from, sizeof(message->from)) == sizeof(message->from) ||
	    strnlen(message->to, sizeof(message->to)) == sizeof(message->to) ||
	    strnlen(message->text, sizeof(message->text)) == sizeof(message->text) ||
	    strnlen(message->datetime, sizeof(message->datetime)) == sizeof(message->datetime) ||
	    strnlen(message->address, sizeof(message->address)) == sizeof(message->address) ||
	    !bepio_pager_id_valid(message->from, BEPIO_PAGER_NAME_MAX) ||
	    !bepio_pager_id_valid(message->to, BEPIO_PAGER_NAME_MAX) ||
	    !datetime_valid(message->datetime)) return false;
	if (message->is_notification &&
	    (strnlen(message->app, sizeof(message->app)) == sizeof(message->app) ||
	     strnlen(message->title, sizeof(message->title)) == sizeof(message->title) ||
	     !message->app[0] || (!message->title[0] && !message->text[0]))) return false;
	if (message->has_location &&
	    (!isfinite(message->latitude) || !isfinite(message->longitude) ||
	     message->latitude < -90 || message->latitude > 90 ||
	     message->longitude < -180 || message->longitude > 180)) return false;
	return true;
}

/* Optional defaults do not permit unknown or duplicate protocol fields. */
static bool fields_valid(const cJSON *root)
{
	static const char *const keys[] = {
		"Version", "Id", "Type", "From", "To", "Text", "Datetime", "Location", "App", "Title",
	};
	if (!cJSON_IsObject(root)) return false;
	unsigned seen = 0;
	const cJSON *field;
	cJSON_ArrayForEach(field, root) {
		size_t i;
		for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
			if (!strcmp(field->string, keys[i])) break;
		}
		if (i == sizeof(keys) / sizeof(keys[0]) || (seen & (1u << i))) return false;
		seen |= 1u << i;
	}
	return true;
}

int bepio_pager_message_parse(const unsigned char *data, size_t size,
			      struct bepio_pager_message *message)
{
	if (!data || !message || !size || size >= BEPIO_PAGER_WIRE_MAX || memchr(data, 0, size)) return -1;
	char *copy = malloc(size + 1);
	if (!copy) return -1;
	memcpy(copy, data, size);
	copy[size] = '\0';
	cJSON *root = cJSON_ParseWithLengthOpts(copy, size + 1, NULL, true);
	free(copy);
	struct bepio_pager_message parsed = {0};
	bool valid = fields_valid(root) &&
		text_copy(root, "From", parsed.from, sizeof(parsed.from), true) &&
		text_copy(root, "To", parsed.to, sizeof(parsed.to), true) &&
		text_copy(root, "Text", parsed.text, sizeof(parsed.text), false) &&
		text_copy(root, "Datetime", parsed.datetime, sizeof(parsed.datetime), true);
	const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "Version");
	const cJSON *type = cJSON_GetObjectItemCaseSensitive(root, "Type");
	valid = valid && (!version || (cJSON_IsNumber(version) && version->valuedouble == 1)) &&
		(!type || (cJSON_IsString(type) && (!strcmp(type->valuestring, "message") ||
		 !strcmp(type->valuestring, "notification"))));
	parsed.is_notification = cJSON_IsString(type) && !strcmp(type->valuestring, "notification");
	const cJSON *app = cJSON_GetObjectItemCaseSensitive(root, "App");
	const cJSON *title = cJSON_GetObjectItemCaseSensitive(root, "Title");
	if (parsed.is_notification) {
		valid = valid && text_copy(root, "App", parsed.app, sizeof(parsed.app), true) &&
			(!title || text_copy(root, "Title", parsed.title, sizeof(parsed.title), false));
	} else if (app || title) valid = false;
	const cJSON *at = cJSON_GetObjectItemCaseSensitive(root, "Location");
	if (cJSON_IsObject(at) && cJSON_GetArraySize(at) == 3) {
		const cJSON *latitude = cJSON_GetObjectItemCaseSensitive(at, "Latitude");
		const cJSON *longitude = cJSON_GetObjectItemCaseSensitive(at, "Longitude");
		valid = valid && cJSON_IsNumber(latitude) && cJSON_IsNumber(longitude) &&
			text_copy(at, "Address", parsed.address, sizeof(parsed.address), false);
		if (valid) {
			parsed.has_location = true;
			parsed.latitude = latitude->valuedouble;
			parsed.longitude = longitude->valuedouble;
		}
	} else if (at && !cJSON_IsNull(at)) valid = false;
	const cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "Id");
	valid = valid && (id ? text_copy(root, "Id", parsed.id, sizeof(parsed.id), true) :
		assign_legacy_id(&parsed) == 0);
	valid = valid && message_valid(&parsed);
	cJSON_Delete(root);
	if (!valid) return -1;
	*message = parsed;
	return 0;
}

int bepio_pager_message_encode(const struct bepio_pager_message *message,
			       char *json, size_t capacity)
{
	if (!json || !capacity) return -1;
	json[0] = '\0';
	if (!message_valid(message)) return -1;
	cJSON *root = cJSON_CreateObject();
	if (!root) return -1;
	bool valid = cJSON_AddNumberToObject(root, "Version", 1) &&
		cJSON_AddStringToObject(root, "Id", message->id) &&
		cJSON_AddStringToObject(root, "Type", message->is_notification ? "notification" : "message") &&
		cJSON_AddStringToObject(root, "From", message->from) &&
		cJSON_AddStringToObject(root, "To", message->to) &&
		cJSON_AddStringToObject(root, "Text", message->text) &&
		cJSON_AddStringToObject(root, "Datetime", message->datetime);
	if (message->is_notification) {
		valid = valid && cJSON_AddStringToObject(root, "App", message->app) &&
			cJSON_AddStringToObject(root, "Title", message->title);
	}
	if (message->has_location) {
		cJSON *at = cJSON_AddObjectToObject(root, "Location");
		valid = valid && at && cJSON_AddNumberToObject(at, "Latitude", message->latitude) &&
			cJSON_AddNumberToObject(at, "Longitude", message->longitude) &&
			cJSON_AddStringToObject(at, "Address", message->address);
	} else valid = valid && cJSON_AddNullToObject(root, "Location");
	char *encoded = valid ? cJSON_PrintUnformatted(root) : NULL;
	int ret = -1;
	if (encoded && strlen(encoded) < capacity && strlen(encoded) < BEPIO_PAGER_WIRE_MAX) {
		strcpy(json, encoded);
		ret = 0;
	}
	cJSON_free(encoded);
	cJSON_Delete(root);
	return ret;
}

int bepio_pager_message_stamp_new(struct bepio_pager_message *message)
{
	if (!message) return -1;
	if (bepio_message_uuid_generate(message->id)) return -1;
	time_t now = time(NULL);
	struct tm utc;
	if (!gmtime_r(&now, &utc) || utc.tm_year < 124) return -1;
	return strftime(message->datetime, sizeof(message->datetime), "%Y-%m-%dT%H:%M:%SZ", &utc) ? 0 : -1;
}
