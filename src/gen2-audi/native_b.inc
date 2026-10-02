/*
 * native_b.inc -- MU0678 native "Screen B" for AirPlay stream 111 (PHASE 1, STUB VIDEO SINK)
 *
 * Included textually by libaltscreen111_gen2.c (single translation unit; it uses that
 * file's statics: logf_u2, g_master_key/g_master_valid, g_derive_screen, g_lock, ...).
 *
 * WHAT THIS DOES (only when /tmp/mibr-alt111-audi.native-b exists; default OFF):
 *   Instead of GEN2's own stream-111 receiver, build a second stock
 *   AirPlayReceiverSessionScreen ("Screen B") using the MU0678 libairplay's own
 *   functions, with its own listener, and let the stock ScreenStream machinery
 *   (ProcessFrames -> ScreenStreamProcessData -> NvssVideoImpl::decode) consume
 *   the iPhone's stream 111.
 *
 * PHASE 1 SAFETY RULE: B's video object is a STUB.  It never reaches
 * NvSSVideoOpen / kdInitializeNV / kdTerminateNV, never touches layer 59 and
 * never drives any display.  Decoded-side output is Annex-B written to a
 * capped file in /tmp.  Type 110 (the MMI picture) uses the untouched real
 * NvssVideoImpl.
 *
 * HOW THE STUB IS ATTACHED (all offsets are for libairplay sha256 a3ab0fe0...,
 * checked at runtime, fail-closed):
 *   - ScreenStreamCreate (0x7f544) creates a fresh NvssVideoImpl per stream and
 *     immediately calls virtual slot +0x08 = initialize().  We export the
 *     NvssVideoImpl methods ourselves (LD_PRELOAD interposition of the exported
 *     symbols, which libairplay's own vtable relocations resolve to).  When our
 *     initialize() wrapper runs on the B-start thread while g_nb_starting is set,
 *     that object is registered as a B impl and served by stubs.
 *   - Every other object is forwarded unchanged to the real method.
 *   - Before StartSession(B) we verify that libairplay's vtable slots really point
 *     at our wrappers.  If not, B is refused (nothing is started).
 *
 * VTABLE (from _ZTV13NvssVideoImpl @0xa92e0, vptr = +8, verified from relocations):
 *   +0x00 D1  +0x04 D0  +0x08 initialize  +0x0c finalize  +0x10 configure
 *   +0x14 start  +0x18 stop  +0x1c decode  +0x20 setVisibility
 *
 * NOT DONE HERE (later stages): real NvSS for B, layer/output rewrite, VC presenter.
 * Nothing in this file has been run on hardware at the time of writing.
 */

#include <sys/select.h>
#include <process.h>

#define NB_MARKER        "/tmp/mibr-alt111-audi.native-b"
#define NB_CAPTURE_FILE  "/tmp/mibr-nativeb.h264"
#define NB_STATUS_FILE   "/tmp/mibr-nativeb.status"
#define NB_CAPTURE_CAP   (6u * 1024u * 1024u)   /* /tmp is RAM: hard cap */
#define NB_MAX_IMPLS     4
#define NB_ACCEPT_SLICES 30                     /* x1 s */
/* STAGE 2 (display) -- default OFF.  Needs BOTH the Phase-1 marker and this one. */
#define NB_DISP_MARKER   "/tmp/mibr-alt111-audi.native-b-display"
#define NB_DISP_CFG      "/tmp/mibr-nativeb-disp.cfg"   /* optional: "outdev layer w h x y" (decimal ints) */

/* --- libairplay ABI facts (MU0678, sha256 a3ab0fe0...) ------------------------- */
#define NB_OFF_SCREEN_CREATE   0x269ccu
#define NB_OFF_SCREENSTREAMCR  0x7f544u
#define NB_OFF_SCREENTHREAD    0x20318u
#define NB_OFF_TS_UPTICKS      0x20644u   /* _ScreenGetUpTicksNearSynchronizedNTPTime */
#define NB_OFF_TS_NTP          0x20650u   /* _ScreenGetSynchronizedNTPTime */
#define NB_OFF_VPTR            0xa92e8u   /* NvssVideoImpl vptr target */
#define NB_SESS_FAMILY         0x51u      /* byte: socket family for ServerSocketOpen */
#define NB_SESS_UUID           0x6cu      /* session UUID bytes */
#define NB_SESS_DEVID          0x80u      /* uint64 client device id */
#define NB_SESS_SERVER         0x0cu      /* pointer to server object */
#define NB_SRV_SCREENCFG       0xd0u      /* CFDictionary given to Screen_StartSession */
#define NB_SRV_FRAMESCFG       0xc8u      /* third argument of Screen_ProcessFrames */
#define NB_SLOT_INIT 2
#define NB_SLOT_FINI 3
#define NB_SLOT_CONF 4
#define NB_SLOT_START 5
#define NB_SLOT_STOP 6
#define NB_SLOT_DECODE 7
#define NB_SLOT_SETVIS 8

typedef OSStatus (*nb_screen_create_t)(void **);
typedef void (*nb_screen_seteventhandler_t)(void *, void (*)(void *, CFTypeRef, CFTypeRef, void *), void *, void *);
typedef void (*nb_screen_updatestate_t)(void *, int);
typedef OSStatus (*nb_screen_setup_t)(void *, CFDictionaryRef, void *);
typedef OSStatus (*nb_screen_setsecurity_t)(void *, const uint8_t *, const uint8_t *);
typedef void (*nb_screen_setuuid_t)(void *, const void *);
typedef void (*nb_screen_setdevid_t)(void *, uint64_t);
typedef void (*nb_screen_settimesync_t)(void *, const void *);
typedef void (*nb_screen_settime_t)(void *, double);
typedef OSStatus (*nb_screen_startsession_t)(void *, CFDictionaryRef);
typedef OSStatus (*nb_screen_processframes_t)(void *, void *, void *);
typedef void (*nb_screen_void_t)(void *);
typedef OSStatus (*nb_serversocketopen_t)(int, int, int, int, int *, int, int *);
typedef OSStatus (*nb_socketaccept_t)(int, int, int *, void *);
typedef OSStatus (*nb_netsocket_create_t)(void **, int);
typedef void (*nb_netsocket_delete_t)(void *);
typedef double (*nb_cftime_t)(void);

static struct {
    int ok;
    uintptr_t base;
    nb_screen_create_t          screen_create;
    nb_screen_seteventhandler_t seteventhandler;
    nb_screen_updatestate_t     updatestate;
    nb_screen_setup_t           setup;
    nb_screen_setsecurity_t     setsecurity;
    nb_screen_setuuid_t         setuuid;
    nb_screen_setdevid_t        setdevid;
    nb_screen_settimesync_t     settimesync;
    nb_screen_settime_t         setrecvend, setauthend, setntpend;
    nb_screen_startsession_t    startsession;
    nb_screen_processframes_t   processframes;
    nb_screen_void_t            stopsession, screen_delete;
    nb_serversocketopen_t       serversocketopen;
    nb_socketaccept_t           socketaccept;
    nb_netsocket_create_t       netsocket_create;
    nb_netsocket_delete_t       netsocket_delete;
    nb_cftime_t                 cftime;
} g_nbabi;

