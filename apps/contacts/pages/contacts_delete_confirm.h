#ifndef BEPIO_CONTACTS_DELETE_CONFIRM_H_
#define BEPIO_CONTACTS_DELETE_CONFIRM_H_
#include "contacts_pages_internal.h"
/* Called on the LVGL owner thread. */
lv_obj_t *bepio_contacts_delete_confirm_build(void *app, void *payload);
#endif
