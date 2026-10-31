#include "apps/settings/view.h"
void settings_view_init(settings_view_t *view, void *root, void *group)
{
	*view = (settings_view_t){.page = SETTINGS_PAGE_HOME, .available = true, .root = root, .group = group};
}
void settings_view_deinit(settings_view_t *view) { *view = (settings_view_t){0}; }
void settings_view_render(const settings_view_t *view, const settings_model_t *model) { (void)view; (void)model; }