/* Real NvssVideoImpl methods (resolved lazily via RTLD_NEXT; never our own wrappers). */
typedef int (*nb_real_i_t)(void *);
typedef int (*nb_real_conf_t)(void *, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
typedef int (*nb_real_dec_t)(void *, uint8_t *, uint32_t, uint64_t, uint8_t);
typedef int (*nb_real_vis_t)(void *, uint8_t);
static nb_real_i_t    nb_real_init, nb_real_fini, nb_real_start, nb_real_stop;
static nb_real_conf_t nb_real_conf;
static nb_real_dec_t  nb_real_dec;
static nb_real_vis_t  nb_real_vis;
static pthread_once_t nb_real_once = PTHREAD_ONCE_INIT;

/* Registry of B impl objects (accessed from the decode hot path: plain reads). */
static void *volatile g_nb_impls[NB_MAX_IMPLS];
static volatile int   g_nb_starting;      /* B's StartSession is running on g_nb_start_thread */
static pthread_t      g_nb_start_thread;

/* Capture / statistics for the stub. */
static pthread_mutex_t g_nb_cap_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE *g_nb_cap_fp;
static uint32_t g_nb_cap_bytes;
static volatile uint32_t g_nb_decode_calls, g_nb_nal_sps, g_nb_nal_pps, g_nb_nal_idr, g_nb_nal_slice;
static volatile uint32_t g_nb_conf[7];
static volatile int g_nb_conf_seen, g_nb_start_calls, g_nb_stop_calls, g_nb_fini_calls, g_nb_init_calls;

/* Stage 2: real NvSS for B, with B-only NvSSVideoOpen parameter rewrite. */
static volatile int g_nb_disp;                 /* display mode active for the current B */
static int g_nb_cfg_outdev = 1, g_nb_cfg_layer = 58, g_nb_cfg_w = 1440, g_nb_cfg_h = 456, g_nb_cfg_x = 0, g_nb_cfg_y = 0;
static volatile int g_nb_real_open;            /* B's real start() opened NvSS successfully */
static volatile int g_nb_in_bstart;            /* B's real start() is executing */
static pthread_t g_nb_bstart_thread;
static pthread_mutex_t g_nb_open_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_nb_run_lock = PTHREAD_MUTEX_INITIALIZER;   /* serialises B decode vs B real stop */
static volatile int g_nb_closed_by_a;          /* B's NvSS was closed ahead of stock A teardown */
static volatile int g_nb_a_logs;               /* cap on A call logging */
static volatile int g_nb_a_started;            /* stock A's start() has completed (open-order experiment) */
typedef uint32_t (*nb_real_open_t)(void *, const void *);
static nb_real_open_t nb_real_open;

/* Session ownership of the running B. */
struct nb_ctx {
    void *session;
    void *screen;
    void *server_cfg;          /* value handed to Screen_StartSession */
    void *server_frames;       /* third arg of Screen_ProcessFrames */
    int listen_fd;
    volatile int client_fd;
    volatile int stop;
    volatile int done;
    int port;
    pthread_t th;
};
static struct nb_ctx *volatile g_nb_ctx;
static pthread_mutex_t g_nb_lock = PTHREAD_MUTEX_INITIALIZER;

static int nb_is_b(const void *self)
{
    int i;
    if (!self) return 0;
    for (i = 0; i < NB_MAX_IMPLS; ++i)
        if (g_nb_impls[i] == self) return 1;
    return 0;
}


#define NB_A_LOG(...) do { if (g_nb_a_logs < 200) { ++g_nb_a_logs; logf_u2(__VA_ARGS__); } } while (0)

/* Close B's real NvSS output ahead of the stock main-screen (A) stop/finalize.  Only in display mode. */
static void nb_close_b_for_a(const char *why)
{
    void *b;
    if (!g_nb_disp || !g_nb_real_open || !nb_real_stop) return;
    b = g_nb_impls[0];
    if (!b) return;
    pthread_mutex_lock(&g_nb_run_lock);
    if (g_nb_real_open) {
        int r;
        logf_u2("NATIVE-B closing cluster NvSS before stock A %s: real stop ENTER self=%p", why, b);
        r = nb_real_stop(b);
        g_nb_real_open = 0;
        g_nb_closed_by_a = 1;
        logf_u2("NATIVE-B closing cluster NvSS before stock A %s: real stop EXIT rc=%d", why, r);
    }
    pthread_mutex_unlock(&g_nb_run_lock);
}

static void nb_register_impl(void *self)
{
    int i;
    for (i = 0; i < NB_MAX_IMPLS; ++i)
        if (!g_nb_impls[i]) { g_nb_impls[i] = self; return; }
    logf_u2("NATIVE-B impl registry full");
}

static void nb_unregister_all(void)
{
    int i;
    for (i = 0; i < NB_MAX_IMPLS; ++i) g_nb_impls[i] = NULL;
}

static void nb_resolve_real(void)
{
    nb_real_init  = (nb_real_i_t)dlsym(RTLD_NEXT, "_ZN13NvssVideoImpl10initializeEv");
    nb_real_fini  = (nb_real_i_t)dlsym(RTLD_NEXT, "_ZN13NvssVideoImpl8finalizeEv");
    nb_real_conf  = (nb_real_conf_t)dlsym(RTLD_NEXT, "_ZN13NvssVideoImpl9configureEjjjjjjj");
    nb_real_start = (nb_real_i_t)dlsym(RTLD_NEXT, "_ZN13NvssVideoImpl5startEv");
    nb_real_stop  = (nb_real_i_t)dlsym(RTLD_NEXT, "_ZN13NvssVideoImpl4stopEv");
    nb_real_dec   = (nb_real_dec_t)dlsym(RTLD_NEXT, "_ZN13NvssVideoImpl6decodeEPhjyb");
    nb_real_vis   = (nb_real_vis_t)dlsym(RTLD_NEXT, "_ZN13NvssVideoImpl13setVisibilityEh");
}

/* ---------------------------------------------------------------- stub capture */

static void nb_write_status(const char *state)
{
    FILE *f = fopen(NB_STATUS_FILE, "w");
    if (!f) return;
    fprintf(f, "state=%s\n", state);
    fprintf(f, "init_calls=%d start_calls=%d stop_calls=%d fini_calls=%d\n",
            g_nb_init_calls, g_nb_start_calls, g_nb_stop_calls, g_nb_fini_calls);
    fprintf(f, "configure_seen=%d args=%u,%u,%u,%u,%u,%u,%u\n", g_nb_conf_seen,
            g_nb_conf[0], g_nb_conf[1], g_nb_conf[2], g_nb_conf[3], g_nb_conf[4], g_nb_conf[5], g_nb_conf[6]);
    fprintf(f, "decode_calls=%u sps=%u pps=%u idr=%u slice=%u capture_bytes=%u\n",
            g_nb_decode_calls, g_nb_nal_sps, g_nb_nal_pps, g_nb_nal_idr, g_nb_nal_slice, g_nb_cap_bytes);
    fclose(f);
}

static void nb_scan_nals(const uint8_t *p, uint32_t n)
{
    uint32_t i = 0;
    while (i + 4 <= n) {
        uint32_t sc = 0;
        if (p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1) sc = 3;
        else if (i + 5 <= n && p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 0 && p[i + 3] == 1) sc = 4;
        if (sc && i + sc < n) {
            unsigned t = p[i + sc] & 0x1fu;
            if (t == 7) ++g_nb_nal_sps;
            else if (t == 8) ++g_nb_nal_pps;
            else if (t == 5) ++g_nb_nal_idr;
            else if (t == 1) ++g_nb_nal_slice;
            i += sc;
        } else ++i;
    }
}

static void nb_capture_au(const uint8_t *data, uint32_t len)
{
    if (!data || !len) return;
    nb_scan_nals(data, len);
    pthread_mutex_lock(&g_nb_cap_lock);
    if (!g_nb_cap_fp) g_nb_cap_fp = fopen(NB_CAPTURE_FILE, "wb");
    if (g_nb_cap_fp && g_nb_cap_bytes + len <= NB_CAPTURE_CAP) {
        if (fwrite(data, 1, len, g_nb_cap_fp) == len) g_nb_cap_bytes += len;
    }
    pthread_mutex_unlock(&g_nb_cap_lock);
}

static void nb_close_capture(void)
{
    pthread_mutex_lock(&g_nb_cap_lock);
    if (g_nb_cap_fp) { fflush(g_nb_cap_fp); fclose(g_nb_cap_fp); g_nb_cap_fp = NULL; }
    pthread_mutex_unlock(&g_nb_cap_lock);
}


/* ------------------------------------------------ forward mode (separate cluster renderer process)
 * Marker /tmp/mibr-alt111-audi.native-b-fwd: B stays a stub inside dio_manager (no NvSS) and every
 * access unit is sent to vc_render on 127.0.0.1:19830:  u32 'AV11' | u32 len | u64 ts | u32 flag | data. */
#include <netinet/tcp.h>
#define NB_FWD_MARKER "/tmp/mibr-alt111-audi.native-b-fwd"
#define NB_FWD_PORT   19830
#define NB_FWD_BUFSZ  (384u * 1024u)
static volatile int g_nb_fwd;
static int g_nb_fwd_fd = -1;
static unsigned long long g_nb_fwd_retry_ms;
static unsigned g_nb_fwd_sent, g_nb_fwd_drop;
static uint8_t g_nb_fwd_buf[NB_FWD_BUFSZ];

/* r11 keyframe handling: replay cached SPS/PPS on connect, hold non-IDR frames until an IDR arrives,
 * and ask the phone for one (AirPlayReceiverSessionForceKeyFrame) up to NB_FWD_FK_MAX times, 1/s. */
#define NB_FWD_FK_MAX 3
typedef OSStatus (*nb_fk_t)(AirPlayReceiverSessionRef, void (*)(OSStatus, CFDictionaryRef, void *), void *);
static nb_fk_t g_nb_fwd_fk;
static AirPlayReceiverSessionRef g_nb_fwd_session;
static uint8_t g_nb_ps[512];
static uint32_t g_nb_ps_len;
static int g_nb_fwd_need_idr;
static unsigned g_nb_fwd_fk_n;
static unsigned long long g_nb_fwd_fk_ms;
static void nb_fk_done(OSStatus st, CFDictionaryRef resp, void *opaque) { (void)st; (void)resp; (void)opaque; }

static unsigned long long nb_now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (unsigned long long)t.tv_sec * 1000ull + (unsigned long long)t.tv_nsec / 1000000ull;
}

