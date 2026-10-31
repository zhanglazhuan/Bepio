#ifndef BEPIO_SETTINGS_VIEW_H_
#define BEPIO_SETTINGS_VIEW_H_

#include <stdbool.h>
#include "model.h"
#include "bepio_wifi.h"

typedef enum {
	SETTINGS_PAGE_HOME,
	SETTINGS_PAGE_ABOUT,
	SETTINGS_PAGE_ID,
	SETTINGS_PAGE_DATETIME,
	SETTINGS_PAGE_UPDATES,
	SETTINGS_PAGE_BATTERY,
	SETTINGS_PAGE_WIFI,
	SETTINGS_PAGE_WIFI_CONNECT,
	SETTINGS_PAGE_USAGE,
	SETTINGS_PAGE_STORAGE,
} settings_page_t;

typedef struct {
	settings_page_t page;
	bepio_wifi_ap_t wifi_ap;
	bool available;
	void *root;
	void *group;
} settings_view_t;

void settings_view_init(settings_view_t *view, void *root, void *group);
void settings_view_deinit(settings_view_t *view);
void settings_view_render(const settings_view_t *view, const settings_model_t *model);

#endif
