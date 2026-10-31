#include "pager_message.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"

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

static bool message_valid(const struct bepio_pager_message *message)
{
	if (!message || strnlen(message->from, sizeof(message->from)) == sizeof(message->from) ||
	    strnlen(message->to, sizeof(message->to)) == sizeof(message->to) ||
	    strnlen(message->text, sizeof(message->text)) == sizeof(message->text) ||
	    strnlen(message->datetime, sizeof(message->datetime)) == sizeof(message->datetime) ||
	    strnlen(message->address, sizeof(message->address)) == sizeof(message->address) ||
	    !message->from[0] || !message->to[0] || !datetime_valid(message->datetime)) return false;
	if (message->has_location &&
	    (!isfinite(message->latitude) || !isfinite(message->longitude) ||
	     message->latitude < -90 || message->latitude > 90 ||
	     message->longitude < -180 || message->longitude > 180)) return false;
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
	bool valid = cJSON_IsObject(root) && cJSON_GetArraySize(root) == 5 &&
		text_copy(root, "From", parsed.from, sizeof(parsed.from), true) &&
		text_copy(root, "To", parsed.to, sizeof(parsed.to), true) &&
		text_copy(root, "Text", parsed.text, sizeof(parsed.text), false) &&
		text_copy(root, "Datetime", parsed.datetime, sizeof(parsed.datetime), true);
	const cJSON *at = cJSON_GetObjectItemCaseSensitive(root, "At");
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
	} else if (!cJSON_IsNull(at)) valid = false;
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
	bool valid = cJSON_AddStringToObject(root, "From", message->from) &&
		cJSON_AddStringToObject(root, "To", message->to) &&
		cJSON_AddStringToObject(root, "Text", message->text) &&
		cJSON_AddStringToObject(root, "Datetime", message->datetime);
	if (message->has_location) {
		cJSON *at = cJSON_AddObjectToObject(root, "At");
		valid = valid && at && cJSON_AddNumberToObject(at, "Latitude", message->latitude) &&
			cJSON_AddNumberToObject(at, "Longitude", message->longitude) &&
			cJSON_AddStringToObject(at, "Address", message->address);
	} else valid = valid && cJSON_AddNullToObject(root, "At");
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

int bepio_pager_message_now(struct bepio_pager_message *message)
{
	if (!message) return -1;
	time_t now = time(NULL);
	struct tm utc;
	if (!gmtime_r(&now, &utc) || utc.tm_year < 124) return -1;
	return strftime(message->datetime, sizeof(message->datetime), "%Y-%m-%dT%H:%M:%SZ", &utc) ? 0 : -1;
}
