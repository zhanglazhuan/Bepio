#ifndef BEPIO_MESSAGE_FILE_H_
#define BEPIO_MESSAGE_FILE_H_
#include <stddef.h>

/* Versioned text snapshot in the storage SPIFFS partition. */
int bepio_message_file_mount(void);
int bepio_message_file_read(void *data, size_t size);
int bepio_message_file_write(const void *data, size_t size);

#endif
