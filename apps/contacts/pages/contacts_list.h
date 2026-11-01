#ifndef BEPIO_CONTACTS_LIST_H_
#define BEPIO_CONTACTS_LIST_H_
#include "contacts_pages_internal.h"
/* Called on the LVGL owner thread. */
lv_obj_t *bepio_contacts_list_build(void *app, void *payload);
void bepio_contacts_list_page_advance(bepio_contacts_page_context_t *ctx, int pages);
#endif