static void nb_fwd_close(void)
{
    if (g_nb_fwd_fd >= 0) {
        close(g_nb_fwd_fd);
        g_nb_fwd_fd = -1;
        logf_u2("NATIVE-B forward: renderer connection closed (sent=%u dropped=%u)", g_nb_fwd_sent, g_nb_fwd_drop);
    }
}

static void nb_fwd_write(const uint8_t *data, uint32_t len, uint64_t ts, uint32_t flag)
{
    uint32_t hdr[5];
    size_t total, off = 0;
    int stalls = 0;
    if (g_nb_fwd_fd < 0) return;
    if ((size_t)len + sizeof(hdr) > NB_FWD_BUFSZ) { ++g_nb_fwd_drop; return; }
    hdr[0] = 0x31315641u; hdr[1] = len; hdr[2] = (uint32_t)(ts & 0xffffffffull); hdr[3] = (uint32_t)(ts >> 32); hdr[4] = flag;
    memcpy(g_nb_fwd_buf, hdr, sizeof(hdr));
    memcpy(g_nb_fwd_buf + sizeof(hdr), data, len);
    total = sizeof(hdr) + len;
    while (off < total) {
        ssize_t w = send(g_nb_fwd_fd, g_nb_fwd_buf + off, total - off, 0);
        if (w > 0) { off += (size_t)w; continue; }
        if (w < 0 && errno == EINTR) continue;
        if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) && ++stalls <= 20) continue;
        logf_u2("NATIVE-B forward: send failed errno=%d sent=%u/%u stalls=%d", errno, (unsigned)off, (unsigned)total, stalls);
        nb_fwd_close();
        ++g_nb_fwd_drop;
        return;
    }
    if (stalls) logf_u2("NATIVE-B forward: send stalled %d x150ms for len=%u but completed", stalls, (unsigned)len);
    ++g_nb_fwd_sent;
}

static void nb_fwd_scan(const uint8_t *p, uint32_t n, int *has_idr, int *has_ps)
{
    uint32_t st[64], i = 0, k = 0, j, e;
    unsigned ty[64];
    *has_idr = 0; *has_ps = 0;
    while (i + 4 <= n && k < 64) {
        uint32_t sc = 0;
        if (p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 1) sc = 3;
        else if (i + 5 <= n && p[i] == 0 && p[i + 1] == 0 && p[i + 2] == 0 && p[i + 3] == 1) sc = 4;
        if (sc && i + sc < n) {
            st[k] = i; ty[k] = p[i + sc] & 0x1fu; ++k;
            i += sc;
        } else ++i;
    }
    for (j = 0; j < k; ++j) {
        if (ty[j] == 5) *has_idr = 1;
        if (ty[j] == 7 || ty[j] == 8) *has_ps = 1;
    }
    for (j = 0; j < k && ty[j] != 7; ++j) { }
    if (j < k) {
        int pps = 0;
        e = j;
        while (e < k && (ty[e] == 7 || ty[e] == 8)) { if (ty[e] == 8) pps = 1; ++e; }
        if (pps) {
            uint32_t end = (e < k) ? st[e] : n;
            uint32_t len = end - st[j];
            if (len > 0 && len <= sizeof(g_nb_ps)) { memcpy(g_nb_ps, p + st[j], len); g_nb_ps_len = len; }
        }
    }
}

