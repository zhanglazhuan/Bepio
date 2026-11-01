#ifndef BEPIO_CONTACTS_DISCARD_CONFIRM_H_
#define BEPIO_CONTACTS_DISCARD_CONFIRM_H_
#include "contacts_pages_internal.h"
/* Called on the LVGL owner thread. */
lv_obj_t *bepio_contacts_discard_confirm_build(void *app, void *payload);
#endif
