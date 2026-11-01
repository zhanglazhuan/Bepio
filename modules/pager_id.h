#ifndef BEPIO_PAGER_ID_H_
#define BEPIO_PAGER_ID_H_

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* Device IDs and delivery addresses contain decimal digits only. */
static inline bool bepio_pager_id_valid(const char *id, size_t max)
{
	if (!id) return false;
	size_t n = 0;
	for (; n <= max && id[n]; n++) {
		if (id[n] < '0' || id[n] > '9') return false;
	}
	return n > 0 && n <= max;
}

#endif
