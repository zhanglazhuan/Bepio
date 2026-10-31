#ifndef BEPIO_SETTINGS_CONTROLLER_H_
#define BEPIO_SETTINGS_CONTROLLER_H_

#include <stdbool.h>
#include "model.h"
#include "view.h"

typedef struct {
	settings_model_t *model;
	settings_view_t *view;
	bool active;
} settings_controller_t;

typedef struct {
	settings_model_t preferences;
	settings_page_t page;
	bool available;
} settings_snapshot_t;

void settings_controller_init(settings_controller_t *controller,
			      settings_model_t *model, settings_view_t *view);
void settings_controller_deinit(settings_controller_t *controller);
bool settings_controller_back(settings_controller_t *controller);
void settings_controller_set_ui_available(settings_controller_t *controller, bool available);
int settings_controller_snapshot(settings_controller_t *controller, settings_snapshot_t *snapshot);
int settings_controller_set(settings_controller_t *controller, const char *key, bool value);
int bepio_settings_controller_wifi_select(settings_controller_t *controller, const bepio_wifi_ap_t *ap);
int bepio_settings_controller_page(settings_controller_t *controller, settings_page_t page);
int bepio_settings_controller_timezone(settings_controller_t *controller, unsigned timezone);
int settings_controller_about(settings_controller_t *controller);
void settings_controller_reset(settings_controller_t *controller);

/* The Settings app exposes its controller for the serial shell adapter. */
settings_controller_t *settings_app_get_controller(void);

#endif
