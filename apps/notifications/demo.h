#ifndef BEPIO_NOTIFICATIONS_DEMO_H_
#define BEPIO_NOTIFICATIONS_DEMO_H_

#include "model.h"

/* Development fixtures use the same bounded receive path as live notifications. */
static inline unsigned bepio_notifications_demo_seed(void)
{
	static const bepio_notification_payload_t examples[] = {
		{.id = "demo-mail", .from = "101", .app = "Mail",
		 .title = "Weekly update", .text = "Your project summary is ready to read.",
		 .datetime = "2026-11-02T08:10:00Z"},
		{.id = "demo-calendar", .from = "102", .app = "Calendar",
		 .title = "Team meeting", .text = "Starts at 3:00 PM. Bring your notes.",
		 .datetime = "2026-11-02T08:20:00Z"},
		{.id = "demo-wechat", .from = "103", .app = "WeChat",
		 .title = "Alex", .text = "See you at the cafe in ten minutes!",
		 .datetime = "2026-11-02T08:30:00Z"},
		{.id = "demo-system", .from = "100", .app = "System",
		 .title = "Storage reminder", .text = "You can choose automatic message cleanup in Settings > Storage.",
		 .datetime = "2026-11-02T08:40:00Z"},
	};
	unsigned added = 0;
	for (size_t i = 0; i < sizeof(examples) / sizeof(examples[0]); i++) {
		if (!bepio_notifications_receive(&examples[i])) added++;
	}
	return added;
}

#endif
