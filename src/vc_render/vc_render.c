/*
 * vc_render -- standalone Virtual Cockpit renderer for the CarPlay AltScreen (stream 111).
 *
 * Why this exists: opening a second NvSS video instance inside dio_manager makes the stock
 * main-screen NvSSVideoClose hang when you leave CarPlay. This process owns the cluster's
 * NvSS instance instead, so dio_manager never holds more than the stock one.
 *
 * Input : TCP 127.0.0.1:<port> (default 19830).  The hook (libaltscreen111, forward mode) connects
 *         and sends frames:  u32 magic 'AV11' | u32 len | u64 ts | u32 flag | len bytes (Annex-B AU).
 *         All integers little-endian.
 * Output: NvSS video on <outputDevice> / layer <layer> (defaults 1 / 58 = cluster stream displayable).
 *
 * NvSS call sequence recovered from the stock NvssVideoImpl in libairplay (MU0678 build):
 *   kdInitializeNV(); NvSSVideoOpen(&h, params[0x18]); NvSSVideoStreamConfigure(h, {w,h,fps_f32});
 *   NvSSVideoGetAttribs(h, buf[0x40]); NvSSVideoDecode(h, {ptr,len,pad,ts64,flag}, 0); NvSSVideoClose(h).
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define VR_MAGIC 0x31315641u /* 'AV11' little-endian */
#define VR_MAX_AU (2u * 1024u * 1024u)
#define VR_LOG "/tmp/vc_render.log"

typedef int      (*kd_init_t)(void);
typedef uint32_t (*nv_open_t)(void **h, const void *params);
typedef uint32_t (*nv_cfg_t)(void *h, const void *cfg);
typedef uint32_t (*nv_attr_t)(void *h, void *out);
typedef uint32_t (*nv_dec_t)(void *h, const void *in, void *unused);
typedef uint32_t (*nv_close_t)(void *h);

static kd_init_t  p_kdinit;
static nv_open_t  p_open;
static nv_cfg_t   p_cfg;
static nv_attr_t  p_attr;
static nv_dec_t   p_dec;
static nv_close_t p_close;

static volatile sig_atomic_t g_quit;
static void *g_h;                 /* current NvSS handle */
static int g_out = 1, g_layer = 58, g_w = 1440, g_hgt = 456, g_fps = 30, g_port = 19830;
static uint32_t g_frames, g_dec_err, g_slow;
static int g_ts_mode;   /* 0 raw, 1 wall clock (us), 2 zero */
static uint32_t w_n;
static uint64_t w_bytes, w_ts0, w_ts1;
static long w_sum_us, w_max_us, w_t0ms;