static void nb_fwd_send(const uint8_t *data, uint32_t len, uint64_t ts, uint32_t flag)
{
    int idr = 0, ps = 0, fresh = 0;
    if (!g_nb_fwd || !data || !len) return;
    {   /* NB_RX_STATS: frames received from the phone, every 5 s */
        static unsigned long long rx_w0, rx_b;
        static unsigned rx_n, rx_idr;
        struct timespec rx_t;
        unsigned long long rx_now;
        clock_gettime(CLOCK_MONOTONIC, &rx_t);
        rx_now = (unsigned long long)rx_t.tv_sec * 1000ull + (unsigned long long)(rx_t.tv_nsec / 1000000L);
        if (!rx_w0) rx_w0 = rx_now;
        ++rx_n; rx_b += len; if (flag & 1u) ++rx_idr;
        if (rx_now - rx_w0 >= 5000ull) {
            unsigned long long el = rx_now - rx_w0;
            logf_u2("NATIVE-B rx %u frames in %llu ms = %llu fps; %llu KB; flag1=%u; fwd sent=%u dropped=%u",
                    rx_n, el, (unsigned long long)rx_n * 1000ull / el, rx_b / 1024ull, rx_idr,
                    g_nb_fwd_sent, g_nb_fwd_drop);
            rx_w0 = rx_now; rx_n = 0; rx_b = 0; rx_idr = 0;
        }
    }
    if (g_nb_fwd_fd < 0) {
        struct sockaddr_in a;
        struct timeval tv;
        int fd;
        unsigned long long now = nb_now_ms();
        if (now < g_nb_fwd_retry_ms) { ++g_nb_fwd_drop; return; }
        g_nb_fwd_retry_ms = now + 250ull;
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) { ++g_nb_fwd_drop; return; }
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_port = htons(NB_FWD_PORT);
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        tv.tv_sec = 0; tv.tv_usec = 150000;
        (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        {
            int sb = 256 * 1024;
            (void)setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sb, sizeof(sb));
        }
        {   /* NB_FWD_NODELAY: do not let Nagle hold small frames waiting for a delayed ACK */
            int nd = 1;
            (void)setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nd, sizeof(nd));
        }
        if (connect(fd, (struct sockaddr *)&a, sizeof(a)) != 0) { close(fd); ++g_nb_fwd_drop; return; }
        g_nb_fwd_fd = fd;
        fresh = 1;
        logf_u2("NATIVE-B forward: connected to renderer on 127.0.0.1:%d", NB_FWD_PORT);
    }
    nb_fwd_scan(data, len, &idr, &ps);
    if (fresh) {
        g_nb_fwd_need_idr = 1; g_nb_fwd_fk_n = 0; g_nb_fwd_fk_ms = 0;
        if (!ps && g_nb_ps_len) {
            nb_fwd_write(g_nb_ps, g_nb_ps_len, ts, 0);
            logf_u2("NATIVE-B forward: replayed cached SPS/PPS (%u bytes)", (unsigned)g_nb_ps_len);
            if (g_nb_fwd_fd < 0) return;
        }
    }
    if (g_nb_fwd_need_idr) {
        if (idr) {
            g_nb_fwd_need_idr = 0;
            logf_u2("NATIVE-B forward: IDR seen (keyframe requests=%u)", g_nb_fwd_fk_n);
        } else if (ps) {
            /* parameter sets only: always pass through */
        } else if (g_nb_fwd_fk_n < NB_FWD_FK_MAX) {
            unsigned long long now = nb_now_ms();
            if (now >= g_nb_fwd_fk_ms) {
                OSStatus r = -1;
                if (g_nb_fwd_fk && g_nb_fwd_session) r = g_nb_fwd_fk(g_nb_fwd_session, nb_fk_done, NULL);
                ++g_nb_fwd_fk_n;
                g_nb_fwd_fk_ms = now + 500ull;
                logf_u2("NATIVE-B forward: waiting for IDR; ForceKeyFrame #%u rc=%d", g_nb_fwd_fk_n, (int)r);
            }
            ++g_nb_fwd_drop;
            return;
        } else {
            g_nb_fwd_need_idr = 0;
            logf_u2("NATIVE-B forward: no IDR after %u requests; forwarding anyway", g_nb_fwd_fk_n);
        }
    }
    nb_fwd_write(data, len, ts, flag);
}

/* ------------------------------------------- exported NvssVideoImpl interposers
 * Non-B objects are forwarded unchanged.  B objects never reach NvSS.  */

int nb_impl_initialize(void *self) __asm__("_ZN13NvssVideoImpl10initializeEv");
int nb_impl_finalize(void *self)   __asm__("_ZN13NvssVideoImpl8finalizeEv");
int nb_impl_configure(void *self, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e, unsigned f, unsigned g)
    __asm__("_ZN13NvssVideoImpl9configureEjjjjjjj");
int nb_impl_start(void *self)      __asm__("_ZN13NvssVideoImpl5startEv");
int nb_impl_stop(void *self)       __asm__("_ZN13NvssVideoImpl4stopEv");
int nb_impl_decode(void *self, uint8_t *data, uint32_t len, uint64_t ts, uint8_t flag)
    __asm__("_ZN13NvssVideoImpl6decodeEPhjyb");
int nb_impl_setvis(void *self, uint8_t v) __asm__("_ZN13NvssVideoImpl13setVisibilityEh");

int nb_impl_initialize(void *self)
{
    (void)pthread_once(&nb_real_once, nb_resolve_real);
    if (g_nb_starting && pthread_equal(g_nb_start_thread, pthread_self())) {
        nb_register_impl(self);
        ++g_nb_init_calls;
        logf_u2("NATIVE-B stub impl registered self=%p (no NvSS init)", self);
        return 0;
    }
    {
        int r;
        NB_A_LOG("NATIVE-A initialize ENTER self=%p", self);
        r = nb_real_init ? nb_real_init(self) : -1;
        NB_A_LOG("NATIVE-A initialize EXIT rc=%d self=%p", r, self);
        return r;
    }
}

int nb_impl_finalize(void *self)
{
    (void)pthread_once(&nb_real_once, nb_resolve_real);
    if (nb_is_b(self)) { ++g_nb_fini_calls; return 0; }     /* never kdTerminateNV for B */
    {
        int r;
        nb_close_b_for_a("finalize");
        g_nb_a_started = 0;
        NB_A_LOG("NATIVE-A finalize ENTER self=%p", self);
        r = nb_real_fini ? nb_real_fini(self) : -1;
        NB_A_LOG("NATIVE-A finalize EXIT rc=%d self=%p", r, self);
        return r;
    }
}

int nb_impl_configure(void *self, unsigned a, unsigned b, unsigned c, unsigned d, unsigned e, unsigned f, unsigned g)
{
    (void)pthread_once(&nb_real_once, nb_resolve_real);
    if (nb_is_b(self)) {
        g_nb_conf[0]=a; g_nb_conf[1]=b; g_nb_conf[2]=c; g_nb_conf[3]=d; g_nb_conf[4]=e; g_nb_conf[5]=f; g_nb_conf[6]=g;
        g_nb_conf_seen = 1;
        if (g_nb_disp && nb_real_conf) {
            /* Stock passes the main-screen properties (1024x480); B's real stream is the alt display size. */
            int r = nb_real_conf(self, (unsigned)g_nb_cfg_w, (unsigned)g_nb_cfg_h, (unsigned)g_nb_cfg_w, (unsigned)g_nb_cfg_h,
                                 (unsigned)g_nb_cfg_x, (unsigned)g_nb_cfg_y, g);
            logf_u2("NATIVE-B real configure %d %d %d %d %d %d %u (stock args %u %u %u %u %u %u %u) -> %d",
                    g_nb_cfg_w, g_nb_cfg_h, g_nb_cfg_w, g_nb_cfg_h, g_nb_cfg_x, g_nb_cfg_y, g, a, b, c, d, e, f, g, r);
            return 0;   /* never fail the stream because of a display problem */
        }
        logf_u2("NATIVE-B stub configure %u %u %u %u %u %u %u", a, b, c, d, e, f, g);
        return 0;
    }
    {
        int r;
        NB_A_LOG("NATIVE-A configure ENTER self=%p %u %u %u %u %u %u %u", self, a, b, c, d, e, f, g);
        r = nb_real_conf ? nb_real_conf(self, a, b, c, d, e, f, g) : -1;
        NB_A_LOG("NATIVE-A configure EXIT rc=%d self=%p", r, self);
        return r;
    }
}

int nb_impl_start(void *self)
{
    (void)pthread_once(&nb_real_once, nb_resolve_real);
    if (nb_is_b(self)) {
        ++g_nb_start_calls;
        if (g_nb_disp && nb_real_start) {
            int r, w;
            /* Open-order experiment: let stock A open NvSS first, as it always does without B. */
            for (w = 0; w < 250 && !g_nb_a_started; ++w) usleep(20000);
            logf_u2("NATIVE-B start: stock A started=%d after %d ms wait", g_nb_a_started, w * 20);
            pthread_mutex_lock(&g_nb_open_lock);
            g_nb_bstart_thread = pthread_self();
            g_nb_in_bstart = 1;
            r = nb_real_start(self);
            g_nb_in_bstart = 0;
            pthread_mutex_unlock(&g_nb_open_lock);
            g_nb_real_open = (r == 0);
            logf_u2("NATIVE-B real start -> %d (%s)", r, r == 0 ? "NvSS open" : "FAILED; B falls back to capture-only");
            return 0;
        }
        logf_u2("NATIVE-B stub start (NvSSVideoOpen NOT called)");
        return 0;
    }
    {
        int r;
        NB_A_LOG("NATIVE-A start ENTER self=%p", self);
        r = nb_real_start ? nb_real_start(self) : -1;
        if (r == 0) g_nb_a_started = 1;
        NB_A_LOG("NATIVE-A start EXIT rc=%d self=%p", r, self);
        return r;
    }
}

