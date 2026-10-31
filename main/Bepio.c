#include "sdkconfig.h"
#include "esp_err.h"
#include "bepio_log.h"
#include "bepio_console.h"
#include "bepio_system_manager.h"
#include "bepio_app_manager.h"
#include "bepio_ui.h"
#include "bepio_account.h"
#include "bepio_launcher.h"
#ifdef CONFIG_BEPIO_APP_SETTINGS
#include "apps/settings/app.h"
#endif
#ifdef CONFIG_BEPIO_APP_CONTACTS
#include "apps/contacts/app.h"
#endif
#ifdef CONFIG_BEPIO_APP_MESSAGES
#include "apps/messages/app.h"
#ifdef CONFIG_BEPIO_MESSAGES_DEMO
#include "apps/messages/demo.h"
#include "esp_timer.h"
#endif
#endif
#ifdef CONFIG_BEPIO_APP_CHAT
#include "apps/chat/app.h"
#endif
BEPIO_LOG_TAG(bepio);

void app_main(void)
{
	BEPIO_LOGI("Bepio starting on ESP-IDF");
	ESP_ERROR_CHECK(bepio_account_init());
#ifdef CONFIG_BEPIO_APP_MESSAGES
	ESP_ERROR_CHECK(bepio_messaging_init());
#ifdef CONFIG_BEPIO_MESSAGES_DEMO
	int64_t demo_started = esp_timer_get_time();
	unsigned fixtures = bepio_messages_demo_seed();
	bepio_messaging_stats_t demo_stats;
	bepio_messaging_stats(&demo_stats);
	BEPIO_LOGI("Demo fixtures: %u/5; mailbox: %u inbox, %u outbox; seed took %lld ms",
		fixtures, (unsigned)demo_stats.inbox, (unsigned)demo_stats.outbox,
		(long long)((esp_timer_get_time() - demo_started) / 1000));
#endif
#endif
#ifdef CONFIG_BEPIO_APP_CONTACTS
	ESP_ERROR_CHECK(bepio_contacts_register());
#endif
#ifdef CONFIG_BEPIO_APP_MESSAGES
	ESP_ERROR_CHECK(bepio_messages_register());
#endif
#ifdef CONFIG_BEPIO_APP_CHAT
	ESP_ERROR_CHECK(bepio_chat_register());
#endif
#ifdef CONFIG_BEPIO_APP_SETTINGS
	ESP_ERROR_CHECK(bepio_settings_register());
#endif
	ESP_ERROR_CHECK(bepio_ui_start(bepio_launcher_home));
	int ret = bepio_system_init();
	if (ret < 0) {
		BEPIO_LOGW("Modem unavailable (%d); continuing without connectivity", ret);
	}
#ifdef CONFIG_BEPIO_CONSOLE
	ESP_ERROR_CHECK(bepio_console_init());
#ifdef CONFIG_BEPIO_SETTINGS_SHELL
	ESP_ERROR_CHECK(bepio_settings_console_register());
#endif
#ifdef CONFIG_BEPIO_CONTACTS_SHELL
	ESP_ERROR_CHECK(bepio_contacts_console_register());
#endif
#ifdef CONFIG_BEPIO_MESSAGES_SHELL
	ESP_ERROR_CHECK(bepio_messages_console_register());
#endif
	ESP_ERROR_CHECK(bepio_console_start());
#endif
}