static void logf_(const char *fmt, ...)
{
    struct timespec ts;
    FILE *f = fopen(VR_LOG, "a");
    va_list ap;
    if (!f) return;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    fprintf(f, "%ld.%03ld [vc_render] ", (long)ts.tv_sec, ts.tv_nsec / 1000000L);
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

static void on_sig(int s) { (void)s; g_quit = 1; }

static int load_libs(void)
{
    void *kd = dlopen("libKD.so", RTLD_NOW | RTLD_GLOBAL);
    void *nv = dlopen("libnvss_video.so", RTLD_NOW | RTLD_GLOBAL);
    if (!kd) logf_("dlopen libKD.so failed: %s", dlerror());
    if (!nv) { logf_("dlopen libnvss_video.so failed: %s", dlerror()); return -1; }
    p_kdinit = (kd_init_t)dlsym(kd ? kd : RTLD_DEFAULT, "kdInitializeNV");
    if (!p_kdinit) p_kdinit = (kd_init_t)dlsym(RTLD_DEFAULT, "kdInitializeNV");
    p_open  = (nv_open_t)dlsym(nv, "NvSSVideoOpen");
    p_cfg   = (nv_cfg_t)dlsym(nv, "NvSSVideoStreamConfigure");
    p_attr  = (nv_attr_t)dlsym(nv, "NvSSVideoGetAttribs");
    p_dec   = (nv_dec_t)dlsym(nv, "NvSSVideoDecode");
    p_close = (nv_close_t)dlsym(nv, "NvSSVideoClose");
    if (!p_kdinit || !p_open || !p_cfg || !p_attr || !p_dec || !p_close) {
        logf_("missing symbols kd=%p open=%p cfg=%p attr=%p dec=%p close=%p",
              (void *)p_kdinit, (void *)p_open, (void *)p_cfg, (void *)p_attr, (void *)p_dec, (void *)p_close);
        return -1;
    }
    return 0;
}

static int nv_start(void)
{
    uint8_t params[0x18];
    uint32_t cfg[3];
    uint8_t attr[0x40];
    float fps = (float)g_fps;
    uint32_t r;

    memset(params, 0, sizeof(params));
    {
        uint32_t od = (uint32_t)g_out;
        memcpy(params + 0, &od, 4);
    }
    params[8] = 0x64;
    params[9] = 1;
    params[10] = (uint8_t)g_layer;
    {
        uint32_t eight = 8;
        memcpy(params + 0xc, &eight, 4);
    }
    g_h = NULL;
    r = p_open(&g_h, params);
    logf_("NvSSVideoOpen(outputDevice=%d layer=%d) -> 0x%x handle=%p", g_out, g_layer, (unsigned)r, g_h);
    if (r != 0 || !g_h) { g_h = NULL; return -1; }

    cfg[0] = (uint32_t)g_w;
    cfg[1] = (uint32_t)g_hgt;
    memcpy(&cfg[2], &fps, 4);
    r = p_cfg(g_h, cfg);
    logf_("NvSSVideoStreamConfigure(%dx%d@%d) -> 0x%x", g_w, g_hgt, g_fps, (unsigned)r);
    if (r != 0) { (void)p_close(g_h); g_h = NULL; return -1; }
    memset(attr, 0, sizeof(attr));
    r = p_attr(g_h, attr);
    logf_("NvSSVideoGetAttribs -> 0x%x", (unsigned)r);
    return 0;
}

static void nv_stop(void)
{
    if (g_h) {
        uint32_t r;
        logf_("NvSSVideoClose ENTER handle=%p frames=%u dec_err=%u", g_h, g_frames, g_dec_err);
        r = p_close(g_h);
        logf_("NvSSVideoClose EXIT rc=0x%x", (unsigned)r);
        g_h = NULL;
    }
}

static int read_full(int fd, void *buf, size_t n)
{
    uint8_t *p = (uint8_t *)buf;
    while (n) {
        ssize_t r = read(fd, p, n);
        if (r > 0) { p += r; n -= (size_t)r; continue; }
        if (r == 0) return -1;
        if (errno == EINTR) { if (g_quit) return -1; continue; }
        return -1;
    }
    return 0;
}

static void serve_client(int fd)
{
    uint8_t *au = (uint8_t *)malloc(VR_MAX_AU);
    logf_("hook connected");
    g_frames = 0; g_dec_err = 0;
    if (!au) { logf_("oom"); return; }
    if (nv_start() != 0) logf_("NvSS start failed; discarding frames until the hook reconnects");
    while (!g_quit) {
        uint32_t hdr[5];   /* magic, len, ts_lo, ts_hi, flag */
        uint8_t din[0x28];
        uint64_t ts;
        if (read_full(fd, hdr, sizeof(hdr)) != 0) break;
        if (hdr[0] != VR_MAGIC || hdr[1] == 0 || hdr[1] > VR_MAX_AU) { logf_("bad frame header magic=0x%x len=%u", hdr[0], hdr[1]); break; }
        if (read_full(fd, au, hdr[1]) != 0) break;
        if (!g_h) continue;
        ts = ((uint64_t)hdr[3] << 32) | hdr[2];
        if (g_ts_mode == 1) {
            struct timespec tw;
            clock_gettime(CLOCK_MONOTONIC, &tw);
            ts = (uint64_t)tw.tv_sec * 1000000ull + (uint64_t)(tw.tv_nsec / 1000L);
        } else if (g_ts_mode == 2) ts = 0;
        memset(din, 0, sizeof(din));
        {
            uint32_t len = hdr[1];
            const uint8_t *ptr = au;
            memcpy(din + 0, &ptr, 4);
            memcpy(din + 4, &len, 4);
            memcpy(din + 8, &ts, 8);
            din[0x10] = (uint8_t)(hdr[4] & 0xffu);
        }
        {
            struct timespec t0, t1;
            long ms;
            uint32_t r;
            clock_gettime(CLOCK_MONOTONIC, &t0);
            r = p_dec(g_h, din, NULL);
            clock_gettime(CLOCK_MONOTONIC, &t1);
            ms = (long)((t1.tv_sec - t0.tv_sec) * 1000L + (t1.tv_nsec - t0.tv_nsec) / 1000000L);
            {
                long us = (long)((t1.tv_sec - t0.tv_sec) * 1000000L + (t1.tv_nsec - t0.tv_nsec) / 1000L);
                long nowms = (long)(t1.tv_sec * 1000L + t1.tv_nsec / 1000000L);
                if (!w_n) { w_t0ms = nowms; w_ts0 = ts; }
                ++w_n; w_bytes += hdr[1]; w_sum_us += us; if (us > w_max_us) w_max_us = us; w_ts1 = ts;
                if (nowms - w_t0ms >= 5000) {
                    long el = nowms - w_t0ms;
                    logf_("STATS %u frames in %ld ms = %ld fps; decode avg %ld us max %ld us; %u KB; ts span %llu",
                          w_n, el, (long)w_n * 1000L / el, w_sum_us / (long)w_n, w_max_us,
                          (unsigned)(w_bytes / 1024u), (unsigned long long)(w_ts1 - w_ts0));
                    w_n = 0; w_bytes = 0; w_sum_us = 0; w_max_us = 0;
                }
            }
            if (ms >= 80 && g_slow < 40) { ++g_slow; logf_("slow decode#%u len=%u flag=%u took %ld ms", g_frames + 1, hdr[1], hdr[4], ms); }
            ++g_frames;
            if (r != 0) ++g_dec_err;
            if (g_frames <= 3 || (g_frames % 300u) == 0) logf_("decode#%u len=%u rc=0x%x flag=%u", g_frames, hdr[1], (unsigned)r, hdr[4]);
        }
    }
    logf_("hook disconnected frames=%u dec_err=%u", g_frames, g_dec_err);
    nv_stop();
    free(au);
}

int main(int argc, char **argv)
{
    int ls, one = 1, i;
    struct sockaddr_in a;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--port") && i + 1 < argc) g_port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) g_out = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--layer") && i + 1 < argc) g_layer = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--w") && i + 1 < argc) g_w = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--h") && i + 1 < argc) g_hgt = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fps") && i + 1 < argc) g_fps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ts") && i + 1 < argc) {
            ++i;
            if (!strcmp(argv[i], "raw")) g_ts_mode = 0;
            else if (!strcmp(argv[i], "wall")) g_ts_mode = 1;
            else if (!strcmp(argv[i], "zero")) g_ts_mode = 2;
            else { fprintf(stderr, "--ts raw|wall|zero\n"); return 2; }
        }
        else { fprintf(stderr, "usage: %s [--port N] [--out 0|1] [--layer N] [--w N] [--h N] [--fps N]\n", argv[0]); return 2; }
    }
    if (g_out < 0 || g_out > 1 || g_layer <= 0 || g_layer > 255 || g_w <= 0 || g_hgt <= 0 || g_fps <= 0) { fprintf(stderr, "bad argument\n"); return 2; }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, on_sig);
    signal(SIGINT, on_sig);
    logf_("start out=%d layer=%d %dx%d@%d port=%d", g_out, g_layer, g_w, g_hgt, g_fps, g_port);
    if (load_libs() != 0) return 1;
    {
        int k = p_kdinit();
        logf_("kdInitializeNV -> %d", k);
    }

    ls = socket(AF_INET, SOCK_STREAM, 0);
    if (ls < 0) { logf_("socket: %s", strerror(errno)); return 1; }
    (void)setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)g_port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(ls, (struct sockaddr *)&a, sizeof(a)) != 0 || listen(ls, 1) != 0) { logf_("bind/listen %d: %s", g_port, strerror(errno)); return 1; }
    logf_("listening on 127.0.0.1:%d ts mode=%d (0 raw, 1 wall, 2 zero)", g_port, g_ts_mode);

    while (!g_quit) {
        int c = accept(ls, NULL, NULL);
        if (c < 0) { if (errno == EINTR) continue; logf_("accept: %s", strerror(errno)); usleep(200000); continue; }
        (void)setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        {
            int rb = 256 * 1024;
            (void)setsockopt(c, SOL_SOCKET, SO_RCVBUF, &rb, sizeof(rb));
        }
        serve_client(c);
        close(c);
    }
    nv_stop();
    close(ls);
    logf_("exit");
    return 0;
}
