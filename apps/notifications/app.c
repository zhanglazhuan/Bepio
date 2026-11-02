#include "app.h"
#include "controller.h"
#include "view.h"
#include "bepio_app_manager.h"
#include "system/uilv/imgs/icon_40x40/bepio_icons.h"
static bepio_notifications_controller_t controller;
static int start(void *root, void *group)
{
	(void)group;
	bepio_notifications_controller_open(&controller);
	return bepio_notifications_view_open(root, &controller);
}
static void stop(void)
{
	bepio_notifications_view_close();
	controller.active = false;
	controller.selected = 0;
}
static bool back(void)
{
	if (!bepio_notifications_controller_back(&controller)) return false;
	bepio_notifications_view_refresh();
	return true;
}
static bepio_application_t application = {
	.name = "Notifications", .category = BEPIO_APP_CATEGORY_TOOLS,
	.icon = &ic_notifications, .start_func = start, .stop_func = stop, .back_func = back,
};
int bepio_notifications_register(void)
{
	return bepio_app_manager_add_application(&application);
}
