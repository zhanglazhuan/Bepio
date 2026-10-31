#ifndef BEPIO_CONTACTS_APP_H_
#define BEPIO_CONTACTS_APP_H_

#include "controller.h"

#define BEPIO_CONTACTS_APP_NAME "Contacts"

typedef struct {
	contacts_model_t model;
	contacts_view_t view;
	contacts_controller_t controller;
} bepio_contacts_app_t;

contacts_controller_t *contacts_app_get_controller(void);
int contacts_app_close_request(void);

int bepio_contacts_register(void);
int bepio_contacts_console_register(void);

#endif
