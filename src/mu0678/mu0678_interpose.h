#ifndef MU0678_INTERPOSE_H
#define MU0678_INTERPOSE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MU0678_DIO_BASE 0x00100000u
#define MU0678_GOT_HID_REGISTER 0x0019d734u
#define MU0678_GOT_HID_DEREGISTER 0x0019d7ecu
#define MU0678_GOT_SCREEN_STREAM_CREATE 0x000aa2d8u
#define MU0678_SETUP_BRANCH 0x00023260u
#define MU0678_NVSS_DECODE_CALL 0x0007fb68u
#define MU0678_NVSS_DECODE_CONT 0x0007fb6cu
#define MU0678_NVSS_DECODE_PLT 0x00017bf4u
#define MU0678_VENEER_HINT 0x000ac000u
void *mu0678_resolve_airplay(const char *name);
int mu0678_interpose_init(void);
int mu0678_interpose_patch_word(volatile uint32_t *target, uint32_t expected, uint32_t replacement, uint32_t alternate_expected);
int mu0678_interpose_setup111(void *session, void *stream, void *setup_result, void *key_base);
#ifdef __cplusplus
}
#endif
#endif