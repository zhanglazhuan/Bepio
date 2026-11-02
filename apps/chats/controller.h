#ifndef BEPIO_CHATS_CONTROLLER_H_
#define BEPIO_CHATS_CONTROLLER_H_

#include "model.h"

/* Provider owns microphone capture and transient audio. Calls must return
 * promptly on the UI thread. stop writes a terminated UTF-8 transcript. */
typedef struct {
	int (*start)(void *context);
	int (*stop)(void *context, char *text, size_t capacity);
	void (*cancel)(void *context);
} bepio_chats_voice_input_t;

typedef struct {
	bepio_chats_model_t *model;
	const bepio_chats_voice_input_t *voice_input;
	void *voice_context;
	bool active;
	bool recording;
	size_t recording_contact;
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
int bepio_chats_controller_delete_message(bepio_chats_controller_t *controller, size_t index);
int bepio_chats_controller_set_voice_input(bepio_chats_controller_t *controller,
	const bepio_chats_voice_input_t *input, void *context);
int bepio_chats_controller_record_start(bepio_chats_controller_t *controller);
int bepio_chats_controller_record_stop(bepio_chats_controller_t *controller, uint32_t *id);
void bepio_chats_controller_record_cancel(bepio_chats_controller_t *controller);

#endif
