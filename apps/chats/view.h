#ifndef BEPIO_CHATS_VIEW_H_
#define BEPIO_CHATS_VIEW_H_

#include "controller.h"
#include "page_navigator.h"

typedef struct {
	page_navigator_t page_nav;
	page_navigator_page_t page_registry[1];
	bepio_chats_controller_t *controller;
} bepio_chats_view_t;

int bepio_chats_view_open(void *root, void *group, bepio_chats_controller_t *controller);
void bepio_chats_view_close(void);

#endif
