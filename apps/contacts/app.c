#include "app.h"
#include "bepio_app_manager.h"
#include <errno.h>

static bepio_contacts_app_t contacts_app;

static int contacts_start(void *root, void *group)
{
	contacts_view_init(&contacts_app.view, root, group);
	contacts_controller_init(&contacts_app.controller, &contacts_app.model, &contacts_app.view);
	return 0;
}

static void contacts_stop(void)
{
	contacts_controller_deinit(&contacts_app.controller);
}

static bool contacts_back(void)
{
	return contacts_controller_back(&contacts_app.controller);
}

static void contacts_ui_unavailable(void)
{
	contacts_controller_set_ui_available(&contacts_app.controller, false);
}

static void contacts_ui_available(void)
{
	contacts_controller_set_ui_available(&contacts_app.controller, true);
}

static bepio_application_t application = {
	.name = BEPIO_CONTACTS_APP_NAME,
	.category = BEPIO_APP_CATEGORY_TOOLS,
	.start_func = contacts_start,
	.stop_func = contacts_stop,
	.back_func = contacts_back,
	.ui_unavailable_func = contacts_ui_unavailable,
	.ui_available_func = contacts_ui_available,
};

contacts_controller_t *contacts_app_get_controller(void)
{
	return &contacts_app.controller;
}

int contacts_app_close_request(void)
{
	return bepio_app_manager_app_close_request(&application);
}

int bepio_contacts_register(void)
{
	contacts_model_init(&contacts_app.model);
	int ret = bepio_contacts_model_restore(&contacts_app.model);
	if (ret && ret != -ENOENT) { return ret; }
	contacts_app.controller.model = &contacts_app.model;
	return bepio_app_manager_add_application(&application);
}


