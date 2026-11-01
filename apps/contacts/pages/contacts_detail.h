#ifndef BEPIO_CONTACTS_DETAIL_H_
#define BEPIO_CONTACTS_DETAIL_H_
#include "contacts_pages_internal.h"
/* Called on the LVGL owner thread. */
lv_obj_t *bepio_contacts_detail_build(void *app, void *payload);
int bepio_contacts_detail_show_delete_sheet(bepio_contacts_page_context_t *ctx);
#endif
