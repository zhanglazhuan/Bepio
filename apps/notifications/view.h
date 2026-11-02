#ifndef BEPIO_NOTIFICATIONS_VIEW_H_
#define BEPIO_NOTIFICATIONS_VIEW_H_
#include "controller.h"
int bepio_notifications_view_open(void *root, bepio_notifications_controller_t *controller);
void bepio_notifications_view_close(void);
void bepio_notifications_view_refresh(void);
#endif
