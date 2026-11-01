#pragma once
#include <time.h>
#ifdef _WIN32
static inline struct tm *bepio_host_gmtime_r(const time_t *clock, struct tm *result)
{
	return gmtime_s(result, clock) ? NULL : result;
}
#define gmtime_r bepio_host_gmtime_r
#endif
