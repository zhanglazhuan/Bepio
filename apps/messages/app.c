#include "app.h"
#include "navigation.h"
#include "bepio_app_manager.h"
static bepio_messages_app_t messages_app;

static int messages_start(void *root, void *group)
{
	messages_view_init(&messages_app.view, root, group, messages_app.model.editing);
	messages_controller_init(&messages_app.controller, &messages_app.model, &messages_app.view);
	return 0;
}
static void messages_stop(void)
{
	messages_controller_deinit(&messages_app.controller);
}
static bool messages_back(void)
{
	return messages_controller_back(&messages_app.controller);
}
static void messages_ui_unavailable(void)
{
	messages_controller_set_ui_available(&messages_app.controller, false);
}
static void messages_ui_available(void)
{
	messages_controller_set_ui_available(&messages_app.controller, true);
}
static bepio_application_t application = {
	.name = BEPIO_MESSAGES_APP_NAME,
	.category = BEPIO_APP_CATEGORY_TOOLS,
	.start_func = messages_start,
	.stop_func = messages_stop,
	.back_func = messages_back,
	.ui_unavailable_func = messages_ui_unavailable,
	.ui_available_func = messages_ui_available,
};
messages_controller_t *messages_app_get_controller(void)
{
	return &messages_app.controller;
}
int messages_app_close_request(void)
{
	return bepio_app_manager_app_close_request(&application);
}
int bepio_messages_register(void)
{
	return bepio_app_manager_add_application(&application);
}



int bepio_messages_compose(const char *phone, const char *name)
{
	return messages_controller_new(&messages_app.controller, phone, name);
}
