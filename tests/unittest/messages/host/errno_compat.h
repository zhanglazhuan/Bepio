/* MinGW omits this POSIX errno used by the messaging service. Host tests only. */
#include <errno.h>
#ifndef ESTALE
#define ESTALE 2001
#endif
