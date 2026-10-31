#ifndef BEPIO_MESSAGES_NAVIGATION_H_
#define BEPIO_MESSAGES_NAVIGATION_H_
/* Call on the UI thread after Messages has started. Opens an unsent compose view. */
int bepio_messages_compose(const char *phone, const char *name);
#endif
