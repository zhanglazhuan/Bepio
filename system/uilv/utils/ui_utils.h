#ifndef BEPIO_UI_UTILS_H_
#define BEPIO_UI_UTILS_H_
#include "lvgl.h"

/* LCD assets are linked into internal flash as LVGL image descriptors. */
#define EPOS_LV_IMG_DECLARE(name) LV_IMAGE_DECLARE(name)
#define EPOS_LV_IMG_USE(name) (&(name))
#define EPOS_LV_IMG_USE_WITH_MOUNT(name, mount_letter) (&(name))
#endif
