#include "messages_confirm.h"
#include <stdlib.h>
#include "messages_page_context.h"
#include "messages_page_common.h"

static void messages_confirm_page_render(messages_page_context_t *ctx)
{
	lv_obj_t *root = ctx->root;
	messages_model_t *model = ctx->controller->model;
	messages_pending_t pending = model->pending;
	messages_page_text(root, pending == MESSAGES_PENDING_DELETE ? "DELETE MESSAGE?" :
		pending == MESSAGES_PENDING_LEAVE ? "UNSAVED MESSAGE" :
		"REVIEW MESSAGE", 0x73d7b6);
	if (pending == MESSAGES_PENDING_LEAVE) {
		messages_page_button(ctx, root, "Save draft", ACT_SAVE, 0x346bd3);
		messages_page_button(ctx, root, "Discard changes", ACT_DISCARD, 0x8d3e4a);
	} else {
		bepio_message_payload_t payload = model->editor;
		bepio_message_t message;
		if (pending == MESSAGES_PENDING_DELETE &&
		    !bepio_messaging_get(model->pending_box, model->pending_id, false, &message)) {
			payload = message.payload;
		}
		messages_page_text(root, payload.phone, 0xa8bad2);
		messages_page_text(root, payload.body, 0xe9eff8);
		messages_page_button(ctx, root, pending == MESSAGES_PENDING_DELETE ?
			"Confirm delete" : "Confirm queue", ACT_CONFIRM, 0x346bd3);
	}
	messages_page_button(ctx, root, "Cancel", ACT_CANCEL, 0x263c5c);
}

lv_obj_t *messages_confirm_build(void *app, void *payload)
{
	(void)payload;
	messages_view_t *view = app;
	messages_page_context_t *ctx = messages_page_alloc(sizeof(messages_page_context_t), view);
	if (!ctx) { return NULL; }
	lv_obj_t *screen = messages_page_begin(ctx, "Messages");
	if (!screen) { free(ctx); return NULL; }
	messages_confirm_page_render(ctx);
	return messages_page_finish(ctx);
}

