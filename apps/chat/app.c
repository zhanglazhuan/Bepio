#include "app.h"
#include "view.h"
#include "model.h"
#include "controller.h"
#include "bepio_app_manager.h"

static bepio_chat_model_t model;
static bepio_chat_controller_t controller;

static int chat_start(void *root, void *group)
{
	(void)group;
	bepio_chat_controller_open(&controller, &model);
	bepio_chat_view_open(root, &controller);
	return 0;
}

static void chat_stop(void)
{
	bepio_chat_view_close();
	bepio_chat_controller_close(&controller);
}

static bepio_application_t application = {
	.name = "Chat",
	.category = BEPIO_APP_CATEGORY_TOOLS,
	.start_func = chat_start,
	.stop_func = chat_stop,
};

int bepio_chat_register(void)
{
	return bepio_app_manager_add_application(&application);
}
