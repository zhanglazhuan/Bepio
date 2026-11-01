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
	ACT_NEXT, ACT_PREVIOUS, ACT_MESSAGE, ACT_FAVORITE } action_t;
struct bepio_contacts_page_context {
	contacts_view_t *view;
	contacts_model_t *model;
	lv_obj_t *root, *error;
	bool rebuilding;
};

bepio_contacts_page_context_t *bepio_contacts_page_alloc(size_t size,
	contacts_view_t *view, contacts_model_t *model);
lv_obj_t *bepio_contacts_page_begin(bepio_contacts_page_context_t *ctx,
	const char *title);
lv_obj_t *bepio_contacts_page_finish(bepio_contacts_page_context_t *ctx);


#endif
