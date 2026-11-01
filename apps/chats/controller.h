#ifndef BEPIO_CHATS_CONTROLLER_H_
#define BEPIO_CHATS_CONTROLLER_H_

#include "model.h"

typedef struct {
	bepio_chats_model_t *model;
	bool active;
} bepio_chats_controller_t;

void bepio_chats_controller_open(bepio_chats_controller_t *controller, bepio_chats_model_t *model);
void bepio_chats_controller_close(bepio_chats_controller_t *controller);
int bepio_chats_controller_select(bepio_chats_controller_t *controller, size_t index);
int bepio_chats_controller_contact_at(const bepio_chats_controller_t *controller, size_t index,
				bepio_chats_contact_t *contact);
int bepio_chats_controller_message_at(const bepio_chats_controller_t *controller, size_t index,
				bepio_chats_message_t *message);
size_t bepio_chats_controller_selected(const bepio_chats_controller_t *controller);
int bepio_chats_controller_play(bepio_chats_controller_t *controller, size_t index);

#endif