int nb_impl_stop(void *self)
{
    (void)pthread_once(&nb_real_once, nb_resolve_real);
    if (nb_is_b(self)) {
        ++g_nb_stop_calls;
        pthread_mutex_lock(&g_nb_run_lock);
        if (g_nb_disp && g_nb_real_open && nb_real_stop) {
            int r = nb_real_stop(self);
            g_nb_real_open = 0;
            logf_u2("NATIVE-B real stop (NvSSVideoClose) -> %d", r);
        }
        pthread_mutex_unlock(&g_nb_run_lock);
        return 0;
    }
    {
        int r;
        nb_close_b_for_a("stop");
        g_nb_a_started = 0;
        NB_A_LOG("NATIVE-A stop ENTER self=%p", self);
        r = nb_real_stop ? nb_real_stop(self) : -1;
        NB_A_LOG("NATIVE-A stop EXIT rc=%d self=%p", r, self);
        return r;
    }
}

int nb_impl_decode(void *self, uint8_t *data, uint32_t len, uint64_t ts, uint8_t flag)
{
    if (nb_is_b(self)) {
        uint32_t n = ++g_nb_decode_calls;
        int r = 0;
        nb_capture_au(data, len);
        if (g_nb_fwd) nb_fwd_send(data, len, ts, (uint32_t)flag);
        if (g_nb_disp && g_nb_real_open) {
            (void)pthread_once(&nb_real_once, nb_resolve_real);
            pthread_mutex_lock(&g_nb_run_lock);
            if (g_nb_real_open && nb_real_dec) r = nb_real_dec(self, data, len, ts, flag);
            pthread_mutex_unlock(&g_nb_run_lock);
        }
        if (n <= 3 || (n % 300u) == 0) { nb_write_status("streaming"); logf_u2("NATIVE-B decode#%u len=%u real_rc=%d", n, len, r); }
        (void)ts; (void)flag;
        return 0;     /* never let a display error tear the stream down */
    }
    (void)pthread_once(&nb_real_once, nb_resolve_real);
    return nb_real_dec ? nb_real_dec(self, data, len, ts, flag) : -1;
}

int nb_impl_setvis(void *self, uint8_t v)
{
    (void)pthread_once(&nb_real_once, nb_resolve_real);
    if (nb_is_b(self)) {
        pthread_mutex_lock(&g_nb_run_lock);
        if (g_nb_disp && g_nb_real_open && nb_real_vis) (void)nb_real_vis(self, v);
        pthread_mutex_unlock(&g_nb_run_lock);
        return 0;
    }
    {
        int r;
        NB_A_LOG("NATIVE-A setVisibility(%u) ENTER self=%p", (unsigned)v, self);
        r = nb_real_vis ? nb_real_vis(self, v) : -1;
        NB_A_LOG("NATIVE-A setVisibility EXIT rc=%d self=%p", r, self);
        return r;
    }
}

/* NvSSVideoOpen interposer (imported by libairplay).  Only B's open (flag + thread) is rewritten;
 * every other call is forwarded untouched.  params layout recovered from NvssVideoImpl::start:
 *   +0x00 u32 outputDevice, +0x08 u8 (0x64), +0x09 u8 (1), +0x0a u8 layerID(displayable id), +0x10 u32 must be 0 */
uint32_t nb_NvSSVideoOpen(void *handle, const void *params) __asm__("NvSSVideoOpen");
uint32_t nb_NvSSVideoOpen(void *handle, const void *params)
{
    if (!nb_real_open) nb_real_open = (nb_real_open_t)dlsym(RTLD_NEXT, "NvSSVideoOpen");
    if (!nb_real_open) return 0x80000001u;
    if (g_nb_in_bstart && pthread_equal(g_nb_bstart_thread, pthread_self()) && params) {
        uint8_t copy[0x18];
        uint32_t od0, od;
        memcpy(copy, params, sizeof(copy));
        memcpy(&od0, copy, 4);
        logf_u2("NATIVE-B NvSSVideoOpen rewrite: outputDevice %u -> %d, layerID %u -> %d",
                (unsigned)od0, g_nb_cfg_outdev, (unsigned)copy[0x0a], g_nb_cfg_layer);
        od = (uint32_t)g_nb_cfg_outdev; memcpy(copy, &od, 4);
        copy[0x0a] = (uint8_t)g_nb_cfg_layer;
        return nb_real_open(handle, copy);
    }
    return nb_real_open(handle, params);
}

/* NvSSVideoClose interposer: logging only, forwards untouched. */
typedef uint32_t (*nb_real_close_t)(void *);
static nb_real_close_t nb_real_close;
uint32_t nb_NvSSVideoClose(void *handle) __asm__("NvSSVideoClose");
uint32_t nb_NvSSVideoClose(void *handle)
{
    uint32_t r;
    if (!nb_real_close) nb_real_close = (nb_real_close_t)dlsym(RTLD_NEXT, "NvSSVideoClose");
    if (!nb_real_close) return 0x80000001u;
    NB_A_LOG("NvSSVideoClose ENTER handle=%p", handle);
    r = nb_real_close(handle);
    NB_A_LOG("NvSSVideoClose EXIT handle=%p rc=0x%x", handle, (unsigned)r);
    return r;
}

/* --------------------------------------------------------------- init / checks */

static void *nb_sym(const char *name)
{
    void *p = dlsym(RTLD_NEXT, name);
    if (!p) p = dlsym(RTLD_DEFAULT, name);
    if (!p) logf_u2("NATIVE-B missing symbol %s", name);
    return p;
}

static uint32_t nb_word(uintptr_t a) { return *(volatile uint32_t *)a; }

/* Returns 1 when every ABI assumption holds.  Called on each B start (cheap). */
static int nb_check_abi(void)
{
    uintptr_t base;
    void *sc = dlsym(RTLD_NEXT, "AirPlayReceiverSessionScreen_Create");
    void *ssc = dlsym(RTLD_NEXT, "ScreenStreamCreate");
    uintptr_t *vt;
    if (!sc || !ssc) return 0;
    base = (uintptr_t)sc - NB_OFF_SCREEN_CREATE;
    if ((uintptr_t)ssc != base + NB_OFF_SCREENSTREAMCR) {
        logf_u2("NATIVE-B ABI mismatch: ScreenStreamCreate=%p expected %p", ssc, (void *)(base + NB_OFF_SCREENSTREAMCR));
        return 0;
    }
    /* prologue words recorded from the analysed library */
    if (nb_word(base + NB_OFF_SCREENTHREAD) != 0xe92d41f0u ||
        nb_word(base + NB_OFF_TS_UPTICKS)   != 0xe30113c8u ||
        nb_word(base + NB_OFF_TS_NTP)       != 0xe30133c8u ||
        nb_word(base + NB_OFF_SCREENSTREAMCR) != 0xe92d41f0u ||
        nb_word(base + NB_OFF_SCREEN_CREATE)  != 0xe92d4070u) {
        logf_u2("NATIVE-B ABI mismatch: prologue words differ (different libairplay build?)");
        return 0;
    }
    g_nbabi.base = base;
    /* interposition proof: vtable slots must point at OUR wrappers */
    vt = (uintptr_t *)(base + NB_OFF_VPTR);
    if (vt[NB_SLOT_INIT]   != (uintptr_t)nb_impl_initialize ||
        vt[NB_SLOT_FINI]   != (uintptr_t)nb_impl_finalize ||
        vt[NB_SLOT_CONF]   != (uintptr_t)nb_impl_configure ||
        vt[NB_SLOT_START]  != (uintptr_t)nb_impl_start ||
        vt[NB_SLOT_STOP]   != (uintptr_t)nb_impl_stop ||
        vt[NB_SLOT_DECODE] != (uintptr_t)nb_impl_decode ||
        vt[NB_SLOT_SETVIS] != (uintptr_t)nb_impl_setvis) {
        logf_u2("NATIVE-B interposition NOT effective (vtable slots init=%p start=%p decode=%p); refusing B",
                (void *)vt[NB_SLOT_INIT], (void *)vt[NB_SLOT_START], (void *)vt[NB_SLOT_DECODE]);
        return 0;
    }
    return 1;
}

