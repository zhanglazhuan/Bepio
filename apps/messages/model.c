#include "model.h"
#include <errno.h>
#include <string.h>

int messages_model_compose(messages_model_t *model, const bepio_message_payload_t *payload,
			   uint32_t draft_id, bool dirty)
{
	int ret;

	if (model->editing) {
		return -EBUSY;
	}
	ret = bepio_messaging_validate(payload, false);
	if (ret < 0) {
		return ret;
	}
	model->editor = *payload;
	model->draft_id = draft_id;
	model->editing = true;
	model->dirty = dirty;
	return 0;
}

int messages_model_text(messages_model_t *model, const char *text)
{
	bepio_message_payload_t payload = model->editor;
	size_t length = 0;
	int ret;

	if (!model->editing || !text) {
		return -EINVAL;
	}
	while (length <= BEPIO_MESSAGE_BODY_MAX && text[length]) {
		length++;
	}
	if (length > BEPIO_MESSAGE_BODY_MAX) {
		return -ENAMETOOLONG;
	}
	memset(payload.body, 0, sizeof(payload.body));
	memcpy(payload.body, text, length);
	ret = bepio_messaging_validate(&payload, false);
	if (ret < 0) {
		return ret;
	}
	if (strcmp(payload.body, model->editor.body)) {
		model->dirty = true;
	}
	model->editor = payload;
	return 0;
}

void messages_model_discard(messages_model_t *model)
{
	model->editing = model->dirty = false;
	model->draft_id = 0;
	memset(&model->editor, 0, sizeof(model->editor));
	model->pending = MESSAGES_PENDING_NONE;
}
