#ifndef BEPIO_CHAT_CONTROLLER_H_
#define BEPIO_CHAT_CONTROLLER_H_

#include "model.h"

typedef struct {
	bepio_chat_model_t *model;
	bool active;
} bepio_chat_controller_t;

void bepio_chat_controller_open(bepio_chat_controller_t *controller, bepio_chat_model_t *model);
void bepio_chat_controller_close(bepio_chat_controller_t *controller);
int bepio_chat_controller_select(bepio_chat_controller_t *controller, size_t index);
int bepio_chat_controller_contact_at(const bepio_chat_controller_t *controller, size_t index,
				bepio_chat_contact_t *contact);
int bepio_chat_controller_message_at(const bepio_chat_controller_t *controller, size_t index,
				bepio_chat_message_t *message);
size_t bepio_chat_controller_selected(const bepio_chat_controller_t *controller);
int bepio_chat_controller_play(bepio_chat_controller_t *controller, size_t index);

#endif
