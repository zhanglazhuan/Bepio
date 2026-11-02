#include "pager_message.h"
#include "messaging.h"
#include <errno.h>
#include "../../../modules/message_uuid.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int parse(cJSON *root)
{
	char *json = cJSON_PrintUnformatted(root);
	assert(json);
	struct bepio_pager_message message;
	int result = bepio_pager_message_parse((const unsigned char *)json, strlen(json), &message);
	cJSON_free(json);
	return result;
}

int main(void)
{
	struct bepio_pager_message message = {0}, decoded;
	strcpy(message.from, "30962488619320");
	strcpy(message.to, "101");
	strcpy(message.text, "ggg");
	assert(!bepio_pager_message_stamp_new(&message));
	assert(bepio_message_uuid_valid(message.id));
	char json[BEPIO_PAGER_WIRE_MAX], first_id[37];
	strcpy(first_id, message.id);
	assert(!bepio_pager_message_encode(&message, json, sizeof(json)));
	assert(!bepio_pager_message_parse((const unsigned char *)json, strlen(json), &decoded));
	assert(!strcmp(decoded.id, message.id) && !strcmp(decoded.datetime, message.datetime));
	assert(!decoded.has_location);
	cJSON *root = cJSON_Parse(json);
	assert(root && cJSON_GetArraySize(root) == 8);
	assert(cJSON_IsNull(cJSON_GetObjectItemCaseSensitive(root, "Location")));
	assert(!cJSON_GetObjectItemCaseSensitive(root, "At"));
	cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(root, "Version"), 2);
	assert(parse(root));
	cJSON_SetNumberValue(cJSON_GetObjectItemCaseSensitive(root, "Version"), 1);
	cJSON_ReplaceItemInObjectCaseSensitive(root, "Type", cJSON_CreateString("receipt"));
	assert(parse(root));
	cJSON_ReplaceItemInObjectCaseSensitive(root, "Type", cJSON_CreateString("message"));
	cJSON_ReplaceItemInObjectCaseSensitive(root, "Id", cJSON_CreateString("content-md5"));
	assert(parse(root));
	cJSON_ReplaceItemInObjectCaseSensitive(root, "Id", cJSON_CreateString(message.id));
	assert(!parse(root));
	/* All combinations of omitted default fields must normalize identically. */
	for (unsigned mask = 0; mask < 8; mask++) {
		cJSON *minimal = cJSON_Duplicate(root, true);
		assert(minimal);
		if (mask & 1) cJSON_DeleteItemFromObjectCaseSensitive(minimal, "Version");
		if (mask & 2) cJSON_DeleteItemFromObjectCaseSensitive(minimal, "Type");
		if (mask & 4) cJSON_DeleteItemFromObjectCaseSensitive(minimal, "Location");
		char *wire = cJSON_PrintUnformatted(minimal);
		assert(wire);
		assert(!bepio_pager_message_parse((const unsigned char *)wire, strlen(wire), &decoded));
		assert(!decoded.has_location && !strcmp(decoded.id, message.id));
		assert(!bepio_pager_message_encode(&decoded, json, sizeof(json)));
		assert(strstr(json, "\"Version\":1") && strstr(json, "\"Type\":\"message\""));
		assert(strstr(json, "\"Location\":null"));
		cJSON_free(wire);
		cJSON_Delete(minimal);
	}
	cJSON_ReplaceItemInObjectCaseSensitive(root, "Version", cJSON_CreateNull());
	assert(parse(root));
	cJSON_ReplaceItemInObjectCaseSensitive(root, "Version", cJSON_CreateNumber(1));
	cJSON_ReplaceItemInObjectCaseSensitive(root, "Type", cJSON_CreateNull());
	assert(parse(root));
	cJSON_ReplaceItemInObjectCaseSensitive(root, "Type", cJSON_CreateString("message"));
	cJSON_AddNumberToObject(root, "Version", 1);
	assert(parse(root)); /* Duplicate keys are ambiguous even with valid values. */
	cJSON_DeleteItemFromObjectCaseSensitive(root, "Version");
	assert(!parse(root));
	cJSON_DeleteItemFromObjectCaseSensitive(root, "Id");
	assert(!parse(root)); /* Missing identity is derived deterministically. */
	cJSON_AddStringToObject(root, "Id", message.id);
	cJSON_DeleteItemFromObjectCaseSensitive(root, "Location");
	cJSON_AddNullToObject(root, "At");
	assert(parse(root));
	cJSON_Delete(root);

	const char *minimal = "{\"From\":\"30962488619320\",\"To\":\"101\",\"Text\":\"ggg\",\"Datetime\":\"2026-11-02T01:00:37Z\"}";
	assert(!bepio_pager_message_parse((const unsigned char *)minimal, strlen(minimal), &decoded));
	assert(!strcmp(decoded.id, "legacy:309729ccc10f3a4caf67952114448766"));
	char legacy_id[40];
	strcpy(legacy_id, decoded.id);
	assert(!bepio_messaging_init());
	bepio_message_payload_t payload = {0};
	strcpy(payload.phone, decoded.from);
	strcpy(payload.body, decoded.text);
	uint32_t received_id;
	assert(!bepio_messaging_receive(decoded.id, &payload, 0, false, &received_id));
	const char *reordered = "{ \"Datetime\":\"2026-11-02T01:00:37Z\", \"Text\":\"ggg\", \"To\":\"101\", \"From\":\"30962488619320\", \"Location\":null, \"Version\":1, \"Type\":\"message\" }";
	assert(!bepio_pager_message_parse((const unsigned char *)reordered, strlen(reordered), &decoded));
	assert(!strcmp(decoded.id, legacy_id));
	assert(bepio_messaging_receive(decoded.id, &payload, 0, false, &received_id) == -EALREADY);
	assert(!bepio_pager_message_encode(&decoded, json, sizeof(json)));
	assert(!bepio_pager_message_parse((const unsigned char *)json, strlen(json), &decoded));
	assert(!strcmp(decoded.id, legacy_id)); /* Explicit derived IDs survive re-encoding. */
	root = cJSON_Parse(minimal);
	const char *changed[] = {"From", "To", "Text", "Datetime"};
	const char *values[] = {"30962488619321", "102", "different", "2026-11-02T01:00:38Z"};
	for (size_t i = 0; i < 4; i++) {
		cJSON *variant = cJSON_Duplicate(root, true);
		cJSON_ReplaceItemInObjectCaseSensitive(variant, changed[i], cJSON_CreateString(values[i]));
		char *wire = cJSON_PrintUnformatted(variant);
		assert(!bepio_pager_message_parse((const unsigned char *)wire, strlen(wire), &decoded));
		assert(strcmp(decoded.id, legacy_id));
		cJSON_free(wire);
		cJSON_Delete(variant);
	}
	cJSON_AddNullToObject(root, "Id");
	assert(parse(root)); /* Explicit invalid IDs must not silently change identity. */
	cJSON_ReplaceItemInObjectCaseSensitive(root, "Id", cJSON_CreateString(""));
	assert(parse(root));
	cJSON_Delete(root);

	const char *notification = "{\"Type\":\"notification\",\"From\":\"101\",\"To\":\"102\",\"App\":\"Chat\",\"Title\":\"Ada\",\"Text\":\"Hello\",\"Datetime\":\"2026-11-02T01:00:00Z\"}";
	assert(!bepio_pager_message_parse((const unsigned char *)notification, strlen(notification), &decoded));
	assert(decoded.is_notification && !strcmp(decoded.app, "Chat") && !strcmp(decoded.title, "Ada"));
	strcpy(legacy_id, decoded.id);
	assert(!bepio_pager_message_encode(&decoded, json, sizeof(json)));
	assert(!bepio_pager_message_parse((const unsigned char *)json, strlen(json), &decoded));
	assert(decoded.is_notification && !strcmp(decoded.id, legacy_id));
	root = cJSON_Parse(notification);
	cJSON_ReplaceItemInObjectCaseSensitive(root, "Type", cJSON_CreateString("message"));
	assert(parse(root)); /* App-specific fields cannot leak into ordinary messages. */
	cJSON_ReplaceItemInObjectCaseSensitive(root, "Type", cJSON_CreateString("notification"));
	cJSON_DeleteItemFromObjectCaseSensitive(root, "App");
	assert(parse(root));
	cJSON_AddStringToObject(root, "App", "Mail");
	char *variant = cJSON_PrintUnformatted(root);
	assert(!bepio_pager_message_parse((const unsigned char *)variant, strlen(variant), &decoded));
	assert(strcmp(decoded.id, legacy_id));
	cJSON_free(variant);
	cJSON_Delete(root);
	message.has_location = true;
	message.latitude = 30.2;
	message.longitude = 120.1;
	strcpy(message.address, "Hangzhou");
	assert(!bepio_pager_message_encode(&message, json, sizeof(json)));
	assert(!bepio_pager_message_parse((const unsigned char *)json, strlen(json), &decoded));
	assert(decoded.has_location && decoded.latitude == 30.2 && decoded.longitude == 120.1);
	assert(!strcmp(decoded.address, "Hangzhou"));
	message.latitude = 91;
	assert(bepio_pager_message_encode(&message, json, sizeof(json)));
	assert(!bepio_pager_message_stamp_new(&message));
	assert(strcmp(first_id, message.id));
	puts("PASS: versioned UUID protocol, independent IDs, Location/defaults, deterministic missing-ID fallback, inbox dedup and invalid schema rejection");
	return 0;
}