static int nb_init_abi(void)
{
    if (g_nbabi.ok) return 1;
    g_nbabi.screen_create    = (nb_screen_create_t)nb_sym("AirPlayReceiverSessionScreen_Create");
    g_nbabi.seteventhandler  = (nb_screen_seteventhandler_t)nb_sym("AirPlayReceiverSessionScreen_SetEventHandler");
    g_nbabi.updatestate      = (nb_screen_updatestate_t)nb_sym("AirPlayReceiverSessionScreen_UpdateState");
    g_nbabi.setup            = (nb_screen_setup_t)nb_sym("AirPlayReceiverSessionScreen_Setup");
    g_nbabi.setsecurity      = (nb_screen_setsecurity_t)nb_sym("AirPlayReceiverSessionScreen_SetSecurityInfo");
    g_nbabi.setuuid          = (nb_screen_setuuid_t)nb_sym("AirPlayReceiverSessionScreen_SetSessionUUID");
    g_nbabi.setdevid         = (nb_screen_setdevid_t)nb_sym("AirPlayReceiverSessionScreen_SetClientDeviceID");
    g_nbabi.settimesync      = (nb_screen_settimesync_t)nb_sym("AirPlayReceiverSessionScreen_SetTimeSynchronizer");
    g_nbabi.setrecvend       = (nb_screen_settime_t)nb_sym("AirPlayReceiverSessionScreen_SetReceiveEndTime");
    g_nbabi.setauthend       = (nb_screen_settime_t)nb_sym("AirPlayReceiverSessionScreen_SetAuthEndTime");
    g_nbabi.setntpend        = (nb_screen_settime_t)nb_sym("AirPlayReceiverSessionScreen_SetNTPEndTime");
    g_nbabi.startsession     = (nb_screen_startsession_t)nb_sym("AirPlayReceiverSessionScreen_StartSession");
    g_nbabi.processframes    = (nb_screen_processframes_t)nb_sym("AirPlayReceiverSessionScreen_ProcessFrames");
    g_nbabi.stopsession      = (nb_screen_void_t)nb_sym("AirPlayReceiverSessionScreen_StopSession");
    g_nbabi.screen_delete    = (nb_screen_void_t)nb_sym("AirPlayReceiverSessionScreen_Delete");
    g_nbabi.serversocketopen = (nb_serversocketopen_t)nb_sym("ServerSocketOpen");
    g_nbabi.socketaccept     = (nb_socketaccept_t)nb_sym("SocketAccept");
    g_nbabi.netsocket_create = (nb_netsocket_create_t)nb_sym("NetSocket_CreateWithNative");
    g_nbabi.netsocket_delete = (nb_netsocket_delete_t)nb_sym("NetSocket_Delete");
    g_nbabi.cftime           = (nb_cftime_t)nb_sym("CFAbsoluteTimeGetCurrent");
    if (!g_nbabi.screen_create || !g_nbabi.seteventhandler || !g_nbabi.updatestate || !g_nbabi.setup ||
        !g_nbabi.setsecurity || !g_nbabi.setuuid || !g_nbabi.setdevid || !g_nbabi.settimesync ||
        !g_nbabi.setrecvend || !g_nbabi.setauthend || !g_nbabi.setntpend || !g_nbabi.startsession ||
        !g_nbabi.processframes || !g_nbabi.stopsession || !g_nbabi.screen_delete ||
        !g_nbabi.serversocketopen || !g_nbabi.socketaccept || !g_nbabi.netsocket_create ||
        !g_nbabi.netsocket_delete || !g_nbabi.cftime)
        return 0;
    g_nbabi.ok = 1;
    return 1;
}

/* B's screen event handler: log only.  The stock handler drives session commands; B must not. */
static void nb_event(void *screen, CFTypeRef ev, CFTypeRef params, void *ctx)
{
    (void)screen; (void)ev; (void)params; (void)ctx;
    logf_u2("NATIVE-B screen event received");
}

/* ------------------------------------------------------------------- B thread */

static void *nb_thread(void *arg)
{
    struct nb_ctx *c = (struct nb_ctx *)arg;
    int client = -1, i;
    void *ns = NULL;
    OSStatus e = -1;
    char *sess = (char *)c->session;
    uintptr_t ts[3];

    for (i = 0; i < NB_ACCEPT_SLICES && !c->stop; ++i) {
        e = g_nbabi.socketaccept(c->listen_fd, 1, &client, NULL);
        if (e == 0) break;
        if (e != -6722) { logf_u2("NATIVE-B accept error %d", (int)e); break; }   /* -6722 = timeout slice */
    }
    if (e != 0 || c->stop) {
        logf_u2("NATIVE-B no iPhone connection (accept e=%d stop=%d)", (int)e, c->stop);
        nb_write_status(c->stop ? "stopped" : "no_connection");
        goto out_nolink;
    }
    logf_u2("NATIVE-B iPhone connected on stream-111 port %d fd=%d", c->port, client);
    c->client_fd = client;
    close(c->listen_fd); c->listen_fd = -1;

    g_nbabi.setuuid(c->screen, sess + NB_SESS_UUID);
    g_nbabi.setdevid(c->screen, *(uint64_t *)(sess + NB_SESS_DEVID));
    if (g_nbabi.netsocket_create(&ns, client) != 0 || !ns) {
        logf_u2("NATIVE-B NetSocket_CreateWithNative failed");
        goto out_nolink;
    }
    client = -1;   /* owned by ns */

    ts[0] = (uintptr_t)c->session;
    ts[1] = g_nbabi.base + NB_OFF_TS_UPTICKS;
    ts[2] = g_nbabi.base + NB_OFF_TS_NTP;
    g_nbabi.settimesync(c->screen, ts);
    g_nbabi.setrecvend(c->screen, g_nbabi.cftime());
    g_nbabi.setauthend(c->screen, g_nbabi.cftime());
    g_nbabi.setntpend(c->screen, g_nbabi.cftime());

    /* StartSession creates ScreenStream B and (via ScreenStreamStart) calls our stub methods. */
    g_nb_start_thread = pthread_self();
    g_nb_starting = 1;
    e = g_nbabi.startsession(c->screen, (CFDictionaryRef)c->server_cfg);
    g_nb_starting = 0;
    if (e != 0 || !g_nb_init_calls) {
        logf_u2("NATIVE-B StartSession failed e=%d stub_inits=%d", (int)e, g_nb_init_calls);
        nb_write_status("startsession_failed");
    } else {
        logf_u2("NATIVE-B ScreenStream B started; entering ProcessFrames");
        nb_write_status("processing");
        e = g_nbabi.processframes(c->screen, ns, c->server_frames);
        logf_u2("NATIVE-B ProcessFrames returned %d", (int)e);
    }
    g_nbabi.stopsession(c->screen);
    nb_write_status("ended");

out_nolink:
    if (ns) g_nbabi.netsocket_delete(ns);
    else if (client >= 0) close(client);
    c->client_fd = -1;
    if (c->listen_fd >= 0) { close(c->listen_fd); c->listen_fd = -1; }
    if (c->screen) g_nbabi.screen_delete(c->screen);
    nb_unregister_all();
    nb_close_capture();
    g_nb_fwd_session = NULL;
    nb_fwd_close();
    c->done = 1;
    return NULL;
}

