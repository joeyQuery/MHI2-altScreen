#include "mu0678_interpose.h"
#include "mu0678_cluster.h"

extern int mprotect(void *, unsigned int, int);
extern long sysconf(int);
extern void *dlsym(void *, const char *);
extern unsigned int getpagesize(void);

#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define _SC_PAGESIZE 11

typedef void *(*HidRegisterFn)(void *, void *);
typedef void (*HidDeregisterFn)(void *);

static HidRegisterFn g_hid_register;
static HidDeregisterFn g_hid_deregister;
static uintptr_t g_dio_base;
static int g_installed;

static int guarded_store(volatile uint32_t *target, uint32_t expected,
                         uint32_t replacement, uint32_t alternate)
{
    uint32_t cur = *target;
    if (cur == replacement) return 1;
    if (cur != expected && (!alternate || cur != alternate)) return 0;
    long ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) ps = (long)getpagesize();
    if (ps <= 0) return 0;
    uintptr_t a = (uintptr_t)target;
    uintptr_t page = a & ~((uintptr_t)ps - 1u);
    if (mprotect((void *)page, (unsigned int)ps, PROT_READ|PROT_WRITE|PROT_EXEC) != 0) return 0;
    *target = replacement;
    __asm__ volatile("dmb sy" ::: "memory");
    return 1;
}

int mu0678_interpose_patch_word(volatile uint32_t *target, uint32_t expected,
                                uint32_t replacement, uint32_t alternate_expected)
{
    return guarded_store(target, expected, replacement, alternate_expected);
}

static void *hid_register_passthrough(void *a, void *b)
{
    return g_hid_register ? g_hid_register(a, b) : (void *)0;
}

static void hid_deregister_passthrough(void *a)
{
    if (g_hid_deregister) g_hid_deregister(a);
}

int mu0678_interpose_init(void)
{
    void *server_cb;
    if (g_installed) return 0;
    server_cb = dlsym((void *)0, "s_serverCopyPropertyCallback");
    if (!server_cb) return -1;

    /* The MU0678 executable is ET_EXEC. Derive its load base from the
       exported callback rather than copying a Q absolute address. */
    g_dio_base = (uintptr_t)server_cb - 0x00139a88u;
    if (g_dio_base != MU0678_DIO_BASE) return -2;

    g_hid_register = (HidRegisterFn)dlsym((void *)0, "HIDRegisterDevice");
    g_hid_deregister = (HidDeregisterFn)dlsym((void *)0, "HIDDeregisterDevice");
    if (!g_hid_register || !g_hid_deregister) return -3;

    if (!guarded_store((volatile uint32_t *)(g_dio_base + 0x19d734u),
                       0x00115288u, (uint32_t)(uintptr_t)hid_register_passthrough, 0))
        return -4;
    if (!guarded_store((volatile uint32_t *)(g_dio_base + 0x19d7ecu),
                       0x00115288u, (uint32_t)(uintptr_t)hid_deregister_passthrough, 0))
        return -5;

    g_installed = 1;
    return 0;
}

int mu0678_interpose_setup111(void *session, void *streams, void *setup_result)
{
    (void)session; (void)streams; (void)setup_result;
    return -2;
}

void mu0678_interpose_session_finalized(void *session)
{
    mu0678_alt111_stop(session);
}
