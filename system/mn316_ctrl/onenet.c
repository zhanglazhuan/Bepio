#include "onenet.h"

#include <stdio.h>
#include <string.h>
#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "mbedtls/platform_util.h"

#if __has_include("onenet_local.h")
#include "onenet_local.h"
#endif
#ifndef BEPIO_ONENET_DEVICE_KEY
#define BEPIO_ONENET_DEVICE_KEY ""
#endif
#ifndef BEPIO_ONENET_EXPIRES
#define BEPIO_ONENET_EXPIRES "1924833600"
#endif

int bepio_onenet_config_with_key(char *command, size_t capacity, const char *device,
				 const char *key_base64, const char *expires)
{
	unsigned char key[128] = {0}, digest[20] = {0}, signature[32] = {0};
	size_t key_size = 0, signature_size = 0;
	char escaped[96];
	size_t used = 0;
	char message[256];
	int ret = -1;

	if (!command || !capacity || !device || !device[0] || strlen(device) > 64 ||
	    !key_base64 || !expires || !expires[0]) {
		return -1;
	}
	command[0] = '\0';
	for (const char *p = device; *p; p++) {
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
		      (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) {
			return -1;
		}
	}
	int message_size = snprintf(message, sizeof(message), "%s\nsha1\nproducts/"
		BEPIO_ONENET_PRODUCT "/devices/%s\n2018-10-31", expires, device);
	if (message_size < 0 || (size_t)message_size >= sizeof(message)) {
		return -1;
	}
	const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
	if (!md || mbedtls_base64_decode(key, sizeof(key), &key_size,
		(const unsigned char *)key_base64,
		strlen(key_base64)) || !key_size ||
	    mbedtls_md_hmac(md, key, key_size, (const unsigned char *)message,
			    (size_t)message_size, digest) ||
	    mbedtls_base64_encode(signature, sizeof(signature), &signature_size,
				  digest, sizeof(digest))) {
		goto out;
	}
	/* Base64 has only three characters requiring percent encoding. */
	for (size_t i = 0; i < signature_size; i++) {
		const char *encoded = signature[i] == '+' ? "%2B" :
			 signature[i] == '/' ? "%2F" : signature[i] == '=' ? "%3D" : NULL;
		if (encoded) {
			memcpy(escaped + used, encoded, 3);
			used += 3;
		} else {
			escaped[used++] = (char)signature[i];
		}
	}
	escaped[used] = '\0';
	int length = snprintf(command, capacity,
		"AT+MQTTCFG=\"mqtts.heclouds.com\",1883,\"%s\",60,\"%s\","
		"\"version=2018-10-31&res=products%%2F%s%%2Fdevices%%2F%s&et=%s"
		"&method=sha1&sign=%s\",1,0",
		device, BEPIO_ONENET_PRODUCT, BEPIO_ONENET_PRODUCT,
		device, expires, escaped);
	ret = length >= 0 && (size_t)length < capacity ? 0 : -1;
out:
	mbedtls_platform_zeroize(key, sizeof(key));
	mbedtls_platform_zeroize(digest, sizeof(digest));
	mbedtls_platform_zeroize(signature, sizeof(signature));
	mbedtls_platform_zeroize(escaped, sizeof(escaped));
	mbedtls_platform_zeroize(message, sizeof(message));
	if (ret) {
		command[0] = '\0';
	}
	return ret;
}

int bepio_onenet_config(char *command, size_t capacity, const char *device)
{
	return bepio_onenet_config_with_key(command, capacity, device,
		BEPIO_ONENET_DEVICE_KEY, BEPIO_ONENET_EXPIRES);
}