/* -------------------------------------------------------------------- start/stop */

static void nb_stop(void)
{
    struct nb_ctx *c;
    int i;
    pthread_mutex_lock(&g_nb_lock);
    c = g_nb_ctx;
    g_nb_ctx = NULL;
    pthread_mutex_unlock(&g_nb_lock);
    if (!c) return;
    c->stop = 1;
    if (c->client_fd >= 0) (void)shutdown(c->client_fd, SHUT_RDWR);
    /* The worker owns and frees everything; wait a bounded time so the AirPlay session is
     * not destroyed underneath the time-synchronizer callbacks. */
    for (i = 0; i < 40 && !c->done; ++i) usleep(100000);
    if (!c->done) { logf_u2("NATIVE-B worker did not finish within 4s; leaving it detached"); return; }
    free(c);
    logf_u2("NATIVE-B stopped");
}

/* r25-autostart: start vc_render (if not already running) and switch cluster display 4 to
 * context 77, so nothing has to be typed after a restart. Renderer binary: /mnt/app/root/vc_render
 * (persistent) else /tmp/vc_render. Marker /mnt/app/root/mibr-carplay111-noautostart disables it.
 * Runs in a detached shell (spawn, P_NOWAITO); the forward link retries until the renderer is up. */
static void r25_run(int restore)
{
    char script[2800];
    char *argv[4];
    int pid;
    int up_s = (int)(nb_now_ms() / 1000ull);
    (void)up_s;
    if (access("/mnt/app/root/mibr-carplay111-noautostart", F_OK) == 0) {
        logf_u2("r25-autostart disabled by marker");
        return;
    }
    /* r35-restore: phone gone -> put the cluster back on the native map (context 72 = KOMBI_MAP_VIEW) */
    /* r44-watcher: mode 1 = a standalone watcher process (own shell, not a child of the autostart script).
     * Phone gone (a new whole-session TEARDOWN line in the hook log) -> native map (context 72). */
    if (restore)
        snprintf(script, sizeof(script),
            "D=/eso/bin/apps/dmdt; export LD_LIBRARY_PATH=/eso/lib IPL_CONFIG_DIR=/etc/eso/production; "
            "[ -e /tmp/r44-watch.pid ] && kill -0 $(cat /tmp/r44-watch.pid) 2>/dev/null && exit 0; echo $$ > /tmp/r44-watch.pid; "
            "L=/tmp/altscreen111.log; c=$(grep -c 'TEARDOWN entry request=NULL' $L 2>/dev/null); "
            "while :; do sleep 1; k=$(grep -c 'TEARDOWN entry request=NULL' $L 2>/dev/null); "
            "if [ $k -gt $c ]; then $D sc 4 72 >> /tmp/r25-autostart.log 2>&1; echo watch-teardown72 >> /tmp/r25-autostart.log; fi; c=$k; done");
    else
    snprintf(script, sizeof(script),
        "R=/mnt/app/root/vc_render; [ -x $R ] || R=/tmp/vc_render; "
        "export LD_LIBRARY_PATH=/mnt/app/armle/lib:/mnt/app/armle/usr/lib:/mnt/app/root/lib-target:/eso/lib:/armle/lib:/armle/lib/dll:/armle/usr/lib:/lib:/usr/lib; "
        /* r34-nowait: start the renderer immediately (no wait for navigation), keep re-asserting the
         * display switch every 3 s, and when the SECOND 'Change NavStateOfOperation to 5' appears
         * (navigation finished, which can reset the cluster) re-acquire the phone UI and switch again. */
        "pidin 2>/dev/null | grep vc_render >/dev/null || on -p 25 $R --h %d --ts wall > /tmp/vc_render.log 2>&1 & "
        "D=/eso/bin/apps/dmdt; export LD_LIBRARY_PATH=/eso/lib IPL_CONFIG_DIR=/etc/eso/production; "
        /* r42-gate: phone present = more session starts than teardowns in the hook log; never re-assert 77 once the phone is gone */
        "P(){ [ $(grep -c 'r25-autostart: spawn' /tmp/altscreen111.log 2>/dev/null) -gt $(grep -c 'TEARDOWN entry request=NULL' /tmp/altscreen111.log 2>/dev/null) ]; }; "
        /* r43-nav-ready-fast: navigation already up (replug / late plug) -> NO phone re-acquire (two of them left the phone at 16 fps) */
        "[ $(sloginfo 2>/dev/null | grep -c 'Change NavStateOfOperation to 5') -ge 2 ] || { sleep 3; P && touch /tmp/mibr-alt111-gen2-reacquire; }; "
        "i=0; n=0; while [ $i -lt 120 ]; do { [ $i -eq 0 ] || P; } && $D sc 4 77 >> /tmp/r25-autostart.log 2>&1; "
        "[ $(sloginfo 2>/dev/null | grep -c 'Change NavStateOfOperation to 5') -ge 2 ] && { n=1; break; }; "
        "sleep 3; i=$((i+1)); done; "
        "[ $n -eq 1 ] && { echo navready >> /tmp/r25-autostart.log; "
        "if [ $i -eq 0 ]; then for w in 1 2 3; do sleep $w; P && $D sc 4 77 >> /tmp/r25-autostart.log 2>&1; done; "
        "else sleep 3; P && touch /tmp/mibr-alt111-gen2-reacquire; "
        "for w in 4 8 20; do sleep $w; P && $D sc 4 77 >> /tmp/r25-autostart.log 2>&1; done; fi; }",
        (int)g_height);
    argv[0] = (char *)"sh"; argv[1] = (char *)"-c"; argv[2] = script; argv[3] = NULL;
    /* r27-cloexec: the child must not inherit dio_manager's descriptors (the hook's listening
     * sockets 19820/19821 would stay bound after a dio_manager restart) nor its environment.
     * Only spawnv/spawnve exist in this toolchain, so mark every open fd >= 3 close-on-exec just
     * around the spawn and restore the original flags afterwards. */
    {
        static char *envp[] = {
            (char *)"PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/bin:/mnt/app/armle/usr/sbin",
            (char *)"IPL_CONFIG_DIR=/etc/eso/production",
            NULL };
        unsigned char was[1024];
        int fd, f;
        for (fd = 3; fd < 1024; ++fd) {
            f = fcntl(fd, F_GETFD);
            was[fd] = (f < 0) ? 0 : ((f & FD_CLOEXEC) ? 2 : 1);
            if (was[fd] == 1) (void)fcntl(fd, F_SETFD, f | FD_CLOEXEC);
        }
        pid = spawnve(P_NOWAITO, "/bin/sh", argv, envp);
        for (fd = 3; fd < 1024; ++fd) {
            if (was[fd] != 1) continue;
            f = fcntl(fd, F_GETFD);
            if (f >= 0) (void)fcntl(fd, F_SETFD, f & ~FD_CLOEXEC);
        }
    }
    logf_u2("%s: spawn pid=%d h=%d", restore ? "r35-restore" : "r25-autostart", pid, (int)g_height);
}

/* r38-watch: no process spawn in the teardown path (it killed dio_manager). The hook only creates/removes
 * a marker; a watcher shell started with the autostart reacts to it. */
