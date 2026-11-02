#include "controller.h"
#include "bepio_log.h"
#include <errno.h>
#include <string.h>
#include "bepio_mutex.h"
#if defined(ESP_PLATFORM) && defined(CONFIG_BEPIO_MN316)
#include "bepio_mn316_manager.h"
#endif
#ifdef CONFIG_BEPIO_APP_CONTACTS
#include "apps/contacts/app.h"
#endif

BEPIO_MUTEX_DEFINE(messages_lock);
BEPIO_LOG_TAG(messages_view);

static void show_page(messages_controller_t *c)
{
	messages_view_t *view = c->view;
	if (!view->available) { return; }
	if (view->page_nav.registry) {
		page_navigator_navigate_to(&view->page_nav, view,
			c->model->flow.page + 1, NULL);
	}
	bepio_messaging_stats_t stats;
	bepio_messaging_stats(&stats);
	BEPIO_LOGI("Messages: page=%d inbox=%u unread=%u outbox=%u queued=%u",
		c->model->flow.page, (unsigned)stats.inbox, (unsigned)stats.unread,
		(unsigned)stats.outbox, (unsigned)stats.queued);
}

static messages_page_t box_page(bepio_message_box_t box)
{
	return box == BEPIO_MESSAGE_INBOX ? MESSAGES_PAGE_HOME : MESSAGES_PAGE_BOX;
}

static int ready(messages_controller_t *c, bool navigation)
{
	if (!c->active) {
		return -EACCES;
	}
	if (c->model->pending != MESSAGES_PENDING_NONE || (navigation && c->model->editing)) {
		return -EBUSY;
	}
	return 0;
}

void messages_controller_init(messages_controller_t *c, messages_model_t *model,
			      messages_view_t *view)
{
	bepio_mutex_lock(messages_lock);
	c->model = model;
	c->view = view;
	c->active = true;
	model->flow = (messages_flow_t){.page = model->editing ? MESSAGES_PAGE_COMPOSE : MESSAGES_PAGE_HOME,
		.box = BEPIO_MESSAGE_INBOX};
	view->controller = c;
	show_page(c);
	bepio_mutex_unlock(messages_lock);
}

void messages_controller_deinit(messages_controller_t *c)
{
	bepio_mutex_lock(messages_lock);
	c->active = false;
	c->model->pending = MESSAGES_PENDING_NONE;
	messages_view_deinit(c->view);
	bepio_mutex_unlock(messages_lock);
}

static void cancel_pending(messages_controller_t *c)
{
	c->model->flow.page = c->model->flow.return_page;
	c->model->pending = MESSAGES_PENDING_NONE;
}

bool messages_controller_back(messages_controller_t *c)
{
	bool consumed = true;

	bepio_mutex_lock(messages_lock);
	if (!c->active) {
		consumed = false;
	} else if (c->model->pending != MESSAGES_PENDING_NONE) {
		cancel_pending(c);
	} else if (c->model->flow.page == MESSAGES_PAGE_COMPOSE) {
		if (c->model->dirty && (c->model->draft_id || c->model->editor.phone[0] ||
		    c->model->editor.name[0] || c->model->editor.body[0])) {
			c->model->pending = MESSAGES_PENDING_LEAVE;
			c->model->flow.return_page = MESSAGES_PAGE_COMPOSE;
			c->model->flow.page = MESSAGES_PAGE_CONFIRM;
		} else {
			messages_model_discard(c->model);
			c->model->flow.page = MESSAGES_PAGE_HOME;
			c->model->flow.box = BEPIO_MESSAGE_INBOX;
		}
	} else if (c->model->flow.page == MESSAGES_PAGE_DETAIL) {
		c->model->flow.page = box_page(c->model->flow.box);
	} else if (c->model->flow.page == MESSAGES_PAGE_BOX) {
		c->model->flow.page = MESSAGES_PAGE_HOME;
		c->model->flow.box = BEPIO_MESSAGE_INBOX;
	} else {
		consumed = false;
	}
	if (consumed) {
		show_page(c);
	}
	bepio_mutex_unlock(messages_lock);
	return consumed;
}

void messages_controller_set_ui_available(messages_controller_t *c, bool available)
{
	bepio_mutex_lock(messages_lock);
	if (c->active) {
		c->view->available = available;
		show_page(c);
	}
	bepio_mutex_unlock(messages_lock);
}

