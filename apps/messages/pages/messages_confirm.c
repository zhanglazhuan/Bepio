#include "messages_confirm.h"
#include "messages_page_context.h"
#include "messages_page_common.h"

lv_obj_t *messages_confirm_build(void *app, void *payload)
{
	(void)payload;
	messages_view_t *view = app;
	Page page = lv_page_create("Messages", true, messages_page_back, view);
	if (!page.screen || !page.container) { return NULL; }
	messages_page_context_t *ctx = messages_page_alloc(sizeof(*ctx), view);
	if (!ctx) { return messages_page_finish(view, page, NULL); }
	messages_page_prepare(ctx, page, "Messages");
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
	return messages_page_finish(view, page, ctx);
}