static void r38_gone_marker(int gone)
{
    /* r39-log */
    if (gone) { int fd = open("/tmp/mibr-alt111-phone-gone", O_WRONLY | O_CREAT, 0644); int e = errno; logf_u2("r39 phone-gone marker create fd=%d errno=%d", fd, e); if (fd >= 0) close(fd); }
    else { int rc = unlink("/tmp/mibr-alt111-phone-gone"); logf_u2("r39 phone-gone marker clear rc=%d", rc); }
}
static int g_r44_started;
static void r25_autostart(void)
{
    r38_gone_marker(0);
    r25_run(0);
    if (!g_r44_started) { g_r44_started = 1; r25_run(1); }
}
static void r35_restore_native_map(void) { r38_gone_marker(1); }

/* Returns dataPort (>0) or -1 (fail-soft, caller falls back to "no alt display" response). */
static int nb_start(AirPlayReceiverSessionRef session, CFDictionaryRef altDesc, uint64_t cid)
{
    struct nb_ctx *c = NULL;
    uint8_t key[16], iv[16];
    void *screen = NULL;
    void *server;
    int fd = -1, port = 0;
    OSStatus e;

    nb_stop();

    g_nb_disp = (access(NB_DISP_MARKER, F_OK) == 0);
    g_nb_real_open = 0;
    g_nb_fwd = (access(NB_FWD_MARKER, F_OK) == 0);
    if (g_nb_fwd) {
        g_nb_disp = 0;   /* no NvSS inside dio_manager in forward mode */
        g_nb_fwd_retry_ms = 0; g_nb_fwd_sent = 0; g_nb_fwd_drop = 0;
        g_nb_fwd_session = session;
        g_nb_fwd_fk = (nb_fk_t)nb_sym("AirPlayReceiverSessionForceKeyFrame");
        g_nb_fwd_need_idr = 0;
        logf_u2("NATIVE-B FORWARD MODE ON: frames go to vc_render on 127.0.0.1:%d (no NvSS in dio_manager)", NB_FWD_PORT);
        r25_autostart();
    }
    if (g_nb_disp) {
        FILE *cf = fopen(NB_DISP_CFG, "r");
        int v[6];
        if (cf) {
            if (fscanf(cf, "%d %d %d %d %d %d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6 &&
                v[0] >= 0 && v[0] <= 1 && v[1] > 0 && v[1] < 256 && v[2] > 0 && v[2] <= 4096 && v[3] > 0 && v[3] <= 4096) {
                g_nb_cfg_outdev = v[0]; g_nb_cfg_layer = v[1]; g_nb_cfg_w = v[2]; g_nb_cfg_h = v[3];
                g_nb_cfg_x = v[4]; g_nb_cfg_y = v[5];
            } else logf_u2("NATIVE-B display cfg invalid; using defaults");
            fclose(cf);
        }
        logf_u2("NATIVE-B DISPLAY MODE ON: outputDevice=%d layerID=%d size=%dx%d offset=%d,%d",
                g_nb_cfg_outdev, g_nb_cfg_layer, g_nb_cfg_w, g_nb_cfg_h, g_nb_cfg_x, g_nb_cfg_y);
    }

    if (!nb_init_abi() || !nb_check_abi()) { nb_write_status("abi_refused"); return -1; }
    (void)pthread_once(&nb_real_once, nb_resolve_real);

    pthread_mutex_lock(&g_lock);
    if (!g_master_valid) {
        pthread_mutex_unlock(&g_lock);
        logf_u2("NATIVE-B: no captured session AES key; cannot start");
        return -1;
    }
    g_derive_screen(g_master_key, 16, cid, key, iv);
    pthread_mutex_unlock(&g_lock);

    server = *(void **)((char *)session + NB_SESS_SERVER);
    if (!server) { logf_u2("NATIVE-B: session server object missing"); memset(key,0,16); memset(iv,0,16); return -1; }

    c = (struct nb_ctx *)calloc(1, sizeof(*c));
    if (!c) { memset(key,0,16); memset(iv,0,16); return -1; }
    c->session = session;
    c->server_cfg = *(void **)((char *)server + NB_SRV_SCREENCFG);
    c->server_frames = *(void **)((char *)server + NB_SRV_FRAMESCFG);
    c->listen_fd = -1; c->client_fd = -1;

    e = g_nbabi.screen_create(&screen);
    if (e != 0 || !screen) { logf_u2("NATIVE-B Screen_Create failed %d", (int)e); goto fail; }
    c->screen = screen;
    g_nbabi.seteventhandler(screen, nb_event, session, NULL);
    g_nbabi.updatestate(screen, 1);
    e = g_nbabi.setup(screen, altDesc, NULL);
    if (e != 0) { logf_u2("NATIVE-B Screen_Setup failed %d", (int)e); goto fail; }
    e = g_nbabi.setsecurity(screen, key, iv);
    memset(key, 0, sizeof(key)); memset(iv, 0, sizeof(iv));
    if (e != 0) { logf_u2("NATIVE-B SetSecurityInfo failed %d", (int)e); goto fail; }

    /* Same call the stock 110 path makes (family from session+0x51, TCP, preferred port, rcvbuf hint). */
    /*
     * The unit's pf ruleset only admits inbound TCP on carplay0 for 5000:5001, 5010, 6000, 6030, 7000:7001, 7100
     * (anything else is silently dropped). 6030 is stock stream 110. Try only allowed, normally idle ports and
     * accept a candidate only if the stock call really bound exactly that port.
     */
    {
        static const int cand[] = { 6000, 5010, 7100, 5001 };
        size_t i;
        port = 0; fd = -1;
        for (i = 0; i < sizeof(cand) / sizeof(cand[0]); i++) {
            int p2 = 0, f2 = -1;
            e = g_nbabi.serversocketopen((int)*(uint8_t *)((char *)session + NB_SESS_FAMILY), 1, 6,
                                         -cand[i], &p2, (int)0xfffd4000, &f2);
            if (e == 0 && f2 >= 0 && p2 == cand[i]) { port = p2; fd = f2; break; }
            logf_u2("NATIVE-B port %d unavailable (e=%d got=%d); trying next", cand[i], (int)e, p2);
            if (f2 >= 0) close(f2);
        }
        if (fd < 0) { logf_u2("NATIVE-B no pf-allowed port could be bound"); goto fail; }
    }
    c->listen_fd = fd; c->port = port;

    gen2_video_begin_current();
    g_nb_decode_calls = 0; g_nb_nal_sps = g_nb_nal_pps = g_nb_nal_idr = g_nb_nal_slice = 0;
    g_nb_cap_bytes = 0; g_nb_conf_seen = 0; g_nb_init_calls = g_nb_start_calls = g_nb_stop_calls = g_nb_fini_calls = 0;

    pthread_mutex_lock(&g_nb_lock);
    g_nb_ctx = c;
    pthread_mutex_unlock(&g_nb_lock);
    if (pthread_create(&c->th, NULL, nb_thread, c) != 0) {
        pthread_mutex_lock(&g_nb_lock); g_nb_ctx = NULL; pthread_mutex_unlock(&g_nb_lock);
        logf_u2("NATIVE-B pthread_create failed");
        goto fail;
    }
    pthread_detach(c->th);
    nb_write_status("listening");
    logf_u2("NATIVE-B Screen B ready: cid=%llu dataPort=%d family=%d (native ServerSocketOpen, stub video sink)",
            (unsigned long long)cid, port, (int)*(uint8_t *)((char *)session + NB_SESS_FAMILY));
    return port;

fail:
    memset(key, 0, sizeof(key)); memset(iv, 0, sizeof(iv));
    if (fd >= 0) close(fd);
    if (screen) g_nbabi.screen_delete(screen);
    free(c);
    nb_write_status("start_failed");
    return -1;
}

static int nb_marker(void) { return access(NB_MARKER, F_OK) == 0; }
