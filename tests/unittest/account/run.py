"""Run account state machine against deterministic SDK/NVS mocks."""
import pathlib, subprocess, tempfile
root = pathlib.Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory() as directory:
    d = pathlib.Path(directory)
    common = r"""
#ifndef MOCK_SDK_H
#define MOCK_SDK_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
typedef int esp_err_t;
typedef int nvs_handle_t;
typedef void *esp_timer_handle_t;
typedef struct { void (*callback)(void *); const char *name; } esp_timer_create_args_t;
#define ESP_ERR_NVS_NOT_FOUND 1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_INVALID_ARG 3
#define NVS_READWRITE 0
static int64_t mock_us;
static unsigned char blob[256];
static size_t blob_size;
static int fail_write;
static int64_t esp_timer_get_time(void) { return mock_us; }
static int nvs_flash_init(void) { return 0; }
static int nvs_open(const char *a, int b, int *c) { (void)a; (void)b; *c=1; return 0; }
static int nvs_get_blob(int a, const char *b, void *c, size_t *n) {
 (void)a;(void)b;if (!blob_size) return 1; memcpy(c,blob,blob_size);*n=blob_size;return 0;
}
static int nvs_set_blob(int a,const char *b,const void *c,size_t n) {
 (void)a;(void)b;if(fail_write)return 3;memcpy(blob,c,n);blob_size=n;return 0;
}
static int nvs_commit(int a) { (void)a;return 0; }
static int esp_efuse_mac_get_default(uint8_t *m) { memcpy(m,"abcdef",6);return 0; }
static int esp_timer_create(const esp_timer_create_args_t *a,void **b) { (void)a;*b=(void*)1;return 0; }
static int esp_timer_start_periodic(void *a,int64_t b) { (void)a;(void)b;return 0; }
#endif
"""
    (d/'mock.h').write_text(common)
    for name in ['esp_mac.h','esp_timer.h','nvs_flash.h','nvs.h']:
        (d/name).write_text('#include "mock.h"\n')
    (d/'bepio_mutex.h').write_text('#define BEPIO_MUTEX_DEFINE(x)\n#define bepio_mutex_lock(x) ((void)0)\n#define bepio_mutex_unlock(x) ((void)0)\n')
    source = r"""
#ifndef ESTALE
#define ESTALE 116
#endif
#include <assert.h>
#include <stdio.h>
#include "bepio_account.c"
int main(void) {
 bepio_account_snapshot_t s; char id[33]; uint32_t token, old;
 assert(!bepio_account_init());
 bepio_account_snapshot(&s); assert(!strcmp(s.id,"107075202213222"));
 assert(!s.confirmed);
 assert(bepio_account_request("123456789")==-ENETDOWN);
 bepio_account_set_online(true);
 assert(bepio_account_request("A bad")==-EINVAL);
 assert(!bepio_account_request("123456789"));
 assert(!bepio_account_take(id,&token));
 assert(bepio_account_complete(id,token+1,0,1000,87400)==-ESTALE);
 assert(!bepio_account_complete(id,token,-EEXIST,1000,0));
 bepio_account_snapshot(&s); assert(!s.confirmed);
 assert(!bepio_account_request("123456789")); assert(!bepio_account_take(id,&token));
 assert(!bepio_account_complete(id,token,0,1000,87400));
 assert(bepio_account_request("987654321")==-EAGAIN);
 mock_us+=86400000000LL;
 assert(!bepio_account_request("987654321")); assert(!bepio_account_take(id,&old));
 mock_us+=31000000; checkpoint(NULL);
 assert(!bepio_account_request("987654321")); assert(!bepio_account_take(id,&token));
 assert(bepio_account_complete(id,old,0,90000,176400)==-ESTALE);
 assert(!bepio_account_complete(id,token,0,90000,176400));
 bepio_account_record(123,456); mock_us+=65000000;
 bepio_account_set_online(false); bepio_account_snapshot(&s);
 assert(s.tx_bytes==123 && s.rx_bytes==456 && s.connected_seconds==86496);
 assert(!strcmp(s.id,"987654321") && s.confirmed);
 memset(&saved,0,sizeof(saved)); assert(!bepio_account_init());
 bepio_account_snapshot(&s); assert(s.tx_bytes==123 && !strcmp(s.id,"987654321"));
 strcpy(saved.id,"bepio-616263646566"); saved.confirmed=true;
 memcpy(blob,&saved,sizeof(saved)); blob_size=sizeof(saved);
 assert(!bepio_account_init());
 bepio_account_snapshot(&s); assert(!strcmp(s.id,"107075202213222") && !s.confirmed);
 fail_write=1; checkpoint(NULL); bepio_account_snapshot(&s); assert(s.storage_error);
 puts("PASS: identity, validation, conflicts, cooldown, stale replies, usage and persistence");
}
"""
    (d/'test.c').write_text(source)
    exe = d/'test.exe'
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-I',str(d),'-I',str(root/'system/services/account'),str(d/'test.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    override = r'''
#define CONFIG_BEPIO_DEVICE_ID_OVERRIDE "100"
#ifndef ESTALE
#define ESTALE 116
#endif
#include <assert.h>
#include <string.h>
#include "bepio_account.c"
int main(void) {
 bepio_account_snapshot_t s;
 assert(!bepio_account_init());
 bepio_account_snapshot(&s); assert(!strcmp(s.id,"100"));
 strcpy(saved.id,"123456789"); saved.confirmed=true; saved.next_change=90000;
 persist(); assert(!storage_error);
 assert(!bepio_account_init());
 bepio_account_snapshot(&s);
 assert(!strcmp(s.id,"100") && !s.confirmed && !saved.next_change);
 assert(!bepio_account_init());
 bepio_account_snapshot(&s); assert(!strcmp(s.id,"100"));
}
'''
    (d/'override.c').write_text(override)
    override_exe = d/'override.exe'
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-I',str(d),
                    '-I',str(root/'system/services/account'),str(d/'override.c'),
                    '-o',str(override_exe)],check=True)
    subprocess.run([str(override_exe)],check=True)