int messages_controller_snapshot(messages_controller_t *c, messages_snapshot_t *snapshot)
{
	int ret = 0;

	if (!snapshot) {
		return -EINVAL;
	}
	bepio_mutex_lock(messages_lock);
	if (!c->active) {
		ret = -EACCES;
	} else {
		snapshot->model = *c->model;
		snapshot->view = *c->view;
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_home(messages_controller_t *c)
{
	int ret;

	bepio_mutex_lock(messages_lock);
	ret = ready(c, true);
	if (!ret) {
		c->model->flow.page = MESSAGES_PAGE_HOME;
		c->model->flow.box = BEPIO_MESSAGE_INBOX;
		show_page(c);
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_box(messages_controller_t *c, bepio_message_box_t box)
{
	int ret;

	if (box != BEPIO_MESSAGE_INBOX && box != BEPIO_MESSAGE_OUTBOX) {
		return -EINVAL;
	}
	bepio_mutex_lock(messages_lock);
	ret = ready(c, true);
	if (!ret) {
		c->model->flow.box = box;
		c->model->flow.page = box_page(box);
		show_page(c);
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_show(messages_controller_t *c, bepio_message_box_t box, uint32_t id,
			     bepio_message_t *message)
{
	int ret;

	bepio_mutex_lock(messages_lock);
	ret = ready(c, true);
	if (!ret) {
		ret = bepio_messaging_get(box, id, true, message);
	}
	if (!ret) {
		c->model->flow.page = MESSAGES_PAGE_DETAIL;
		c->model->flow.box = box;
		c->model->flow.selected_id = id;
		show_page(c);
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

static int begin_compose(messages_controller_t *c, const bepio_message_payload_t *payload,
			 uint32_t draft_id, bool dirty)
{
	int ret = ready(c, true);

	if (!ret) {
		ret = messages_model_compose(c->model, payload, draft_id, dirty);
	}
	if (!ret) {
		c->model->flow.page = MESSAGES_PAGE_COMPOSE;
		show_page(c);
	}
	return ret;
}

int messages_controller_new(messages_controller_t *c, const char *phone, const char *name)
{
	bepio_message_payload_t payload = {0};
	size_t phone_len = 0, name_len = 0;
	int ret;

	if (!phone || !name) {
		return -EINVAL;
	}
	while (phone_len <= BEPIO_MESSAGE_PHONE_MAX && phone[phone_len]) {
		phone_len++;
	}
	while (name_len <= BEPIO_MESSAGE_NAME_MAX && name[name_len]) {
		name_len++;
	}
	if (phone_len > BEPIO_MESSAGE_PHONE_MAX || name_len > BEPIO_MESSAGE_NAME_MAX) {
		return -ENAMETOOLONG;
	}
	memcpy(payload.phone, phone, phone_len);
	memcpy(payload.name, name, name_len);
	bepio_mutex_lock(messages_lock);
	ret = begin_compose(c, &payload, 0, true);
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_recipient(messages_controller_t *c, const char *phone, const char *name)
{
	bepio_message_payload_t payload;
	size_t phone_len = 0, name_len = 0;
	int ret;

	if (!phone || !name) {
		return -EINVAL;
	}
	while (phone_len <= BEPIO_MESSAGE_PHONE_MAX && phone[phone_len]) {
		phone_len++;
	}
	while (name_len <= BEPIO_MESSAGE_NAME_MAX && name[name_len]) {
		name_len++;
	}
	if (phone_len > BEPIO_MESSAGE_PHONE_MAX || name_len > BEPIO_MESSAGE_NAME_MAX) {
		return -ENAMETOOLONG;
	}
	bepio_mutex_lock(messages_lock);
	ret = ready(c, false);
	if (!ret && !c->model->editing) {
		ret = -EPERM;
	}
	if (!ret) {
		payload = c->model->editor;
		memset(payload.name, 0, sizeof(payload.name));
		memset(payload.phone, 0, sizeof(payload.phone));
		memcpy(payload.name, name, name_len);
		memcpy(payload.phone, phone, phone_len);
		ret = bepio_messaging_validate(&payload, false);
		if (!ret) {
			c->model->dirty |= strcmp(payload.name, c->model->editor.name) != 0 ||
					   strcmp(payload.phone, c->model->editor.phone) != 0;
			c->model->editor = payload;
			show_page(c);
		}
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_contact(messages_controller_t *c, uint32_t contact_id)
{
#ifdef CONFIG_BEPIO_APP_CONTACTS
	contacts_entry_t entry;
	int ret = contacts_controller_lookup(contacts_app_get_controller(), contact_id, &entry);

	if (ret < 0) {
		return ret;
	}
	bepio_mutex_lock(messages_lock);
	ret = ready(c, false);
	if (!ret) {
		ret = c->model->editing ? messages_controller_recipient(c, entry.phone, entry.name)
					: messages_controller_new(c, entry.phone, entry.name);
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
#else
	(void)c;
	(void)contact_id;
	return -ENOTSUP;
#endif
}

int messages_controller_reply(messages_controller_t *c, uint32_t inbox_id)
{
	bepio_message_t source;
	int ret;

	bepio_mutex_lock(messages_lock);
	ret = ready(c, true);
	if (!ret) {
		ret = bepio_messaging_get(BEPIO_MESSAGE_INBOX, inbox_id, false, &source);
	}
	if (!ret) {
		memset(source.payload.body, 0, sizeof(source.payload.body));
		ret = begin_compose(c, &source.payload, 0, true);
		if (!ret) {
			ret = bepio_messaging_get(BEPIO_MESSAGE_INBOX, inbox_id, true, &source);
		}
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_edit(messages_controller_t *c, uint32_t draft_id)
{
	bepio_message_t message;
	int ret;

	bepio_mutex_lock(messages_lock);
	ret = ready(c, true);
	if (!ret) {
		ret = bepio_messaging_get(BEPIO_MESSAGE_OUTBOX, draft_id, false, &message);
	}
	if (!ret && message.state != BEPIO_MESSAGE_DRAFT) {
		ret = -EPERM;
	}
	if (!ret) {
		ret = begin_compose(c, &message.payload, draft_id, false);
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_text(messages_controller_t *c, const char *text)
{
	int ret;

	bepio_mutex_lock(messages_lock);
	ret = ready(c, false);
	if (!ret) {
		ret = messages_model_text(c->model, text);
	}
	if (!ret) {
		show_page(c);
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_save(messages_controller_t *c, uint32_t *id)
{
	int ret = 0;

	bepio_mutex_lock(messages_lock);
	if (!c->active) {
		ret = -EACCES;
	} else if (!c->model->editing || (c->model->pending != MESSAGES_PENDING_NONE &&
					  c->model->pending != MESSAGES_PENDING_LEAVE)) {
		ret = -EPERM;
	} else {
		ret = bepio_messaging_save(c->model->draft_id, &c->model->editor, id);
	}
	if (!ret) {
		messages_model_discard(c->model);
		c->model->flow.page = MESSAGES_PAGE_BOX;
		c->model->flow.box = BEPIO_MESSAGE_OUTBOX;
		show_page(c);
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_discard(messages_controller_t *c)
{
	int ret = 0;

	bepio_mutex_lock(messages_lock);
	if (!c->active) {
		ret = -EACCES;
	} else if (!c->model->editing || (c->model->pending != MESSAGES_PENDING_NONE &&
					  c->model->pending != MESSAGES_PENDING_LEAVE)) {
		ret = -EPERM;
	} else {
		messages_model_discard(c->model);
		c->model->flow.page = MESSAGES_PAGE_HOME;
		c->model->flow.box = BEPIO_MESSAGE_INBOX;
		show_page(c);
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

static void confirm_page(messages_controller_t *c, messages_pending_t pending)
{
	c->model->pending = pending;
	c->model->flow.return_page = c->model->flow.page;
	c->model->flow.page = MESSAGES_PAGE_CONFIRM;
	show_page(c);
}

int messages_controller_send(messages_controller_t *c)
{
	int ret;

	bepio_mutex_lock(messages_lock);
	ret = ready(c, false);
	if (!ret && !c->model->editing) {
		ret = -EPERM;
	}
	if (!ret) {
		ret = bepio_messaging_validate(&c->model->editor, true);
	}
	if (!ret) {
		confirm_page(c, MESSAGES_PENDING_SEND);
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

static int submit_editor(messages_controller_t *c, uint32_t *id)
{
	int ret = bepio_messaging_submit(c->model->draft_id, &c->model->editor, id);
	if (!ret) {
		messages_model_discard(c->model);
		c->model->flow.page = MESSAGES_PAGE_DETAIL;
		c->model->flow.box = BEPIO_MESSAGE_OUTBOX;
		c->model->flow.selected_id = *id;
	}
	return ret;
}

static void activate_transport(void)
{
#if defined(ESP_PLATFORM) && defined(CONFIG_BEPIO_MN316)
	int ret = bepio_mn316_manager_emqx_start();
	if (ret) {
		BEPIO_LOGW("MN316 send queued; transport unavailable (%d)", ret);
	}
#endif
}

int messages_controller_send_now(messages_controller_t *c, uint32_t *id)
{
	int ret;
	if (!id) { return -EINVAL; }
	bepio_mutex_lock(messages_lock);
	ret = ready(c, false);
	if (!ret && !c->model->editing) { ret = -EPERM; }
	if (!ret) { ret = bepio_messaging_validate(&c->model->editor, true); }
	if (!ret) { ret = submit_editor(c, id); }
	if (!ret) { show_page(c); }
	bepio_mutex_unlock(messages_lock);
	if (!ret) { activate_transport(); }
	return ret;
}

int messages_controller_delete(messages_controller_t *c, bepio_message_box_t box, uint32_t id)
{
	bepio_message_t message;
	int ret;

	bepio_mutex_lock(messages_lock);
	ret = ready(c, true);
	if (!ret) {
		ret = bepio_messaging_get(box, id, false, &message);
	}
	if (!ret && message.state == BEPIO_MESSAGE_SENDING) {
		ret = -EBUSY;
	}
	if (!ret) {
		c->model->pending_id = id;
		c->model->pending_box = box;
		confirm_page(c, MESSAGES_PENDING_DELETE);
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_delete_now(messages_controller_t *c, bepio_message_box_t box, uint32_t id)
{
	if (box != BEPIO_MESSAGE_INBOX && box != BEPIO_MESSAGE_OUTBOX) return -EINVAL;
	bepio_mutex_lock(messages_lock);
	int ret = ready(c, true);
	if (!ret && c->model->flow.page != MESSAGES_PAGE_DETAIL) ret = -EPERM;
	bepio_message_t removed;
	if (!ret) ret = bepio_messaging_get(box, id, false, &removed);
	if (!ret) ret = bepio_messaging_delete(box, id);
	if (!ret && c->model->flow.box == box && c->model->flow.selected_id == id) {
		bool found = false;
		for (int pass = 0; pass < 2 && !found; pass++) {
			bepio_message_box_t candidate_box = pass ?
				(box == BEPIO_MESSAGE_INBOX ? BEPIO_MESSAGE_OUTBOX : BEPIO_MESSAGE_INBOX) : box;
			bepio_message_t candidate;
			for (size_t i = 0; !bepio_messaging_get_at(candidate_box, i, &candidate); i++) {
				if (strcmp(candidate.payload.phone, removed.payload.phone)) continue;
				c->model->flow.box = candidate_box;
				c->model->flow.selected_id = candidate.id;
				found = true;
				break;
			}
		}
		if (!found) c->model->flow.page = box_page(box);
	}
	if (!ret) show_page(c);
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_confirm(messages_controller_t *c, uint32_t *id)
{
	int ret = -EPERM;
	bool sent = false;

	if (!id) {
		return -EINVAL;
	}
	bepio_mutex_lock(messages_lock);
	if (!c->active) {
		ret = -EACCES;
	} else if (c->model->pending == MESSAGES_PENDING_SEND) {
		ret = submit_editor(c, id);
		sent = !ret;
	} else if (c->model->pending == MESSAGES_PENDING_DELETE) {
		ret = bepio_messaging_delete(c->model->pending_box, c->model->pending_id);
		if (!ret || ret == -ENOENT) {
			*id = c->model->pending_id;
			c->model->flow.box = c->model->pending_box;
			c->model->flow.page = box_page(c->model->flow.box);
			c->model->pending = MESSAGES_PENDING_NONE;
		}
	}
	if (!ret) {
		show_page(c);
	}
	bepio_mutex_unlock(messages_lock);
	if (sent) { activate_transport(); }
	return ret;
}

int messages_controller_cancel(messages_controller_t *c)
{
	int ret = 0;

	bepio_mutex_lock(messages_lock);
	if (!c->active) {
		ret = -EACCES;
	} else if (c->model->pending == MESSAGES_PENDING_NONE) {
		ret = -EPERM;
	} else {
		cancel_pending(c);
		show_page(c);
	}
	bepio_mutex_unlock(messages_lock);
	return ret;
}

int messages_controller_retry(messages_controller_t *c, uint32_t id)
{
	int ret;

	bepio_mutex_lock(messages_lock);
	ret = ready(c, true);
	if (!ret) {
		ret = bepio_messaging_retry(id);
	}
	if (!ret) {
		c->model->flow.box = BEPIO_MESSAGE_OUTBOX;
		c->model->flow.selected_id = id;
		c->model->flow.page = MESSAGES_PAGE_DETAIL;
		show_page(c);
	}
	bepio_mutex_unlock(messages_lock);
	if (!ret) { activate_transport(); }
	return ret;
}
