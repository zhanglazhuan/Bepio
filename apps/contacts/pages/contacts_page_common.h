#ifndef BEPIO_CONTACTS_PAGE_COMMON_H_
#define BEPIO_CONTACTS_PAGE_COMMON_H_
#include "contacts_pages_internal.h"
bool bepio_contacts_page_wide(void);
lv_obj_t *bepio_contacts_page_text(lv_obj_t *parent, const char *value, int x, int y, int width, uint32_t color);
lv_obj_t *bepio_contacts_page_button(lv_obj_t *parent, const char *value, int x, int y, int width, int height, uint32_t color, action_t id);
void bepio_contacts_page_action(lv_event_t *event);
void bepio_contacts_page_report(int error);
int bepio_contacts_page_sync_draft(void);
char bepio_contacts_page_initial(const char *name);
#endif
