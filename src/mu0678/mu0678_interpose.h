#ifndef MU0678_INTERPOSE_H
#define MU0678_INTERPOSE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MU0678_DIO_BASE 0x00100000u
#define MU0678_GOT_HID_REGISTER 0x0019d734u
#define MU0678_GOT_HID_DEREGISTER 0x0019d7ecu
#define MU0678_GOT_GUARD 0x00115288u

int mu0678_interpose_init(void);
int mu0678_interpose_patch_word(volatile uint32_t *target, uint32_t expected,
                                uint32_t replacement, uint32_t alternate_expected);

/* Exact SETUP trampoline ABI is intentionally target-specific and is not guessed. */
int mu0678_interpose_setup111(void *session, void *streams, void *setup_result);
void mu0678_interpose_session_finalized(void *session);

#ifdef __cplusplus
}
#endif
#endif
