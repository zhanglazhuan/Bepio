#ifndef BEPIO_CONTACTS_EDIT_H_
#define BEPIO_CONTACTS_EDIT_H_
#include "contacts_pages_internal.h"
/* Called on the LVGL owner thread. */
lv_obj_t *bepio_contacts_edit_build(void *app, void *payload);
int bepio_contacts_edit_page_sync(bepio_contacts_page_context_t *ctx);
#endif
