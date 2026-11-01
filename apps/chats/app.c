#include "app.h"
#include "view.h"
#include "model.h"
#include "controller.h"
#include "bepio_app_manager.h"
#include <errno.h>

static bepio_chats_model_t model;
static bepio_chats_controller_t controller;

static int chats_start(void *root, void *group)
{
	bepio_chats_controller_open(&controller, &model);
	if (bepio_chats_view_open(root, group, &controller)) {
		bepio_chats_controller_close(&controller);
		return -ENOMEM;
	}
	return 0;
}

static void chats_stop(void)
{
	bepio_chats_view_close();
	bepio_chats_controller_close(&controller);
}

static bepio_application_t application = {
	.name = "Chats",
	.category = BEPIO_APP_CATEGORY_TOOLS,
	.start_func = chats_start,
	.stop_func = chats_stop,
};

int bepio_chats_register(void)
{
	return bepio_app_manager_add_application(&application);
}
