#ifndef BEPIO_CONTACTS_PAGES_INTERNAL_H_
#define BEPIO_CONTACTS_PAGES_INTERNAL_H_
/* Private Contacts rendering state; access only on the LVGL owner thread. */
#include "../view.h"
#include "../app.h"
#include "lvgl.h"
#include "lv_page.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define BG 0xf5f7f9
#define INK 0x1b2938
#define MUTED 0x738295
#define GREEN 0x24785e
#define RED 0xc14545
#define PAGE_SIZE 12

typedef enum { ACT_NEW, ACT_EDIT, ACT_SAVE, ACT_BACK, ACT_DELETE,
	ACT_DELETE_YES, ACT_DISCARD, ACT_NEXT, ACT_PREVIOUS, ACT_MESSAGE, ACT_FAVORITE } action_t;
typedef struct {
	contacts_view_t *view;
	const contacts_model_t *model;
	lv_obj_t *root, *list, *keyboard, *name, *phone, *error;
	bool rebuilding;
	size_t offset;
} bepio_contacts_page_context_t;
extern bepio_contacts_page_context_t bepio_contacts_ui;


#endif
