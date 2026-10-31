#ifndef HOST_BEPIO_LOG_H_
#define HOST_BEPIO_LOG_H_
#include <stdio.h>
#define BEPIO_LOG_TAG(name)
#define BEPIO_LOGI(...) do { if (0) { printf(__VA_ARGS__); } } while (0)
#endif
