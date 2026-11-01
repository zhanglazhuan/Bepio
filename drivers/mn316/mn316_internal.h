#ifndef BEPIO_MN316_INTERNAL_H_
#define BEPIO_MN316_INTERNAL_H_

#include "mn316.h"

/* For compound modem AT transactions. The caller must hold the driver lock
 * across all locked commands. No UART consumer may bypass this lock.
 */
int bepio_mn316_driver_acquire(void);
void bepio_mn316_driver_release(void);
int bepio_mn316_command_locked(const char *command, const char *completion,
	char *reply, size_t capacity, int timeout_ms);

#endif
