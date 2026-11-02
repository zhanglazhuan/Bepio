#ifndef BEPIO_NOTIFICATIONS_CONTROLLER_H_
#define BEPIO_NOTIFICATIONS_CONTROLLER_H_
#include "model.h"
typedef struct {
	bool active;
	uint32_t selected;
} bepio_notifications_controller_t;
void bepio_notifications_controller_open(bepio_notifications_controller_t *controller);
int bepio_notifications_controller_select(bepio_notifications_controller_t *controller, uint32_t id);
int bepio_notifications_controller_mark_read(bepio_notifications_controller_t *controller, uint32_t id);
int bepio_notifications_controller_delete(bepio_notifications_controller_t *controller, uint32_t id);
bool bepio_notifications_controller_back(bepio_notifications_controller_t *controller);
#endif
