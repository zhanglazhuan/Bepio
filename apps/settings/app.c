#include "app.h"
#include "bepio_app_manager.h"

static bepio_settings_app_t settings_app;

static int settings_start(void *root, void *group)
{
	settings_view_init(&settings_app.view, root, group);
	settings_controller_init(&settings_app.controller, &settings_app.model,
				 &settings_app.view);
	return 0;
}

static void settings_stop(void)
{
	settings_controller_deinit(&settings_app.controller);
}

static bool settings_back(void)
{
	return settings_controller_back(&settings_app.controller);
}

static bool settings_factory_reset(void)
{
	settings_controller_reset(&settings_app.controller);
	return true;
}

static void settings_ui_unavailable(void)
{
	settings_controller_set_ui_available(&settings_app.controller, false);
}

static void settings_ui_available(void)
{
	settings_controller_set_ui_available(&settings_app.controller, true);
}

static bepio_application_t application = {
	.name = BEPIO_SETTINGS_APP_NAME,
	.category = BEPIO_APP_CATEGORY_SYSTEM,
	.start_func = settings_start,
	.stop_func = settings_stop,
	.back_func = settings_back,
	.factory_reset_func = settings_factory_reset,
	.ui_unavailable_func = settings_ui_unavailable,
	.ui_available_func = settings_ui_available,
};

settings_controller_t *settings_app_get_controller(void)
{
	return &settings_app.controller;
}

int settings_app_close_request(void)
{
	return bepio_app_manager_app_close_request(&application);
}

int bepio_settings_register(void)
{
	settings_model_reset(&settings_app.model);
	settings_app.controller.model = &settings_app.model;
	return bepio_app_manager_add_application(&application);
}


