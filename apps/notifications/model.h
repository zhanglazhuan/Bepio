#ifndef BEPIO_NOTIFICATIONS_MODEL_H_
#define BEPIO_NOTIFICATIONS_MODEL_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BEPIO_NOTIFICATIONS_CAPACITY 16
/* Length limits are UTF-8 bytes, excluding NUL. */
typedef struct {
	char id[40];
	char from[65];
	char app[65];
	char title[97];
	char text[385];
	char datetime[26];
} bepio_notification_payload_t;
typedef struct {
	uint32_t id;
	bool read;
	bepio_notification_payload_t payload;
} bepio_notification_t;
/* Initialize once at boot, before producers; closing the app retains RAM data. */
void bepio_notifications_init(void);
/* Thread-safe upsert by From + App + remote Id; identical repeats are no-ops. */
int bepio_notifications_receive(const bepio_notification_payload_t *payload);
int bepio_notifications_get_at(size_t index, bepio_notification_t *item);
int bepio_notifications_get(uint32_t id, bool mark_read, bepio_notification_t *item);
int bepio_notifications_delete(uint32_t id);
uint32_t bepio_notifications_revision(void);
#endif
