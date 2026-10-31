#ifndef BEPIO_MUTEX_H_
#define BEPIO_MUTEX_H_
#include <pthread.h>
#include <stdlib.h>

/* Preserve recursive callback reentry; pthread_once makes first use race safe. */
static inline void bepio_mutex_check(int result)
{
	if (result != 0) { abort(); }
}
#define BEPIO_MUTEX_DEFINE(name) \
	static pthread_mutex_t name; \
	static pthread_once_t name##_once = PTHREAD_ONCE_INIT; \
	static void name##_init(void) \
	{ \
		pthread_mutexattr_t attr; \
		bepio_mutex_check(pthread_mutexattr_init(&attr)); \
		bepio_mutex_check(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE)); \
		bepio_mutex_check(pthread_mutex_init(&name, &attr)); \
		bepio_mutex_check(pthread_mutexattr_destroy(&attr)); \
	}
#define bepio_mutex_lock(name) do { \
	bepio_mutex_check(pthread_once(&name##_once, name##_init)); \
	bepio_mutex_check(pthread_mutex_lock(&name)); \
} while (0)
#define bepio_mutex_unlock(name) bepio_mutex_check(pthread_mutex_unlock(&name))
#endif
