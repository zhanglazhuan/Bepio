#ifndef BEPIO_MESSAGES_CONTROLLER_H_
#define BEPIO_MESSAGES_CONTROLLER_H_
#include "model.h"
#include "view.h"

typedef struct {
	messages_model_t *model;
	messages_view_t *view;
	bool active;
} messages_controller_t;
typedef struct {
	messages_model_t model;
	messages_view_t view;
} messages_snapshot_t;

void messages_controller_init(messages_controller_t *controller, messages_model_t *model,
			      messages_view_t *view);
void messages_controller_deinit(messages_controller_t *controller);
bool messages_controller_back(messages_controller_t *controller);
void messages_controller_set_ui_available(messages_controller_t *controller, bool available);
int messages_controller_snapshot(messages_controller_t *controller, messages_snapshot_t *snapshot);
int messages_controller_home(messages_controller_t *controller);
int messages_controller_box(messages_controller_t *controller, bepio_message_box_t box);
int messages_controller_show(messages_controller_t *controller, bepio_message_box_t box,
			     uint32_t id, bepio_message_t *message);
int messages_controller_new(messages_controller_t *controller, const char *phone, const char *name);
int messages_controller_recipient(messages_controller_t *controller, const char *phone,
				  const char *name);
int messages_controller_contact(messages_controller_t *controller, uint32_t contact_id);
int messages_controller_reply(messages_controller_t *controller, uint32_t inbox_id);
int messages_controller_edit(messages_controller_t *controller, uint32_t draft_id);
int messages_controller_text(messages_controller_t *controller, const char *text);
int messages_controller_save(messages_controller_t *controller, uint32_t *id);
int messages_controller_discard(messages_controller_t *controller);
int messages_controller_send(messages_controller_t *controller);
int messages_controller_delete(messages_controller_t *controller, bepio_message_box_t box,
			       uint32_t id);
int messages_controller_confirm(messages_controller_t *controller, uint32_t *id);
int messages_controller_cancel(messages_controller_t *controller);
int messages_controller_retry(messages_controller_t *controller, uint32_t id);
#endif
