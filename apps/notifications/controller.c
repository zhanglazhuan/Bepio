#include "controller.h"
#include <errno.h>
void bepio_notifications_controller_open(bepio_notifications_controller_t *controller)
{
	*controller = (bepio_notifications_controller_t){.active = true};
}
int bepio_notifications_controller_select(bepio_notifications_controller_t *controller, uint32_t id)
{
	if (!controller || !controller->active) return -EACCES;
	bepio_notification_t item;
	int ret = bepio_notifications_get(id, true, &item);
	if (!ret) controller->selected = id;
	return ret;
}
int bepio_notifications_controller_mark_read(bepio_notifications_controller_t *controller, uint32_t id)
{
	if (!controller || !controller->active) return -EACCES;
	bepio_notification_t item;
	return bepio_notifications_get(id, true, &item);
}
int bepio_notifications_controller_delete(bepio_notifications_controller_t *controller, uint32_t id)
{
	if (!controller || !controller->active) return -EACCES;
	int ret = bepio_notifications_delete(id);
	if (!ret && controller->selected == id) controller->selected = 0;
	return ret;
}
bool bepio_notifications_controller_back(bepio_notifications_controller_t *controller)
{
	if (!controller || !controller->active || !controller->selected) return false;
	controller->selected = 0;
	return true;
}
