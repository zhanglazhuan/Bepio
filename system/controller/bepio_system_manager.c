#include "bepio_system_manager.h"
#include "bepio_log.h"
#ifdef CONFIG_BEPIO_MN316
#include "bepio_mn316_manager.h"
#endif

BEPIO_LOG_TAG(bepio_system);

int bepio_system_init(void)
{
#ifdef CONFIG_BEPIO_MN316
	int ret = bepio_mn316_manager_init();

	if (ret == 0) {
		BEPIO_LOGI("MN316 answered AT");
	}
	return ret;
#else
	return 0;
#endif
}
