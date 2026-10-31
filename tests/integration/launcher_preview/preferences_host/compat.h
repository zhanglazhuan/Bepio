#ifndef BEPIO_PREFERENCES_TEST_COMPAT_H_
#define BEPIO_PREFERENCES_TEST_COMPAT_H_
#include <time.h>
int bepio_test_setenv(const char *name, const char *value, int overwrite);
struct tm *bepio_test_localtime_r(const time_t *time, struct tm *result);
#endif
