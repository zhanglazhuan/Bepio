/* Single-threaded host tests; scheduling is covered by the ESP-IDF port. */
#ifndef HOST_BEPIO_MUTEX_H_
#define HOST_BEPIO_MUTEX_H_
#include <assert.h>
#define BEPIO_MUTEX_DEFINE(name) static unsigned name
#define bepio_mutex_lock(name) ((void)++(name))
#define bepio_mutex_unlock(name) do { assert((name) > 0); --(name); } while (0)
#endif
