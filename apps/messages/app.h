#ifndef BEPIO_MESSAGES_APP_H_
#define BEPIO_MESSAGES_APP_H_
#include "controller.h"
#define BEPIO_MESSAGES_APP_NAME "Messages"
typedef struct {
	messages_model_t model;
	messages_view_t view;
	messages_controller_t controller;
} bepio_messages_app_t;
messages_controller_t *messages_app_get_controller(void);
int messages_app_close_request(void);
int bepio_messages_register(void);
int bepio_messages_console_register(void);

#endif
