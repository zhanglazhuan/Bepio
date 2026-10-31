#include "bepio_store.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <errno.h>

static int result(esp_err_t error)
{
	if (error == ESP_ERR_NVS_NOT_FOUND) { return -ENOENT; }
	return error == ESP_OK ? 0 : -EIO;
}

int bepio_store_read(const char *space, const char *key, void *data, size_t *size)
{
	esp_err_t ret = nvs_flash_init();
	if (ret) { return result(ret); }
	nvs_handle_t handle;
	ret = nvs_open(space, NVS_READONLY, &handle);
	if (ret) { return result(ret); }
	ret = nvs_get_blob(handle, key, data, size);
	nvs_close(handle);
	return result(ret);
}

int bepio_store_write(const char *space, const char *key, const void *data, size_t size)
{
	esp_err_t ret = nvs_flash_init();
	if (ret) { return result(ret); }
	nvs_handle_t handle;
	ret = nvs_open(space, NVS_READWRITE, &handle);
	if (ret) { return result(ret); }
	ret = nvs_set_blob(handle, key, data, size);
	if (!ret) { ret = nvs_commit(handle); }
	nvs_close(handle);
	return result(ret);
}
