#ifndef BEPIO_SETTINGS_APP_H_
#define BEPIO_SETTINGS_APP_H_

#include "model.h"
#include "view.h"
#include "controller.h"

#define BEPIO_SETTINGS_APP_NAME "Settings"

typedef struct {
	settings_model_t model;
	settings_view_t view;
	settings_controller_t controller;
} bepio_settings_app_t;

/* Rejects closure if another app owns the manager. */
int settings_app_close_request(void);

int bepio_settings_register(void);
int bepio_settings_console_register(void);

#endif
