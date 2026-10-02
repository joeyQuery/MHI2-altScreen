#define _GNU_SOURCE
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "alt111.h"

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/*
 * MU1440 CarPlay AltScreen Gen-2 vehicle candidate.
 *
 * This source deliberately forks the vehicle-proven Run143 handshake/crypto
 * boundary but replaces UI ownership and local-consumer synchronization with
 * the independently tested alt111 C99 core. Run143 remains untouched.
 *
 *
 * Target baseline (hard gate in installer):
 *   MHI2_ER_SKG13_P4526_MU1440
 *   /mnt/app/eso/lib/libairplay.so
 *   SHA256 193A4FD9101EC2AA05E7159CFA307B96500810D379CA74A194F172ADC13A46B5
 *
 * IRC-parity design:
 *  - Keep the exact MU1440 stock AirPlay implementation and its platform media ABI.
 *  - Replay the recovered MHI2Q IRC negotiation semantics around that stock core.
 *  - Advertise root enabledFeatures before stream 111 is selected.
 *  - Clone stock display[0], remove the reference-proven non-portable fields, and
 *    append a minimally modified AltScreen display without forcing display type=111.
 *  - Pass the original SETUP request to stock first; on 111, clone the requested
 *    stream descriptor and append dataPort + streamID=111 to the stock response.
 *  - Keep startup showUI/forceKeyFrame disabled unless parity-specific opt-in is set.
 *  - Prologue-hook stock Setup/Start/TearDown with the vehicle-proven MAP_FIXED
 *    private-page fallback because stock local binding bypasses plain interposition.
 *  - Reuse stock per-screen AES derivation/AES-CTR and receive stream 111 on TCP 6031.
 *  - Convert AVCC H.264 to Annex-B and expose it on 127.0.0.1:19820.
 *  - Main CarPlay stream 110 remains stock-owned.
 */

typedef const void *CFTypeRef;
typedef const void *CFStringRef;
typedef const void *CFDictionaryRef;
typedef void *CFMutableDictionaryRef;
typedef const void *CFArrayRef;
typedef void *CFMutableArrayRef;
typedef const void *CFDataRef;
typedef unsigned long CFTypeID;
typedef long CFIndex;
typedef int32_t OSStatus;
typedef unsigned char Boolean;
typedef void *AirPlayReceiverSessionRef;

#define K_NO_ERR 0
#define K_STREAM_ALT 111
#define K_SCREEN_HDR_SIZE 128
#define K_SCREEN_VIDEO_FRAME 0
#define K_SCREEN_VIDEO_CONFIG 1
#define K_SCREEN_KEEPALIVE 2
#define K_SCREEN_FORCE_KEYFRAME 3
#define K_SCREEN_IGNORE 4
#define K_SCREEN_KEEPALIVE_BODY 5
#define CF_UTF8 0x08000100u
#define MAX_SCREEN_BODY (8u * 1024u * 1024u)
#define ALT_UUID_DEFAULT "b7e6c5a0-2222-4000-8000-000000000002"
#define ALT_URL_DEFAULT  "maps:/car/instrumentcluster"
#define ALT_URL_MAP      "maps:/car/instrumentcluster/map"
#define ALT_URL_INSTRUCTIONCARD "maps:/car/instrumentcluster/instructioncard"
#define AIRPLAY_FEATURE_BIT26 (1ULL << 26)

/* Opaque storage. Stock AES_CTR_Context is smaller than this on the target. */
typedef union {
    uint64_t align;
    unsigned char bytes[512];
} aes_ctr_storage_t;

typedef struct {
    uint32_t bodySize;
    uint8_t opcode;
    uint8_t smallParam[3];
    uint8_t params[15 * 8];
} AirPlayScreenHeaderCompat;

typedef OSStatus (*fn_setup_t)(AirPlayReceiverSessionRef, CFDictionaryRef, CFDictionaryRef *);
typedef OSStatus (*fn_start_t)(AirPlayReceiverSessionRef, void *);
typedef void (*fn_teardown_t)(AirPlayReceiverSessionRef, CFDictionaryRef, OSStatus, Boolean *);
typedef CFDictionaryRef (*fn_serverinfo_t)(AirPlayReceiverSessionRef, CFArrayRef, uint8_t *, OSStatus *);
typedef void (*fn_command_completion_t)(OSStatus, CFDictionaryRef, void *);
typedef OSStatus (*fn_sendcmd_t)(AirPlayReceiverSessionRef, CFDictionaryRef, fn_command_completion_t, void *);
typedef OSStatus (*fn_platform_control_t)(AirPlayReceiverSessionRef, uint32_t, CFStringRef, CFTypeRef, CFDictionaryRef, CFDictionaryRef *);
typedef OSStatus (*fn_session_control_t)(AirPlayReceiverSessionRef, uint32_t, CFStringRef, CFTypeRef, CFDictionaryRef, CFDictionaryRef *);
typedef struct {
    int32_t screen;
    int32_t main_audio;
    int32_t phone;
    int32_t speech;
    int32_t speech_detail;
    int32_t turns;
} mibr_mode_state_t;
typedef OSStatus (*fn_make_mode_state_t)(AirPlayReceiverSessionRef, CFDictionaryRef, mibr_mode_state_t *);
typedef OSStatus (*fn_aes_cbc_init_t)(void *, const uint8_t[16], const uint8_t[16], Boolean);
typedef OSStatus (*fn_aes_ctr_init_t)(void *, const uint8_t[16], const uint8_t[16]);
typedef OSStatus (*fn_aes_ctr_update_t)(void *, const void *, size_t, void *);
typedef void (*fn_aes_ctr_final_t)(void *);
typedef void (*fn_derive_screen_t)(const void *, size_t, uint64_t, uint8_t[16], uint8_t[16]);

/* CFLite function types. */
typedef CFStringRef (*fn_cfstr_create_t)(void *, const char *, uint32_t);
typedef const char *(*fn_cfstr_cstr_ptr_t)(CFStringRef, uint32_t);
typedef Boolean (*fn_cfstr_cstr_t)(CFStringRef, char *, CFIndex, uint32_t);
typedef CFTypeID (*fn_cfget_typeid_t)(CFTypeRef);
typedef CFTypeID (*fn_cftypeid_t)(void);
typedef int64_t (*fn_cfget_i64_t)(CFTypeRef, OSStatus *);
typedef Boolean (*fn_cfbool_get_t)(CFTypeRef);
typedef CFIndex (*fn_cfdict_count_t)(CFDictionaryRef);
typedef void (*fn_cfdict_keys_t)(CFDictionaryRef, const void **, const void **);
typedef CFTypeRef (*fn_cfdict_get_t)(CFDictionaryRef, CFTypeRef);
typedef void (*fn_cfdict_set_t)(CFMutableDictionaryRef, CFTypeRef, CFTypeRef);
typedef void (*fn_cfdict_remove_t)(CFMutableDictionaryRef, CFTypeRef);
typedef int64_t (*fn_cfdict_get_i64_t)(CFDictionaryRef, CFStringRef, OSStatus *);
typedef void (*fn_cfdict_set_i64_t)(CFMutableDictionaryRef, CFStringRef, int64_t);
typedef CFMutableDictionaryRef (*fn_cfdict_new_t)(void *, CFIndex, const void *, const void *);
typedef CFIndex (*fn_cfarr_count_t)(CFArrayRef);
typedef CFTypeRef (*fn_cfarr_get_t)(CFArrayRef, CFIndex);
typedef void (*fn_cfarr_append_t)(CFMutableArrayRef, CFTypeRef);
typedef CFMutableArrayRef (*fn_cfarr_new_t)(void *, CFIndex, const void *);
typedef CFMutableArrayRef (*fn_cfarr_copy_t)(void *, CFIndex, CFArrayRef);
typedef const uint8_t *(*fn_cfdata_ptr_t)(CFDataRef);
typedef CFIndex (*fn_cfdata_len_t)(CFDataRef);
typedef CFTypeRef (*fn_cfretain_t)(CFTypeRef);
typedef void (*fn_cfrelease_t)(CFTypeRef);

static fn_setup_t g_setup_trampoline;
static fn_start_t g_start_trampoline;
static fn_teardown_t g_teardown_trampoline;
static fn_platform_control_t g_platform_control_trampoline;
static fn_session_control_t g_session_control_trampoline;
/*
 * Preserve exact stock entry points separately from executable trampolines.
 * This guarantees a true fail-closed fallback when an inline hook cannot be
 * installed: externally-preempted calls still delegate to stock instead of
 * dereferencing a NULL trampoline.
 */
static fn_setup_t g_real_setup;
static fn_start_t g_real_start;
static fn_teardown_t g_real_teardown;
static fn_platform_control_t g_real_platform_control;
static fn_session_control_t g_real_session_control;
static fn_make_mode_state_t g_make_mode_state;
static fn_serverinfo_t g_real_serverinfo;
static fn_sendcmd_t g_sendcmd;
static fn_aes_cbc_init_t g_real_aes_cbc_init;
static fn_aes_ctr_init_t g_aes_ctr_init;
static fn_aes_ctr_update_t g_aes_ctr_update;
static fn_aes_ctr_final_t g_aes_ctr_final;
static fn_derive_screen_t g_derive_screen;

static fn_cfstr_create_t p_CFStringCreateWithCString;
static fn_cfstr_cstr_ptr_t p_CFStringGetCStringPtr;
static fn_cfstr_cstr_t p_CFStringGetCString;
static fn_cfget_typeid_t p_CFGetTypeID;
static fn_cftypeid_t p_CFStringGetTypeID;
static fn_cftypeid_t p_CFDictionaryGetTypeID;
static fn_cftypeid_t p_CFArrayGetTypeID;
static fn_cftypeid_t p_CFNumberGetTypeID;
static fn_cftypeid_t p_CFBooleanGetTypeID;
static fn_cftypeid_t p_CFDataGetTypeID;
static fn_cfget_i64_t p_CFGetInt64;
static fn_cfbool_get_t p_CFBooleanGetValue;
static fn_cfdata_len_t p_CFDataGetLength;
static fn_cfdata_ptr_t p_CFDataGetBytePtr;
static fn_cfdict_count_t p_CFDictionaryGetCount;
static fn_cfdict_keys_t p_CFDictionaryGetKeysAndValues;
static fn_cfdict_get_t p_CFDictionaryGetValue;
static fn_cfdict_set_t p_CFDictionarySetValue;
static fn_cfdict_remove_t p_CFDictionaryRemoveValue;
static fn_cfdict_get_i64_t p_CFDictionaryGetInt64;
static fn_cfdict_set_i64_t p_CFDictionarySetInt64;
static fn_cfdict_new_t p_CFDictionaryCreateMutable;
static fn_cfarr_count_t p_CFArrayGetCount;
static fn_cfarr_get_t p_CFArrayGetValueAtIndex;
static fn_cfarr_append_t p_CFArrayAppendValue;
static fn_cfarr_new_t p_CFArrayCreateMutable;
static fn_cfarr_copy_t p_CFArrayCreateMutableCopy;
static fn_cfretain_t p_CFRetain;
static fn_cfrelease_t p_CFRelease;
static const void *p_dict_key_callbacks;
static const void *p_dict_val_callbacks;
static const void *p_array_callbacks;
static CFTypeRef p_cfl_boolean_false;

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_heartbeat_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_capture_lock = PTHREAD_MUTEX_INITIALIZER;
static AirPlayReceiverSessionRef g_active_session;
static uint8_t g_master_key[16];
static int g_master_valid;
static uintptr_t g_security_fn;

static int g_alt_listen = -1;
static int g_alt_client = -1;
static int g_alt_listener_ipv6;
static pthread_t g_alt_thread;
static int g_alt_thread_started;
static aes_ctr_storage_t g_alt_aes;
static int g_alt_aes_valid;

static int g_tee_listen = -1;
static int g_tee_client = -1;
static uint64_t g_tee_consumer_generation;
static pthread_t g_tee_thread;
static int g_tee_started;

static int g_capture_listen = -1;
static int g_capture_client = -1;
static int g_capture_port = 19821;
static pthread_t g_capture_thread;
static int g_capture_started;
static int g_capture_wait_idr = 1;
static uint64_t g_capture_bytes;
static uint64_t g_capture_chunks;
static uint64_t g_capture_drops;

static int g_enabled = 1;
static int g_alt_port = 6031;
static int g_tee_port = 19820;
static int g_width = 1010;
static int g_height = 376;
static int g_width_mm = 200;
static int g_height_mm = 74;
static int g_fps = 30;
static int g_auto_show = 0;
static int g_viewareas = 1;
static const char *g_viewareas_marker = "/mnt/app/root/mibr-carplay111-viewareas.enabled";
static const char *g_safearea_config_path = "/mnt/app/root/mibr-carplay111-safearea.conf";
static const char *g_autoshow_disable_marker = "/mnt/app/root/mibr-carplay111-autoshow.disabled";
static const char *g_url_map_marker = "/mnt/app/root/mibr-carplay111-url-map.enabled";
static const char *g_url_mode_path = "/tmp/mibr-alt111-url-mode";
/*
 * Runtime navigation composition controls.
 *
 * The master query marker keeps the historic bare-URL behaviour as the
 * fail-safe default.  Surface and appearance values live under /mnt/app/root
 * so the same switches can be used for one-off vehicle A/B tests and later
 * persistent operation.  instructioncard is intentionally kept query-free,
 * matching observed Apple Simulator behaviour.
 */
static const char *g_nav_query_enable_marker = "/mnt/app/root/mibr-carplay111-nav-query.enabled";
static const char *g_nav_surface_path = "/mnt/app/root/mibr-carplay111-nav.surface";
static const char *g_nav_eta_path = "/mnt/app/root/mibr-carplay111-nav.showETA";
static const char *g_nav_speed_path = "/mnt/app/root/mibr-carplay111-nav.showSpeedLimit";
static const char *g_nav_compass_path = "/mnt/app/root/mibr-carplay111-nav.showCompass";
static const char *g_nav_maneuver_path = "/mnt/app/root/mibr-carplay111-nav.maneuverLayout";
static const char *g_bit26_on_marker = "/mnt/app/root/mibr-carplay111-bit26.force-on";
static const char *g_bit26_off_marker = "/mnt/app/root/mibr-carplay111-bit26.force-off";
static char g_alt_uuid[96] = ALT_UUID_DEFAULT;
static char g_alt_url[160] = ALT_URL_DEFAULT;
static const char *g_log_path = "/tmp/altscreen111.log";
static const char *g_state_path = "/tmp/mibr-carplay111.state";
static const char *g_heartbeat_path = "/tmp/mibr-carplay111.heartbeat";
static const char *g_capture_status_path = "/tmp/mibr-alt111-capture.status";
static int g_streaming;
static int g_video_config_seen;
static uint64_t g_last_heartbeat_ms;

/* Gen-2 core: all access is serialized through g2_core_lock. */
static pthread_mutex_t g2_core_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g2_status_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g2_core_cv = PTHREAD_COND_INITIALIZER;
static struct alt111_profile g2_profile;
static struct alt111_control g2_control;
static struct alt111_video g2_video;
static struct alt111_resync g2_resync;
static mibr_mode_state_t g2_last_mode_state;
static unsigned g2_last_mode_valid;
static uint64_t g2_mode_sequence;
static uint64_t g2_control_session;
static uint64_t g2_video_stream;
static uint64_t g2_last_dispatched_request;
static uint64_t g2_last_completed_request;
static int g2_last_completion_status;
static unsigned g2_command_ready;
static pthread_t g2_control_thread;
static pthread_t g2_output_thread;
static int g2_workers_started;
static const char *g2_status_path = "/tmp/mibr-alt111-gen2.status";
static const char *g2_reacquire_marker = "/tmp/mibr-alt111-gen2-reacquire";
static const char *g2_diag_stop_marker = "/tmp/mibr-alt111-stop-only";
static const char *g2_diag_show_marker = "/tmp/mibr-alt111-show-only";
static const char *g2_diag_keyframe_marker = "/tmp/mibr-alt111-keyframe-only";
static const char *g2_resync_enable_marker = "/tmp/mibr-alt111-resync.enabled";
static const char *g2_resync_arm_marker = "/tmp/mibr-alt111-resync-arm";
static const char *g2_d2_enable_marker = "/tmp/mibr-alt111-keyframe-policy.enabled";
static const char *g2_d2_persist_marker = "/mnt/app/root/mibr-alt111-keyframe-policy.enabled";

#define G2_D2_EVENT_DELAY_MS 250u
#define G2_D2_MIN_GAP_MS 1000u
#define G2_D2_WATCHDOG_MS 1000u
#define G2_D2_SOURCE_TURNS 0x01u
#define G2_D2_SOURCE_SUGGEST_UI 0x02u

static unsigned g2_d2_was_enabled;
static unsigned g2_d2_pending_sources;
static uint64_t g2_d2_pending_due_ms;
static uint64_t g2_d2_last_request_ms;
static uint64_t g2_d2_last_seen_idrs;
static uint64_t g2_d2_last_idr_ms;
static uint64_t g2_d2_event_triggers;
static uint64_t g2_d2_watchdog_triggers;
static uint64_t g2_d2_coalesced;

static void gen2_publish_status(void);
static void gen2_control_projection_on(void);
static void gen2_control_release(void);
static void gen2_set_command_ready(unsigned ready);
static void gen2_video_begin_current(void);
static void gen2_video_end_current(void);
static void gen2_close_consumer(void);
static void gen2_resync_poll(void);
static void gen2_d2_schedule(unsigned source, const char *label);
static void tee_drop_client_locked(void);
static void capture_drop_client_locked(void);
static void capture_mirror_send(const uint8_t *p, size_t n,
                                const struct alt111_output_ticket *ticket);
static int start_capture_server(void);

static void logf_u2(const char *fmt, ...)
{
    char buf[768];
    int fd;
    int n;
    va_list ap;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    n = snprintf(buf, sizeof(buf), "%ld.%03ld [altscreen111] ", (long)ts.tv_sec, ts.tv_nsec / 1000000L);
    if (n < 0) return;
    va_start(ap, fmt);
    n += vsnprintf(buf + n, (n < (int)sizeof(buf)) ? sizeof(buf) - (size_t)n : 0, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= sizeof(buf) - 2) n = (int)sizeof(buf) - 2;
    buf[n++] = '\n';
    fd = open(g_log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd >= 0) { (void)write(fd, buf, (size_t)n); close(fd); }
}

static uint64_t monotonic_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000L);
}

static unsigned gen2_d2_enabled(void)
{
    return (access(g2_d2_enable_marker,F_OK)==0 ||
            access(g2_d2_persist_marker,F_OK)==0) ? 1u : 0u;
}

static void gen2_d2_schedule(unsigned source, const char *label)
{
    uint64_t now = monotonic_ms();
    uint64_t due = now + G2_D2_EVENT_DELAY_MS;
    uint64_t last_request = 0;
    unsigned active = 0;
    unsigned pending = 0;
    int enabled = gen2_d2_enabled() ? 1 : 0;

    if(!enabled) return;

    pthread_mutex_lock(&g2_core_lock);
    if(g2_d2_last_request_ms && due < g2_d2_last_request_ms + G2_D2_MIN_GAP_MS)
        due = g2_d2_last_request_ms + G2_D2_MIN_GAP_MS;

    if(g2_resync.state == ALT111_RESYNC_NEED_IDR) {
        ++g2_d2_coalesced;
        active = 1;
    } else {
        /*
         * Re-arm the debounce window from the newest event.  This intentionally
         * coalesces suggestUI + turns bursts into one IDR request after the
         * scene transition has had time to land on Stream 111.
         */
        g2_d2_pending_sources |= source;
        g2_d2_pending_due_ms = due;
        ++g2_d2_event_triggers;
        pending = g2_d2_pending_sources;
    }
    last_request = g2_d2_last_request_ms;
    pthread_mutex_unlock(&g2_core_lock);

    logf_u2("GEN2 D2 EVENT source=%s action=%s pending=0x%x due_ms=%llu last_request_ms=%llu",
            label?label:"unknown",active?"coalesced-active":"scheduled",pending,
            (unsigned long long)due,(unsigned long long)last_request);
}

static void capture_publish_status(const char *state)
{
    char b[320];
    int fd, n, client, wait_idr;
    uint64_t bytes, chunks, drops;

    pthread_mutex_lock(&g_capture_lock);
    client = g_capture_client >= 0 ? 1 : 0;
    wait_idr = g_capture_wait_idr;
    bytes = g_capture_bytes;
    chunks = g_capture_chunks;
    drops = g_capture_drops;
    pthread_mutex_unlock(&g_capture_lock);

    n = snprintf(b, sizeof(b),
                 "state=%s\nport=%d\nclient=%d\nwait_idr=%d\nbytes=%llu\nchunks=%llu\ndrops=%llu\n",
                 state ? state : "unknown", g_capture_port, client, wait_idr,
                 (unsigned long long)bytes,
                 (unsigned long long)chunks,
                 (unsigned long long)drops);
    if (n <= 0) return;
    if ((size_t)n >= sizeof(b)) n = (int)sizeof(b) - 1;
    fd = open(g_capture_status_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        (void)write(fd, b, (size_t)n);
        close(fd);
    }
}

static void publish_state(const char *state)
{
    int fd;
    size_t n;
    if (!state) return;
    fd = open(g_state_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        n = strlen(state);
        (void)write(fd, state, n);
        (void)write(fd, "\n", 1);
        close(fd);
    }
}

static void publish_video_heartbeat_internal(int force)
{
    char b[48];
    int fd;
    int n;
    uint64_t now = monotonic_ms();

    pthread_mutex_lock(&g_heartbeat_lock);
    if (!force && now && g_last_heartbeat_ms && now - g_last_heartbeat_ms < 250u) {
        pthread_mutex_unlock(&g_heartbeat_lock);
        return;
    }
    if (force && now <= g_last_heartbeat_ms) now = g_last_heartbeat_ms + 1u;
    g_last_heartbeat_ms = now;
    n = snprintf(b, sizeof(b), "%llu\n", (unsigned long long)now);
    if (n > 0) {
        fd = open(g_heartbeat_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            (void)write(fd, b, (size_t)n);
            close(fd);
        }
    }
    pthread_mutex_unlock(&g_heartbeat_lock);
}

static void publish_video_heartbeat(void)
{
    publish_video_heartbeat_internal(0);
}

static void publish_video_heartbeat_force(void)
{
    publish_video_heartbeat_internal(1);
}

static void publish_video_heartbeat_source_arm(void)
{
    int need_arm;
    pthread_mutex_lock(&g_heartbeat_lock);
    need_arm = (g_last_heartbeat_ms == 0);
    pthread_mutex_unlock(&g_heartbeat_lock);
    if (need_arm) publish_video_heartbeat_force();
}

static void clear_video_heartbeat(void)
{
    pthread_mutex_lock(&g_heartbeat_lock);
    g_last_heartbeat_ms = 0;
    (void)unlink(g_heartbeat_path);
    pthread_mutex_unlock(&g_heartbeat_lock);
}

static void clear_video_observer(void)
{
    g_streaming = 0;
    g_video_config_seen = 0;
    clear_video_heartbeat();
}

static int env_i(const char *name, int defv)
{
    const char *s = getenv(name);
    return (s && *s) ? atoi(s) : defv;
}

static void env_s(const char *name, char *dst, size_t cap, const char *defv)
{
    const char *s = getenv(name);
    if (!s || !*s) s = defv;
    snprintf(dst, cap, "%s", s);
}

static CFStringRef s_cf(const char *s);

/*
 * Persistent A/B controls are intentionally file-based so the vehicle can
 * switch protocol variants without replacing/recompiling the injected binary.
 */
static int read_trimmed_value(const char *path, char *out, size_t cap)
{
    int fd;
    ssize_t n;
    if (!path || !out || cap < 2u) return -1;
    out[0] = '\0';
    fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    n = read(fd, out, cap - 1u);
    close(fd);
    if (n <= 0) return -1;
    out[n] = '\0';
    while (n > 0 && (out[n-1] == '\n' || out[n-1] == '\r' ||
                     out[n-1] == ' ' || out[n-1] == '\t'))
        out[--n] = '\0';
    return n > 0 ? 0 : -1;
}

static const char *g_safearea_idle_path = "/mnt/app/root/mibr-carplay111-safearea-idle.conf";
static unsigned g2_want_view;
/* r56-layouts: the cluster layout chosen with the MMI View button (dial size) changes the visible map area.
 * Slot 0 = small dials (Rect width ~700, the original idle/route files), slot 1 = big dials (~460),
 * slot 2 = third layout (wider than 800). Each slot owns an idle+route view pair; g2_layout_base[slot] is the view
 * index of its idle view (-1 = no config files -> that layout reuses slot 0). View index = base + (route ? 1 : 0). */
static int g2_layout_base[3]={0,-1,-1};
static unsigned g2_layout_cur;
static int g2_turns_route;
static unsigned g2_view_for(int route)
{
    int b=g2_layout_base[g2_layout_cur<3u?g2_layout_cur:0u];
    if(b<0) b=g2_layout_base[0]<0?0:g2_layout_base[0];
    return (unsigned)b+(route?1u:0u);
}

static int load_safearea_path(const char *path, int *x, int *y, int *w, int *h);
static void load_safearea_config(int *x, int *y, int *w, int *h)
{
    (void)load_safearea_path(g_safearea_config_path,x,y,w,h);
}

static int load_safearea_path(const char *path, int *x, int *y, int *w, int *h)
{
    char b[192];
    int fd, n;
    int tx=0, ty=0, tw=g_width, th=g_height;

    if(!x||!y||!w||!h) return -1;
    *x=0; *y=0; *w=g_width; *h=g_height;

    fd=open(path,O_RDONLY);
    if(fd<0) return -1;
    n=(int)read(fd,b,sizeof(b)-1u);
    close(fd);
    if(n<=0) return -1;
    b[n]='\0';

    if(sscanf(b,"x=%d\ny=%d\nw=%d\nh=%d",&tx,&ty,&tw,&th)!=4){
        logf_u2("GEN2 SafeArea config invalid path=%s; using full canvas",
                path);
        return -1;
    }

    if(tx<0||ty<0||tw<1||th<1||tx>=g_width||ty>=g_height||
       tx+tw>g_width||ty+th>g_height){
        logf_u2("GEN2 SafeArea config out-of-range x=%d y=%d w=%d h=%d full=%dx%d; using full canvas",
                tx,ty,tw,th,g_width,g_height);
        return -1;
    }

    *x=tx; *y=ty; *w=tw; *h=th;
    return 0;
}

static const char *nav_surface_url(char *surface, size_t surface_cap)
{
    char mode[32];

    /*
     * Volatile vehicle-test override has highest priority for backwards
     * compatibility with the existing A/B harness.
     */
    if (read_trimmed_value(g_url_mode_path, mode, sizeof(mode)) == 0) {
        if (strcmp(mode, "base") == 0) {
            snprintf(surface, surface_cap, "%s", "base");
            return ALT_URL_DEFAULT;
        }
        if (strcmp(mode, "map") == 0) {
            snprintf(surface, surface_cap, "%s", "map");
            return ALT_URL_MAP;
        }
        if (strcmp(mode, "instructioncard") == 0) {
            snprintf(surface, surface_cap, "%s", "instructioncard");
            return ALT_URL_INSTRUCTIONCARD;
        }
    }

    if (read_trimmed_value(g_nav_surface_path, mode, sizeof(mode)) == 0) {
        if (strcmp(mode, "base") == 0) {
            snprintf(surface, surface_cap, "%s", "base");
            return ALT_URL_DEFAULT;
        }
        if (strcmp(mode, "map") == 0) {
            snprintf(surface, surface_cap, "%s", "map");
            return ALT_URL_MAP;
        }
        if (strcmp(mode, "instructioncard") == 0) {
            snprintf(surface, surface_cap, "%s", "instructioncard");
            return ALT_URL_INSTRUCTIONCARD;
        }
    }

    /* Preserve the earlier persistent base/map A/B marker as fallback. */
    if (access(g_url_map_marker, F_OK) == 0) {
        snprintf(surface, surface_cap, "%s", "map");
        return ALT_URL_MAP;
    }
    snprintf(surface, surface_cap, "%s", "base");
    return g_alt_url;
}

static void nav_tristate_value(const char *path, char *out, size_t cap,
                               const char *defv)
{
    char value[24];
    if (read_trimmed_value(path, value, sizeof(value)) == 0 &&
        (strcmp(value, "yes") == 0 || strcmp(value, "no") == 0 ||
         strcmp(value, "user") == 0 || strcmp(value, "1") == 0 ||
         strcmp(value, "0") == 0)) {
        snprintf(out, cap, "%s", value);
        return;
    }
    snprintf(out, cap, "%s", defv);
}

static void nav_maneuver_value(char *out, size_t cap)
{
    char value[32];
    if (read_trimmed_value(g_nav_maneuver_path, value, sizeof(value)) != 0 ||
        strcmp(value, "none") == 0 || value[0] == '\0') {
        out[0] = '\0';
        return;
    }
    if (strcmp(value, "left") == 0 || strcmp(value, "leftAligned") == 0) {
        snprintf(out, cap, "%s", "leftAligned");
        return;
    }
    if (strcmp(value, "right") == 0 || strcmp(value, "rightAligned") == 0) {
        snprintf(out, cap, "%s", "rightAligned");
        return;
    }
    if (strcmp(value, "top") == 0 || strcmp(value, "topAligned") == 0) {
        snprintf(out, cap, "%s", "topAligned");
        return;
    }
    out[0] = '\0';
}

static void active_alt_url_copy(char *out, size_t cap)
{
    char surface[24];
    char eta[24], speed[24], compass[24], maneuver[32];
    const char *base;

    if (!out || cap == 0u) return;
    out[0] = '\0';
    base = nav_surface_url(surface, sizeof(surface));

    /*
     * Compatibility/default mode: keep the exact historical bare URL.
     * Apple Simulator observations also keep instructioncard bare even when
     * map/base appearance query parameters are in use.
     */
    if (access(g_nav_query_enable_marker, F_OK) != 0 ||
        strcmp(surface, "instructioncard") == 0) {
        snprintf(out, cap, "%s", base);
        return;
    }

    nav_tristate_value(g_nav_speed_path, speed, sizeof(speed), "user");
    nav_tristate_value(g_nav_compass_path, compass, sizeof(compass), "user");
    nav_tristate_value(g_nav_eta_path, eta, sizeof(eta), "yes");
    nav_maneuver_value(maneuver, sizeof(maneuver));

    snprintf(out, cap,
             "%s?showSpeedLimit=%s&showCompass=%s&showETA=%s&maneuverLayout=%s",
             base, speed, compass, eta, maneuver);
}

/* 0 = preserve stock, +1 = force bit 26, -1 = clear bit 26. */
static int airplay_bit26_mode(void)
{
    if (access(g_bit26_on_marker, F_OK) == 0) return 1;
    if (access(g_bit26_off_marker, F_OK) == 0) return -1;
    return 0;
}

static void apply_airplay_bit26_ab(CFMutableDictionaryRef info)
{
    CFStringRef k;
    OSStatus err = 0;
    int mode;
    int64_t before, after;

    if (!info) return;
    mode = airplay_bit26_mode();
    if (!mode) return;

    k = s_cf("features");
    if (!k) return;
    before = p_CFDictionaryGetInt64(info, k, &err);
    if (err) {
        logf_u2("GEN2 A/B bit26 mode=%s root features unavailable; unchanged",
                mode > 0 ? "force-on" : "force-off");
        p_CFRelease(k);
        return;
    }

    after = (int64_t)(mode > 0
        ? ((uint64_t)before | AIRPLAY_FEATURE_BIT26)
        : ((uint64_t)before & ~AIRPLAY_FEATURE_BIT26));
    if (after != before) p_CFDictionarySetInt64(info, k, after);
    logf_u2("GEN2 A/B bit26 mode=%s features=0x%llx->0x%llx",
            mode > 0 ? "force-on" : "force-off",
            (unsigned long long)(uint64_t)before,
            (unsigned long long)(uint64_t)after);
    p_CFRelease(k);
}

static void *sym_next(const char *name)
{
    void *p = dlsym(RTLD_NEXT, name);
    if (!p) p = dlsym(RTLD_DEFAULT, name);
    return p;
}

static int init_api(void)
{
#define RESOLVE(dst, name) do { dst = (void *)sym_next(name); if (!(dst)) { logf_u2("missing symbol %s", name); return -1; } } while (0)
    RESOLVE(g_real_serverinfo, "AirPlayCopyServerInfo");
    RESOLVE(g_sendcmd, "AirPlayReceiverSessionSendCommand");
    RESOLVE(g_real_aes_cbc_init, "AES_CBCFrame_Init");
    RESOLVE(g_aes_ctr_init, "AES_CTR_Init");
    RESOLVE(g_aes_ctr_update, "AES_CTR_Update");
    RESOLVE(g_aes_ctr_final, "AES_CTR_Final");
    RESOLVE(g_derive_screen, "AirPlay_DeriveAESKeySHA512ForScreen");
    RESOLVE(p_CFStringCreateWithCString, "CFStringCreateWithCString");
    RESOLVE(p_CFStringGetCStringPtr, "CFStringGetCStringPtr");
    RESOLVE(p_CFStringGetCString, "CFStringGetCString");
    RESOLVE(p_CFGetTypeID, "CFGetTypeID");
    RESOLVE(p_CFStringGetTypeID, "CFStringGetTypeID");
    RESOLVE(p_CFDictionaryGetTypeID, "CFDictionaryGetTypeID");
    RESOLVE(p_CFArrayGetTypeID, "CFArrayGetTypeID");
    RESOLVE(p_CFNumberGetTypeID, "CFNumberGetTypeID");
    RESOLVE(p_CFBooleanGetTypeID, "CFBooleanGetTypeID");
    RESOLVE(p_CFDataGetTypeID, "CFDataGetTypeID");
    RESOLVE(p_CFGetInt64, "CFGetInt64");
    RESOLVE(p_CFBooleanGetValue, "CFBooleanGetValue");
    RESOLVE(p_CFDataGetLength, "CFDataGetLength");
    RESOLVE(p_CFDataGetBytePtr, "CFDataGetBytePtr");
    RESOLVE(p_CFDictionaryGetCount, "CFDictionaryGetCount");
    RESOLVE(p_CFDictionaryGetKeysAndValues, "CFDictionaryGetKeysAndValues");
    RESOLVE(p_CFDictionaryGetValue, "CFDictionaryGetValue");
    RESOLVE(p_CFDictionarySetValue, "CFDictionarySetValue");
    RESOLVE(p_CFDictionaryRemoveValue, "CFDictionaryRemoveValue");
    RESOLVE(p_CFDictionaryGetInt64, "CFDictionaryGetInt64");
    RESOLVE(p_CFDictionarySetInt64, "CFDictionarySetInt64");
    RESOLVE(p_CFDictionaryCreateMutable, "CFDictionaryCreateMutable");
    RESOLVE(p_CFArrayGetCount, "CFArrayGetCount");
    RESOLVE(p_CFArrayGetValueAtIndex, "CFArrayGetValueAtIndex");
    RESOLVE(p_CFArrayAppendValue, "CFArrayAppendValue");
    RESOLVE(p_CFArrayCreateMutable, "CFArrayCreateMutable");
    RESOLVE(p_CFArrayCreateMutableCopy, "CFArrayCreateMutableCopy");
    RESOLVE(p_CFRetain, "CFRetain");
    RESOLVE(p_CFRelease, "CFRelease");
    p_dict_key_callbacks = dlsym(RTLD_DEFAULT, "kCFLDictionaryKeyCallBacksCFLTypes");
    p_dict_val_callbacks = dlsym(RTLD_DEFAULT, "kCFLDictionaryValueCallBacksCFLTypes");
    p_array_callbacks = dlsym(RTLD_DEFAULT, "kCFLArrayCallBacksCFLTypes");
    {
        CFTypeRef *false_slot = (CFTypeRef *)dlsym(RTLD_DEFAULT, "kCFLBooleanFalse");
        p_cfl_boolean_false = false_slot ? *false_slot : NULL;
    }
    if (!p_dict_key_callbacks || !p_dict_val_callbacks || !p_array_callbacks ||
        !p_cfl_boolean_false) {
        logf_u2("missing CFLite callback tables/boolean singleton");
        return -1;
    }
    g_security_fn = (uintptr_t)sym_next("AirPlayReceiverSessionSetSecurityInfo");
    if (!g_security_fn) { logf_u2("missing AirPlayReceiverSessionSetSecurityInfo"); return -1; }
#undef RESOLVE
    return 0;
}

static CFStringRef s_cf(const char *s)
{
    return p_CFStringCreateWithCString(NULL, s, CF_UTF8);
}

static CFMutableDictionaryRef dict_new(void)
{
    return p_CFDictionaryCreateMutable(NULL, 0, p_dict_key_callbacks, p_dict_val_callbacks);
}

static CFMutableDictionaryRef dict_clone(CFDictionaryRef src)
{
    CFMutableDictionaryRef dst;
    CFIndex n, i;
    const void **keys, **vals;
    if (!src) return NULL;
    dst = dict_new();
    if (!dst) return NULL;
    n = p_CFDictionaryGetCount(src);
    if (n <= 0) return dst;
    keys = calloc((size_t)n, sizeof(*keys));
    vals = calloc((size_t)n, sizeof(*vals));
    if (!keys || !vals) { free(keys); free(vals); p_CFRelease(dst); return NULL; }
    p_CFDictionaryGetKeysAndValues(src, keys, vals);
    for (i = 0; i < n; ++i) p_CFDictionarySetValue(dst, keys[i], vals[i]);
    free(keys); free(vals);
    return dst;
}

static int stream_type(CFDictionaryRef d)
{
    OSStatus e = 0;
    CFStringRef k = s_cf("type");
    int64_t v = p_CFDictionaryGetInt64(d, k, &e);
    p_CFRelease(k);
    return e ? -1 : (int)v;
}

static uint64_t stream_connection_id(CFDictionaryRef d)
{
    OSStatus e = 0;
    CFStringRef k = s_cf("streamConnectionID");
    int64_t v = p_CFDictionaryGetInt64(d, k, &e);
    p_CFRelease(k);
    return e ? 0 : (uint64_t)v;
}

static CFArrayRef get_streams(CFDictionaryRef d)
{
    CFStringRef k;
    CFArrayRef a;
    if (!d) return NULL;
    k = s_cf("streams");
    a = (CFArrayRef)p_CFDictionaryGetValue(d, k);
    p_CFRelease(k);
    return a;
}

static int contains_stream_type(CFDictionaryRef request, int wanted)
{
    CFArrayRef a = get_streams(request);
    CFIndex i, n;
    if (!a) return 0;
    n = p_CFArrayGetCount(a);
    for (i = 0; i < n; ++i) {
        CFDictionaryRef sd = (CFDictionaryRef)p_CFArrayGetValueAtIndex(a, i);
        if (stream_type(sd) == wanted) return 1;
    }
    return 0;
}

static int contains_stream111(CFDictionaryRef request, CFDictionaryRef *outDesc, int *outOtherCount)
{
    CFArrayRef a = get_streams(request);
    CFIndex i, n;
    int found = 0, other = 0;
    if (outDesc) *outDesc = NULL;
    if (!a) { if (outOtherCount) *outOtherCount = 0; return 0; }
    n = p_CFArrayGetCount(a);
    for (i = 0; i < n; ++i) {
        CFDictionaryRef sd = (CFDictionaryRef)p_CFArrayGetValueAtIndex(a, i);
        if (stream_type(sd) == K_STREAM_ALT) {
            found = 1;
            if (outDesc && !*outDesc) *outDesc = sd;
        } else ++other;
    }
    if (outOtherCount) *outOtherCount = other;
    return found;
}

static CFMutableDictionaryRef clone_without_111(CFDictionaryRef request)
{
    CFMutableDictionaryRef d = dict_clone(request);
    CFArrayRef a = get_streams(request);
    CFMutableArrayRef b;
    CFStringRef k;
    CFIndex i, n;
    if (!d || !a) return d;
    b = p_CFArrayCreateMutable(NULL, 0, p_array_callbacks);
    if (!b) return d;
    n = p_CFArrayGetCount(a);
    for (i = 0; i < n; ++i) {
        CFDictionaryRef sd = (CFDictionaryRef)p_CFArrayGetValueAtIndex(a, i);
        if (stream_type(sd) != K_STREAM_ALT) p_CFArrayAppendValue(b, sd);
    }
    k = s_cf("streams");
    p_CFDictionarySetValue(d, k, b);
    p_CFRelease(k);
    p_CFRelease(b);
    return d;
}

/*
 * Recovered MHI2Q IRC behavior: every successful /info or SETUP response
 * advertises the AltScreen capability at the root before the peer has to
 * choose stream 111. The reference replaces enabledFeatures with
 * ["altScreen","viewAreas"] rather than waiting for a 111 request.
 */
/* ===== AUDI MU0678 VARIANT (audi-mu0678-r4) ==========================
 * Runtime policy markers, read on every /info and SETUP so they can be flipped
 * in /tmp between phone plug-ins without rebuilding or rebooting:
 *   /tmp/mibr-alt111-audi.features-stock : never touch enabledFeatures
 *   /tmp/mibr-alt111-audi.features-merge : keep stock entries, append ours
 *   (neither)                            : upstream behaviour = replace
 *   /tmp/mibr-alt111-audi.no-alt-display : do not append the type-111 display
 * ==================================================================== */
#define ALT111_AUDI_BUILD_TAG "audi-mu0678-r23-rxstats"
static const char *g_audi_feat_stock_marker = "/tmp/mibr-alt111-audi.features-stock";
static const char *g_audi_feat_merge_marker = "/tmp/mibr-alt111-audi.features-merge";
static const char *g_audi_nodisp_marker     = "/tmp/mibr-alt111-audi.no-alt-display";

static const char *flight_command_cstr(CFStringRef command, char *buf, size_t n);
static int audi_verbose(void);
static int audi_stage(void);

static void audi_log_features(const char *tag, CFTypeRef v)
{
    if(!v){ logf_u2("AUDI enabledFeatures[%s]=<absent>",tag); return; }
    if(p_CFGetTypeID(v)==p_CFArrayGetTypeID()){
        CFIndex i,n=p_CFArrayGetCount((CFArrayRef)v);
        logf_u2("AUDI enabledFeatures[%s]=array count=%ld",tag,(long)n);
        for(i=0;i<n && i<16;++i){
            CFTypeRef it=(CFTypeRef)p_CFArrayGetValueAtIndex((CFArrayRef)v,i);
            if(it && p_CFGetTypeID(it)==p_CFStringGetTypeID()){
                char b[96];
                logf_u2("AUDI enabledFeatures[%s][%ld]=%s",tag,(long)i,
                        flight_command_cstr((CFStringRef)it,b,sizeof(b)));
            }else logf_u2("AUDI enabledFeatures[%s][%ld]=<type:%lu>",tag,(long)i,
                          (unsigned long)(it?p_CFGetTypeID(it):0));
        }
    }else logf_u2("AUDI enabledFeatures[%s]=<type:%lu>",tag,(unsigned long)p_CFGetTypeID(v));
}

static int audi_array_has(CFArrayRef a, const char *want)
{
    CFIndex i,n;
    if(!a) return 0;
    n=p_CFArrayGetCount(a);
    for(i=0;i<n;++i){
        CFTypeRef it=(CFTypeRef)p_CFArrayGetValueAtIndex(a,i);
        if(it && p_CFGetTypeID(it)==p_CFStringGetTypeID()){
            char b[96];
            if(strcmp(flight_command_cstr((CFStringRef)it,b,sizeof(b)),want)==0) return 1;
        }
    }
    return 0;
}

static void set_reference_enabled_features(CFMutableDictionaryRef response)
{
    CFStringRef k = NULL, alt = NULL, va = NULL;
    CFMutableArrayRef a = NULL;
    CFTypeRef cur;
    int stock_mode, merge_mode;
    if(!response) return;

    k = s_cf("enabledFeatures");
    if(!k) return;
    cur = (CFTypeRef)p_CFDictionaryGetValue(response,k);
    audi_log_features("stock",cur);
    stock_mode = access(g_audi_feat_stock_marker,F_OK)==0;
    merge_mode = !stock_mode && access(g_audi_feat_merge_marker,F_OK)==0;
    if(stock_mode){
        logf_u2("AUDI features policy=stock (enabledFeatures untouched)");
        p_CFRelease(k);
        return;
    }

    alt = s_cf("altScreen");
    va = s_cf("viewAreas");
    if(merge_mode && cur && p_CFGetTypeID(cur)==p_CFArrayGetTypeID())
        a = p_CFArrayCreateMutableCopy(NULL,0,(CFArrayRef)cur);
    else
        a = p_CFArrayCreateMutable(NULL,0,p_array_callbacks);
    if(a && alt && !audi_array_has((CFArrayRef)a,"altScreen")) p_CFArrayAppendValue(a,alt);
    if(a && va && g_viewareas && !audi_array_has((CFArrayRef)a,"viewAreas")) p_CFArrayAppendValue(a,va);
    if(a) p_CFDictionarySetValue(response,k,a);
    logf_u2("AUDI features policy=%s",merge_mode?"merge":"replace(upstream)");
    audi_log_features("final",(CFTypeRef)a);

    if(a) p_CFRelease(a);
    if(va) p_CFRelease(va);
    if(alt) p_CFRelease(alt);
    p_CFRelease(k);
}

/* Clone the exact requested 111 descriptor and preserve unknown peer fields. */
static void append_alt_setup_response(CFMutableDictionaryRef response,
                                      CFDictionaryRef requested,
                                      int data_port)
{
    CFStringRef kstreams = NULL, kport = NULL, kstreamid = NULL;
    CFArrayRef old = NULL;
    CFMutableArrayRef a = NULL;
    CFMutableDictionaryRef sd = NULL;

    if(!response || !requested) return;
    kstreams = s_cf("streams");
    old = (CFArrayRef)p_CFDictionaryGetValue(response,kstreams);
    a = old ? p_CFArrayCreateMutableCopy(NULL,0,old)
            : p_CFArrayCreateMutable(NULL,0,p_array_callbacks);
    sd = dict_clone(requested);
    kport = s_cf("dataPort");
    kstreamid = s_cf("streamID");

    if(a && sd && kport && kstreamid) {
        p_CFDictionarySetInt64(sd,kport,data_port);
        p_CFDictionarySetInt64(sd,kstreamid,K_STREAM_ALT);
        p_CFArrayAppendValue(a,sd);
        p_CFDictionarySetValue(response,kstreams,a);
    }

    if(sd) p_CFRelease(sd);
    if(a) p_CFRelease(a);
    if(kstreamid) p_CFRelease(kstreamid);
    if(kport) p_CFRelease(kport);
    if(kstreams) p_CFRelease(kstreams);
}

/*
 * Two deliberately different network boundaries:
 *  - stream 111 is inbound from the iPhone and therefore must be reachable on
 *    the CarPlay link (INADDR_ANY / actual head-unit address);
 *  - the decoded H.264 tee is local-only and must never leave loopback.
 *
 * Only the iPhone-facing listener may fall back to an ephemeral port because
 * its actual dataPort is returned in SETUP. The renderer tee is configured by
 * URL and must fail closed if its fixed port is occupied.
 */
static int bind_listener_ipv4(int preferred, int *out_port, int loopback, int allow_ephemeral)
{
    int fd, one = 1;
    struct sockaddr_in sa;
    socklen_t sl = sizeof(sa);
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(loopback ? INADDR_LOOPBACK : INADDR_ANY);
    sa.sin_port = htons((uint16_t)preferred);
    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        if (!allow_ephemeral) { close(fd); return -1; }
        sa.sin_port = 0;
        if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) { close(fd); return -1; }
    }
    if (listen(fd, 2) < 0) { close(fd); return -1; }
    if (getsockname(fd, (struct sockaddr *)&sa, &sl) == 0 && out_port) *out_port = ntohs(sa.sin_port);
    return fd;
}

static int bind_listener_stream111(int preferred, int *out_port, int allow_ephemeral)
{
#ifdef AF_INET6
    int fd,one=1,off=0;
    struct sockaddr_in6 sa6;
    socklen_t sl6=sizeof(sa6);
    fd=socket(AF_INET6,SOCK_STREAM,IPPROTO_TCP);
    if(fd>=0){
        setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(one));
#ifdef IPV6_V6ONLY
        (void)setsockopt(fd,IPPROTO_IPV6,IPV6_V6ONLY,&off,sizeof(off));
#endif
        memset(&sa6,0,sizeof(sa6));
        sa6.sin6_family=AF_INET6;
        sa6.sin6_port=htons((uint16_t)preferred);
        if(bind(fd,(struct sockaddr *)&sa6,sizeof(sa6))<0){
            if(allow_ephemeral){
                sa6.sin6_port=0;
                if(bind(fd,(struct sockaddr *)&sa6,sizeof(sa6))<0){close(fd);fd=-1;}
            }else{
                close(fd);
                fd=-1;
            }
        }
        if(fd>=0){
            if(listen(fd,1)==0){
                if(getsockname(fd,(struct sockaddr *)&sa6,&sl6)==0 && out_port)*out_port=ntohs(sa6.sin6_port);
                g_alt_listener_ipv6=1;
                return fd;
            }
            close(fd);
        }
    }
#endif
    g_alt_listener_ipv6=0;
    return bind_listener_ipv4(preferred,out_port,0,allow_ephemeral);
}

static CFMutableDictionaryRef command_force_keyframe(void);

static void set_active_session(AirPlayReceiverSessionRef s)
{
    AirPlayReceiverSessionRef old = NULL, keep = NULL;

    if (s) keep = (AirPlayReceiverSessionRef)p_CFRetain(s);
    pthread_mutex_lock(&g_lock);
    if (g_active_session == s) {
        pthread_mutex_unlock(&g_lock);
        if (keep) p_CFRelease(keep);
        return;
    }
    old = g_active_session;
    g_active_session = keep;
    pthread_mutex_unlock(&g_lock);
    if (old) p_CFRelease(old);
}

static AirPlayReceiverSessionRef retain_active_session(void)
{
    AirPlayReceiverSessionRef s = NULL;
    pthread_mutex_lock(&g_lock);
    if (g_active_session) s = (AirPlayReceiverSessionRef)p_CFRetain(g_active_session);
    pthread_mutex_unlock(&g_lock);
    return s;
}

/* ---------------- Gen-2 video adapter ---------------- */

static void gen2_close_consumer(void)
{
    pthread_mutex_lock(&g_lock);
    tee_drop_client_locked();
    pthread_mutex_unlock(&g_lock);

    pthread_mutex_lock(&g2_core_lock);
    alt111_video_detach(&g2_video);
    alt111_resync_cancel(&g2_resync, ALT111_RESYNC_CANCEL_CONSUMER);
    pthread_cond_broadcast(&g2_core_cv);
    pthread_mutex_unlock(&g2_core_lock);
}

static void gen2_video_begin_current(void)
{
    uint64_t session;
    gen2_close_consumer();
    pthread_mutex_lock(&g2_core_lock);
    alt111_resync_cancel(&g2_resync, ALT111_RESYNC_CANCEL_STREAM);
    session = g2_control_session ? g2_control_session : (g2_control.session + 1u);
    g2_video_stream = alt111_video_begin(&g2_video, session);
    pthread_cond_broadcast(&g2_core_cv);
    pthread_mutex_unlock(&g2_core_lock);
    gen2_publish_status();
}

static void gen2_video_end_current(void)
{
    pthread_mutex_lock(&g2_core_lock);
    alt111_resync_cancel(&g2_resync, ALT111_RESYNC_CANCEL_STREAM);
    if (g2_video_stream) (void)alt111_video_end(&g2_video, g2_video_stream);
    g2_video_stream = 0;
    pthread_cond_broadcast(&g2_core_cv);
    pthread_mutex_unlock(&g2_core_lock);

    pthread_mutex_lock(&g_lock);
    tee_drop_client_locked();
    pthread_mutex_unlock(&g_lock);

    pthread_mutex_lock(&g_capture_lock);
    capture_drop_client_locked();
    pthread_mutex_unlock(&g_capture_lock);
    capture_publish_status("listening");
    gen2_publish_status();
}

static int gen2_video_config(const uint8_t *p, size_t n)
{
    int rc;
    pthread_mutex_lock(&g2_core_lock);
    rc = g2_video_stream ? alt111_video_config(&g2_video, g2_video_stream, p, n) : ALT111_STALE;
    pthread_cond_broadcast(&g2_core_cv);
    pthread_mutex_unlock(&g2_core_lock);
    if (rc == ALT111_RESTART_CONSUMER || rc < 0) {
        logf_u2("gen2 VideoConfig rc=%d -> consumer reset", rc);
        gen2_close_consumer();
    }
    gen2_publish_status();
    return rc;
}

static int gen2_video_submit_au(const uint8_t *p, size_t n, int *accepted, int *consumer_attached)
{
    int rc;
    uint64_t before, after_count;
    if (accepted) *accepted = 0;
    if (consumer_attached) *consumer_attached = 0;
    pthread_mutex_lock(&g2_core_lock);
    before = g2_video.source_aus;
    rc = g2_video_stream ? alt111_video_submit(&g2_video, g2_video_stream, p, n, 1) : ALT111_STALE;
    after_count = g2_video.source_aus;
    if (accepted && after_count > before) *accepted = 1;
    if (consumer_attached) *consumer_attached = g2_video.attached ? 1 : 0;
    pthread_cond_broadcast(&g2_core_cv);
    pthread_mutex_unlock(&g2_core_lock);
    if (rc == ALT111_RESTART_CONSUMER || rc < 0) {
        logf_u2("gen2 video AU rc=%d body=%zu -> consumer reset", rc, n);
        gen2_close_consumer();
    }
    return rc;
}

static void gen2_keyframe_intent(void)
{
    int rc = ALT111_WAIT;
    pthread_mutex_lock(&g2_core_lock);
    if (g2_control_session)
        rc = alt111_control_keyframe(&g2_control, g2_control_session);
    pthread_cond_broadcast(&g2_core_cv);
    pthread_mutex_unlock(&g2_core_lock);
    logf_u2("gen2 consumer keyframe intent rc=%d", rc);
}

static void gen2_consumer_attach(void)
{
    int rc;
    uint64_t generation = 0;
    pthread_mutex_lock(&g2_core_lock);
    if (g2_video.attached) alt111_video_detach(&g2_video);
    rc = alt111_video_attach(&g2_video);
    if (rc == ALT111_OK) generation = g2_video.consumer;
    pthread_cond_broadcast(&g2_core_cv);
    pthread_mutex_unlock(&g2_core_lock);

    if (rc == ALT111_OK) {
        pthread_mutex_lock(&g_lock);
        if (g_tee_client >= 0) g_tee_consumer_generation = generation;
        pthread_mutex_unlock(&g_lock);
    }
    logf_u2("gen2 renderer consumer attach rc=%d generation=%llu",
            rc,(unsigned long long)generation);
    if (rc == ALT111_OK) gen2_keyframe_intent();
    gen2_publish_status();
}

static void *gen2_output_worker(void *arg)
{
    uint8_t *copy = NULL;
    size_t cap = 0;
    (void)arg;

    for (;;) {
        const uint8_t *p = NULL;
        size_t n = 0, chunk = 0;
        struct alt111_output_ticket ticket;
        int prc, fd, send_failed = 0, would_block = 0, stale_socket = 0;
        int arc = ALT111_WAIT, primed_before = 0, primed_after = 0;
        uint64_t delivered_before = 0, delivered_after = 0;
        ssize_t sent = 0;

        /*
         * Keep the core generation stable from the final peek through the
         * nonblocking socket write and advance. This closes the last race where
         * a codec/consumer generation could be invalidated after bytes were
         * copied but before they were sent.
         *
         * g_lock is nested only inside g2_core_lock here. All other adapter
         * paths release g_lock before acquiring g2_core_lock, so there is no
         * reverse nested order.
         */
        pthread_mutex_lock(&g2_core_lock);
        prc = alt111_video_peek(&g2_video, &p, &n, &ticket);
        if (prc != ALT111_OK || !n) {
            pthread_mutex_unlock(&g2_core_lock);
            usleep(5000);
            continue;
        }

        chunk = n > 65536u ? 65536u : n;
        if (cap < chunk) {
            uint8_t *next = (uint8_t *)realloc(copy, chunk);
            if (!next) {
                pthread_mutex_unlock(&g2_core_lock);
                usleep(10000);
                continue;
            }
            copy = next;
            cap = chunk;
        }
        memcpy(copy, p, chunk);

        delivered_before = g2_video.delivered_aus;
        primed_before = g2_video.consumer_primed;

        pthread_mutex_lock(&g_lock);
        fd = g_tee_client;
        if (fd >= 0 && g_tee_consumer_generation != ticket.consumer) {
            stale_socket = 1;
        } else if (fd >= 0) {
            do {
                sent = send(fd, copy, chunk, MSG_NOSIGNAL);
            } while (sent < 0 && errno == EINTR);

            if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                would_block = 1;
            } else if (sent <= 0) {
                tee_drop_client_locked();
                send_failed = 1;
            }
        } else {
            send_failed = 1;
        }
        pthread_mutex_unlock(&g_lock);

        if (stale_socket) {
            pthread_mutex_unlock(&g2_core_lock);
            usleep(1000);
            continue;
        }

        if (would_block) {
            pthread_mutex_unlock(&g2_core_lock);
            usleep(5000);
            continue;
        }

        if (send_failed) {
            alt111_video_detach(&g2_video);
            pthread_cond_broadcast(&g2_core_cv);
            pthread_mutex_unlock(&g2_core_lock);
            gen2_publish_status();
            usleep(10000);
            continue;
        }

        /*
         * Passive capture mirror: copy only bytes that the primary pre-TS
         * consumer actually accepted. The mirror is nonblocking and may be
         * dropped on any backpressure/partial write; it can never delay or
         * detach the production consumer.
         */
        capture_mirror_send(copy, (size_t)sent, &ticket);

        arc = alt111_video_advance(&g2_video, &ticket, (size_t)sent);
        delivered_after = g2_video.delivered_aus;
        primed_after = g2_video.consumer_primed;
        pthread_mutex_unlock(&g2_core_lock);

        if (arc == ALT111_OK && delivered_after > delivered_before) {
            /*
             * Before a consumer attaches the source heartbeat is enough to let
             * Auto-Direct start the bridge. Once attached, only actual complete
             * AU delivery advances the heartbeat, so DIRECT cannot be declared
             * ready merely on non-IDR source traffic.
             *
             * The first priming AU bypasses the 250 ms throttle: a static map
             * may emit no second frame.
             */
            if (!primed_before && primed_after)
                publish_video_heartbeat_force();
            else
                publish_video_heartbeat();

            if (!primed_before || primed_after != primed_before ||
                (delivered_after & 7u) == 0u)
                gen2_publish_status();
        }
    }
    return NULL;
}

static int read_exact(int fd, void *buf, size_t len)
{
    uint8_t *p = (uint8_t *)buf;
    while (len) {
        ssize_t n = recv(fd, p, len, 0);
        if (n == 0) return 0;
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        p += n; len -= (size_t)n;
    }
    return 1;
}

static void tee_drop_client_locked(void)
{
    if (g_tee_client >= 0) close(g_tee_client);
    g_tee_client = -1;
    g_tee_consumer_generation = 0;
}

static void capture_drop_client_locked(void)
{
    if (g_capture_client >= 0) close(g_capture_client);
    g_capture_client = -1;
    g_capture_wait_idr = 1;
}

static void capture_mirror_send(const uint8_t *p, size_t n,
                                const struct alt111_output_ticket *ticket)
{
    ssize_t w;
    int drop = 0, publish = 0;

    if (!p || !n || !ticket) return;

    pthread_mutex_lock(&g_capture_lock);
    if (g_capture_client < 0) {
        pthread_mutex_unlock(&g_capture_lock);
        return;
    }

    if (g_capture_wait_idr) {
        if (!ticket->idr || ticket->offset != 0) {
            pthread_mutex_unlock(&g_capture_lock);
            return;
        }
        g_capture_wait_idr = 0;
        publish = 1;
    }

    do {
        w = send(g_capture_client, p, n, MSG_NOSIGNAL);
    } while (w < 0 && errno == EINTR);

    if (w != (ssize_t)n) {
        ++g_capture_drops;
        capture_drop_client_locked();
        drop = 1;
        publish = 1;
    } else {
        g_capture_bytes += (uint64_t)n;
        ++g_capture_chunks;
        if ((g_capture_chunks & 63u) == 0u) publish = 1;
    }
    pthread_mutex_unlock(&g_capture_lock);

    if (drop)
        logf_u2("GEN2 capture mirror dropped: nonblocking write rc=%ld expected=%zu; production unaffected",
                (long)w, n);
    if (publish) capture_publish_status(drop ? "dropped" : "recording");
}

static void *tee_accept_thread(void *arg)
{
    (void)arg;
    for (;;) {
        int c = accept(g_tee_listen, NULL, NULL);
        if (c < 0) { if (errno == EINTR) continue; sleep(1); continue; }
        {
            int flags = fcntl(c, F_GETFL, 0);
            if (flags >= 0) (void)fcntl(c, F_SETFL, flags | O_NONBLOCK);
        }
        pthread_mutex_lock(&g_lock);
        tee_drop_client_locked();
        g_tee_client = c;
        pthread_mutex_unlock(&g_lock);
        logf_u2("gen2 renderer connected nonblocking on 127.0.0.1:%d; waiting for config+complete IDR", g_tee_port);
        gen2_consumer_attach();
    }
    return NULL;
}

static int start_tee_server(void)
{
    int actual = 0;
    if (g_tee_started) return 0;
    g_tee_listen = bind_listener_ipv4(g_tee_port, &actual, 1, 0);
    if (g_tee_listen < 0) { logf_u2("cannot bind renderer tee port %d: %s", g_tee_port, strerror(errno)); return -1; }
    g_tee_port = actual;
    if (pthread_create(&g_tee_thread, NULL, tee_accept_thread, NULL) != 0) { close(g_tee_listen); g_tee_listen=-1; return -1; }
    pthread_detach(g_tee_thread);
    g_tee_started = 1;
    logf_u2("renderer tee listening on 127.0.0.1:%d", g_tee_port);
    return 0;
}

static void *capture_accept_thread(void *arg)
{
    (void)arg;
    for (;;) {
        int c = accept(g_capture_listen, NULL, NULL);
        uint8_t cfg[ALT111_CODEC_CAP];
        size_t cfg_n = 0, off = 0;

        if (c < 0) {
            if (errno == EINTR) continue;
            sleep(1);
            continue;
        }

        pthread_mutex_lock(&g2_core_lock);
        if (g2_video.config_valid && g2_video.config_length <= sizeof(cfg)) {
            cfg_n = g2_video.config_length;
            memcpy(cfg, g2_video.config, cfg_n);
        }
        pthread_mutex_unlock(&g2_core_lock);

        /*
         * Seed the standalone .h264 capture with current SPS/PPS when available.
         * This happens only in the diagnostic accept thread, never on the media
         * worker. A fresh forceKeyFrame is then requested and forwarding waits
         * for that IDR before recording pictures.
         */
        while (off < cfg_n) {
            ssize_t w = send(c, cfg + off, cfg_n - off, MSG_NOSIGNAL);
            if (w > 0) {
                off += (size_t)w;
                continue;
            }
            if (w < 0 && errno == EINTR) continue;
            break;
        }
        if (off != cfg_n) {
            close(c);
            logf_u2("GEN2 capture mirror seed failed bytes=%zu/%zu", off, cfg_n);
            continue;
        }

        {
            int flags = fcntl(c, F_GETFL, 0);
            if (flags >= 0) (void)fcntl(c, F_SETFL, flags | O_NONBLOCK);
        }

        pthread_mutex_lock(&g_capture_lock);
        capture_drop_client_locked();
        g_capture_client = c;
        g_capture_wait_idr = 1;
        g_capture_bytes = (uint64_t)cfg_n;
        g_capture_chunks = cfg_n ? 1u : 0u;
        g_capture_drops = 0;
        pthread_mutex_unlock(&g_capture_lock);

        capture_publish_status("waiting_idr");
        logf_u2("GEN2 capture mirror connected on 127.0.0.1:%d seed=%zu; requesting fresh IDR",
                g_capture_port, cfg_n);
        gen2_keyframe_intent();
    }
    return NULL;
}

static int start_capture_server(void)
{
    int actual = 0;
    if (g_capture_started) return 0;
    g_capture_listen = bind_listener_ipv4(g_capture_port, &actual, 1, 0);
    if (g_capture_listen < 0) {
        logf_u2("cannot bind diagnostic H264 capture port %d: %s",
                g_capture_port, strerror(errno));
        capture_publish_status("unavailable");
        return -1;
    }
    g_capture_port = actual;
    if (pthread_create(&g_capture_thread, NULL, capture_accept_thread, NULL) != 0) {
        close(g_capture_listen);
        g_capture_listen = -1;
        capture_publish_status("unavailable");
        return -1;
    }
    pthread_detach(g_capture_thread);
    g_capture_started = 1;
    capture_publish_status("listening");
    logf_u2("diagnostic H264 capture mirror listening on 127.0.0.1:%d", g_capture_port);
    return 0;
}

/*
 * Stop and join the previous stream-111 worker before its global AES/session
 * state can be reused. The worker owns the accepted client fd; stop only
 * shutdowns that fd to wake read_exact(), then joins the worker. This prevents
 * a late cleanup from an old CarPlay session finalising a newly-created AES
 * context after a fast reconnect.
 */
static void nb_stop(void);
static void stop_alt_receiver(void)
{
    nb_stop();
    pthread_t t;
    int do_join = 0;

    pthread_mutex_lock(&g_lock);
    if (g_alt_client >= 0) (void)shutdown(g_alt_client, SHUT_RDWR);
    if (g_alt_listen >= 0) {
        (void)shutdown(g_alt_listen, SHUT_RDWR);
        close(g_alt_listen);
        g_alt_listen = -1;
    }
    if (g_alt_thread_started && !pthread_equal(pthread_self(), g_alt_thread)) {
        t = g_alt_thread;
        g_alt_thread_started = 0;
        do_join = 1;
    }
    pthread_mutex_unlock(&g_lock);

    if (do_join) (void)pthread_join(t, NULL);

    pthread_mutex_lock(&g_lock);
    if (!do_join && g_alt_client >= 0) {
        /* Constructor/failure fallback: no worker owns this descriptor. */
        close(g_alt_client);
        g_alt_client = -1;
    }
    if (g_alt_aes_valid) {
        g_aes_ctr_final(&g_alt_aes);
        g_alt_aes_valid = 0;
    }
    pthread_mutex_unlock(&g_lock);
    gen2_video_end_current();
}

static void *alt_receiver_thread(void *arg)
{
    int listen_fd = (int)(intptr_t)arg;
    int c;
    logf_u2("waiting for iPhone AltScreen connection on 0.0.0.0:%d", g_alt_port);
    c = accept(listen_fd, NULL, NULL);
    if (c < 0) { logf_u2("AltScreen accept failed: %s", strerror(errno)); return NULL; }
    pthread_mutex_lock(&g_lock); g_alt_client = c; pthread_mutex_unlock(&g_lock);
    logf_u2("AltScreen stream 111 connected");
    publish_state("connected");
    for (;;) {
        AirPlayScreenHeaderCompat h;
        uint8_t *body = NULL;
        int rr = read_exact(c, &h, sizeof(h));
        if (rr <= 0) break;
        if (h.bodySize > MAX_SCREEN_BODY) { logf_u2("reject bodySize=%u", h.bodySize); break; }
        if (h.bodySize) {
            body = malloc(h.bodySize);
            if (!body) break;
            rr = read_exact(c, body, h.bodySize);
            if (rr <= 0) { free(body); break; }
        }
        switch (h.opcode) {
            case K_SCREEN_VIDEO_CONFIG:
                if (body && h.bodySize) {
                    float source_w = 0.0f, source_h = 0.0f;
                    memcpy(&source_w, h.params + 8, sizeof(source_w));
                    memcpy(&source_h, h.params + 12, sizeof(source_h));
                    logf_u2("VideoConfig header source=%.1fx%.1f flags=0x%02x body=%u",
                            (double)source_w, (double)source_h,
                            (unsigned)h.smallParam[1], (unsigned)h.bodySize);
                    {
                        int vrc = gen2_video_config(body, h.bodySize);
                        if (vrc == ALT111_OK || vrc == ALT111_RESTART_CONSUMER) {
                            if (!g_video_config_seen) {
                                g_video_config_seen = 1;
                                publish_state("video_config");
                                logf_u2("gen2: stream111 valid transactional video config received");
                            }
                        } else if (vrc == ALT111_INVALID) {
                            g_video_config_seen = 0;
                            g_streaming = 0;
                            clear_video_heartbeat();
                            publish_state("video_config_invalid");
                            logf_u2("gen2: invalid VideoConfig revoked Stream111 readiness");
                        }
                    }
                }
                break;
            case K_SCREEN_VIDEO_FRAME:
                if (body && h.bodySize) {
                    if (!g_alt_aes_valid || g_aes_ctr_update(&g_alt_aes, body, h.bodySize, body) != K_NO_ERR) {
                        logf_u2("AES-CTR decrypt failed"); free(body); goto done;
                    }
                    if (!g_video_config_seen) {
                        logf_u2("video frame ignored before valid avcC config body=%u", (unsigned)h.bodySize);
                        break;
                    }
                    {
                        int accepted = 0, consumer_attached = 0;
                        int vrc = gen2_video_submit_au(body, h.bodySize, &accepted, &consumer_attached);
                        if (accepted) {
                            if (!g_streaming) {
                                g_streaming = 1;
                                publish_state("streaming");
                                logf_u2("gen2: first complete Screen VIDEO_FRAME accepted as AU");
                            }
                            /* Source activity only creates the initial arm
                             * heartbeat. It never advances that heartbeat while
                             * no local consumer exists; otherwise Auto-Direct
                             * could mistake pre-connect source traffic for
                             * post-connect decoder-ready output. */
                            if (!consumer_attached) publish_video_heartbeat_source_arm();
                        }
                        if (vrc == ALT111_INVALID) {
                            g_streaming = 0;
                            clear_video_heartbeat();
                            publish_state("video_invalid");
                            logf_u2("gen2: rejected VIDEO_FRAME body=%u as invalid complete AU; source readiness revoked",
                                    (unsigned)h.bodySize);
                        }
                    }
                }
                break;
            case K_SCREEN_KEEPALIVE:
            case K_SCREEN_KEEPALIVE_BODY:
            case K_SCREEN_IGNORE:
            case K_SCREEN_FORCE_KEYFRAME:
                break;
            default:
                logf_u2("unknown screen opcode=%u", h.opcode);
                break;
        }
        free(body);
    }
done:
    logf_u2("AltScreen stream 111 disconnected");
    clear_video_observer();
    publish_state("disconnected");
    pthread_mutex_lock(&g_lock);
    if (g_alt_client == c) g_alt_client = -1;
    close(c);
    if (g_alt_listen == listen_fd) {
        close(g_alt_listen);
        g_alt_listen = -1;
    }
    if (g_alt_aes_valid) { g_aes_ctr_final(&g_alt_aes); g_alt_aes_valid=0; }
    pthread_mutex_unlock(&g_lock);
    gen2_video_end_current();
    return NULL;
}

static int start_alt_receiver(uint64_t connection_id)
{
    uint8_t key[16], iv[16];
    int actual = 0;
    int allow_ephemeral = env_i("ALTSCREEN111_ALLOW_EPHEMERAL", 0);

    /* Complete previous worker teardown before reusing global session state. */
    stop_alt_receiver();
    clear_video_observer();

    /* Start the core generation before taking g_lock: gen2_video_begin_current()
     * deliberately closes/reset the local consumer and therefore takes g_lock. */
    gen2_video_begin_current();

    pthread_mutex_lock(&g_lock);
    if (!g_master_valid) {
        pthread_mutex_unlock(&g_lock);
        gen2_video_end_current();
        logf_u2("no captured session AES key; cannot start stream111");
        return -1;
    }

    g_derive_screen(g_master_key, 16, connection_id, key, iv);
    memset(&g_alt_aes, 0, sizeof(g_alt_aes));
    if (g_aes_ctr_init(&g_alt_aes, key, iv) != K_NO_ERR) {
        memset(key,0,sizeof(key)); memset(iv,0,sizeof(iv));
        pthread_mutex_unlock(&g_lock);
        gen2_video_end_current();
        return -1;
    }
    memset(key,0,sizeof(key)); memset(iv,0,sizeof(iv));
    g_alt_aes_valid = 1;

    /* iPhone-facing listener: reachable from CarPlay link, not loopback. */
    g_alt_listen = bind_listener_stream111(g_alt_port, &actual, allow_ephemeral);
    if (g_alt_listen < 0) {
        g_aes_ctr_final(&g_alt_aes);
        g_alt_aes_valid=0;
        pthread_mutex_unlock(&g_lock);
        gen2_video_end_current();
        logf_u2("cannot bind iPhone stream111 listener on port %d: %s", g_alt_port, strerror(errno));
        return -1;
    }
    g_alt_port = actual;
    if (pthread_create(&g_alt_thread, NULL, alt_receiver_thread, (void *)(intptr_t)g_alt_listen) != 0) {
        close(g_alt_listen);
        g_alt_listen=-1;
        g_aes_ctr_final(&g_alt_aes);
        g_alt_aes_valid=0;
        pthread_mutex_unlock(&g_lock);
        gen2_video_end_current();
        return -1;
    }
    g_alt_thread_started = 1;
    pthread_mutex_unlock(&g_lock);
    publish_state("listening");
    logf_u2("stream111 receiver ready: conn=%llu dataPort=%d transport=%s",
            (unsigned long long)connection_id,g_alt_port,
            g_alt_listener_ipv6?"IPv6-dualstack":"IPv4");
    return g_alt_port;
}

static void set_str(CFMutableDictionaryRef d, const char *key, const char *val)
{
    CFStringRef k=s_cf(key), v=s_cf(val); p_CFDictionarySetValue(d,k,v); p_CFRelease(v); p_CFRelease(k);
}

static void set_i64(CFMutableDictionaryRef d, const char *key, int64_t val)
{
    CFStringRef k=s_cf(key); p_CFDictionarySetInt64(d,k,val); p_CFRelease(k);
}

static void set_false(CFMutableDictionaryRef d, const char *key)
{
    CFStringRef k;
    if(!d || !key || !p_cfl_boolean_false) return;
    k=s_cf(key);
    if(k){p_CFDictionarySetValue(d,k,p_cfl_boolean_false);p_CFRelease(k);}
}

static void remove_key(CFMutableDictionaryRef d, const char *name)
{
    CFStringRef k;
    if(!d || !name) return;
    k=s_cf(name);
    if(k){p_CFDictionaryRemoveValue(d,k);p_CFRelease(k);}
}

/*
 * iOS 27.2 builds navigator cluster UI candidates and intersects them with the
 * receiver-advertised altScreenSuggestUIURLs list.  Stock MU1440 has no such
 * key because it has no auxiliary cluster display.  Our synthetic type-111
 * display must therefore advertise the roles it intentionally supports.
 *
 * This is capability metadata only: it does not issue showUI/stopUI, rebuild
 * Stream111, change codec state, or synthesize a provider suggestion.
 */
static void add_alt_suggest_ui_urls(CFMutableDictionaryRef alt)
{
    CFMutableArrayRef urls=NULL;
    CFStringRef key=NULL,base=NULL,map=NULL,card=NULL;
    if(!alt)return;

    urls=p_CFArrayCreateMutable(NULL,0,p_array_callbacks);
    if(!urls)return;

    base=s_cf(ALT_URL_DEFAULT);
    map=s_cf(ALT_URL_MAP);
    card=s_cf(ALT_URL_INSTRUCTIONCARD);
    if(!base||!map||!card)goto done;

    p_CFArrayAppendValue(urls,base);
    p_CFArrayAppendValue(urls,map);
    p_CFArrayAppendValue(urls,card);

    key=s_cf("altScreenSuggestUIURLs");
    if(!key)goto done;
    p_CFDictionarySetValue(alt,key,urls);

    logf_u2("GEN2 advertised altScreenSuggestUIURLs[0]=%s [1]=%s [2]=%s",
            ALT_URL_DEFAULT,ALT_URL_MAP,ALT_URL_INSTRUCTIONCARD);

done:
    if(key)p_CFRelease(key);
    if(card)p_CFRelease(card);
    if(map)p_CFRelease(map);
    if(base)p_CFRelease(base);
    if(urls)p_CFRelease(urls);
}

/*
 * Keep the recovered reference ViewArea/SafeArea structure, but do not import
 * the Audi-specific 420x330 safe window into the 1010x376 Skoda VC baseline.
 *
 * Vehicle-PoC policy: initially expose the complete secondary-display canvas
 * as both ViewArea and SafeArea. This lets iOS/the navigation app decide what
 * cluster UI it can render without us prematurely constraining overlays to a
 * narrow center strip. A smaller/tube-specific SafeArea can be added later
 * once the full-width and classic VC layouts have been measured on-car.
 */
static void add_reference_viewarea(CFMutableDictionaryRef alt)
{
    CFMutableArrayRef areas=NULL;
    CFStringRef k=NULL;
    unsigned vi, nviews;

    if(!alt || g_width<=0 || g_height<=0) return;

    nviews=g2_profile.view_count;
    if(nviews<1u) nviews=1u;
    areas=p_CFArrayCreateMutable(NULL,0,p_array_callbacks);
    if(!areas) return;

    for(vi=0;vi<nviews;++vi){
        CFMutableDictionaryRef view=dict_new();
        CFMutableDictionaryRef safe=dict_new();
        int safe_w,safe_h,safe_x,safe_y;

        if(nviews>1u){
            safe_x=(int)g2_profile.views[vi].safe.x;
            safe_y=(int)g2_profile.views[vi].safe.y;
            safe_w=(int)g2_profile.views[vi].safe.width;
            safe_h=(int)g2_profile.views[vi].safe.height;
        }else{
            load_safearea_config(&safe_x,&safe_y,&safe_w,&safe_h);
        }
        if(!view||!safe){
            if(view)p_CFRelease(view);
            if(safe)p_CFRelease(safe);
            break;
        }
        {
            int dx=0, vw=g_width;
            if(vi==1u && nviews>1u){
                char db[16]; int df, dn;
                df=open("/tmp/mibr-alt111-audi.v1org",O_RDONLY);
                if(df>=0){ dn=(int)read(df,db,sizeof(db)-1u); close(df);
                    if(dn>0){ db[dn]='\0'; dx=atoi(db); if(dx<0||dx>=g_width/2) dx=0; } }
                vw=g_width-dx;
                logf_u2("GEN2 view1 outer origin dx=%d width=%d",dx,vw);
            }
            set_i64(view,"widthPixels",vw);
            set_i64(view,"heightPixels",g_height);
            set_i64(view,"originXPixels",dx);
            set_i64(view,"originYPixels",0);
        }
        set_i64(safe,"widthPixels",safe_w);
        set_i64(safe,"heightPixels",safe_h);
        set_i64(safe,"originXPixels",safe_x);
        set_i64(safe,"originYPixels",safe_y);
        set_false(view,"drawUIOutsideSafeArea");
        if(access("/tmp/mibr-alt111-audi.vatc",F_OK)==0){
            CFTypeRef *ts=(CFTypeRef *)dlsym(RTLD_DEFAULT,"kCFLBooleanTrue");
            if(ts&&*ts){
                CFStringRef vk=s_cf("viewAreaTransitionControl");
                if(vk){p_CFDictionarySetValue(view,vk,*ts);p_CFRelease(vk);}
                logf_u2("GEN2 viewAreaTransitionControl=TRUE (vatc marker)");
            }else set_false(view,"viewAreaTransitionControl");
        }else set_false(view,"viewAreaTransitionControl");
        k=s_cf("safeArea");
        p_CFDictionarySetValue(view,k,safe);
        p_CFRelease(k); k=NULL;
        p_CFArrayAppendValue(areas,view);
        logf_u2("GEN2 ViewArea[%u] full=%dx%d safe=%dx%d@%d,%d",
                vi,g_width,g_height,safe_w,safe_h,safe_x,safe_y);
        p_CFRelease(safe);
        p_CFRelease(view);
    }

    k=s_cf("viewAreas");
    p_CFDictionarySetValue(alt,k,areas);
    p_CFRelease(k); k=NULL;
    set_i64(alt,"initialViewArea",0);
    p_CFRelease(areas);
}

static void log_stream_types(const char *tag, CFDictionaryRef request)
{
    CFArrayRef a=get_streams(request);
    CFIndex i,n;
    char b[256];
    int used=0;
    if(!a){logf_u2("%s streams=<none>",tag);return;}
    n=p_CFArrayGetCount(a);
    used=snprintf(b,sizeof(b),"%s streams=%ld types=",tag,(long)n);
    for(i=0;i<n && i<16 && used>0 && used<(int)sizeof(b)-16;++i)
        used+=snprintf(b+used,sizeof(b)-(size_t)used,"%s%d",(i?",":""),stream_type((CFDictionaryRef)p_CFArrayGetValueAtIndex(a,i)));
    logf_u2("%s",b);
}

static const char *flight_command_cstr(CFStringRef command, char *buf, size_t cap)
{
    const char *p;
    if(!buf||cap==0)return "<invalid-buffer>";
    buf[0]='\0';
    if(!command)return "<null>";
    p=p_CFStringGetCStringPtr(command,CF_UTF8);
    if(p&&*p)return p;
    if(p_CFStringGetCString(command,buf,(CFIndex)cap,CF_UTF8) && buf[0])return buf;
    snprintf(buf,cap,"<cfstr:%p>",(void *)command);
    return buf;
}

static int flight_dict_i64(CFDictionaryRef d, const char *key, int64_t *out)
{
    CFStringRef k;
    OSStatus e=0;
    int64_t v;
    if(!d||!key||!out)return 0;
    k=s_cf(key);
    if(!k)return 0;
    v=p_CFDictionaryGetInt64(d,k,&e);
    p_CFRelease(k);
    if(e)return 0;
    *out=v;
    return 1;
}

#define FLIGHT_MAX_DEPTH 4
#define FLIGHT_MAX_CONTAINER_ITEMS 48
#define FLIGHT_MAX_STRING 180

static int flight_sensitive_key(const char *key)
{
    char b[96];
    size_t i,n;
    static const char *needles[]={"token","signature","password","credential","secret","nonce","auth"};
    static const char *identifiers[]={
        "deviceid","macaddress","sessionuuid","sessioncorrelationuuid","name","pi","pk","serial","serialnumber","vin","sn","hostname","bluetoothids"
    };
    if(!key)return 0;
    n=strlen(key);
    if(n>=sizeof(b))n=sizeof(b)-1;
    for(i=0;i<n;++i){
        unsigned char c=(unsigned char)key[i];
        b[i]=(char)((c>='A'&&c<='Z')?(c-'A'+'a'):c);
    }
    b[n]='\0';
    for(i=0;i<sizeof(needles)/sizeof(needles[0]);++i)
        if(strstr(b,needles[i]))return 1;
    /*
     * Flight Recorder logs are routinely copied out of the vehicle and may
     * later be used in public issue/research evidence.  Keep protocol shape
     * but never persist phone/user correlators such as the device name, MAC/
     * device identifier, or per-session UUIDs.
     */
    for(i=0;i<sizeof(identifiers)/sizeof(identifiers[0]);++i)
        if(strcmp(b,identifiers[i])==0)return 1;
    return 0;
}

static const char *flight_cfstring(CFStringRef s, char *buf, size_t cap)
{
    const char *p;
    if(!buf||cap==0)return "<invalid-buffer>";
    buf[0]='\0';
    if(!s)return "<null>";
    p=p_CFStringGetCStringPtr(s,CF_UTF8);
    if(p)return p;
    if(p_CFStringGetCString(s,buf,(CFIndex)cap,CF_UTF8))return buf;
    snprintf(buf,cap,"<cfstr:%p>",(void *)s);
    return buf;
}

static void flight_dump_value(const char *command, const char *path, CFTypeRef obj, int depth);

static void flight_dump_dict(const char *command, const char *path, CFDictionaryRef d, int depth)
{
    CFIndex count,i;
    const void **keys=NULL,**vals=NULL;
    if(!d)return;
    count=p_CFDictionaryGetCount(d);
    logf_u2("GEN2 FLIGHT DUMP command=%s path=%s type=dict count=%ld depth=%d",
            command?command:"<null>",path?path:"params",(long)count,depth);
    if(depth>=FLIGHT_MAX_DEPTH){
        logf_u2("GEN2 FLIGHT DUMP command=%s path=%s stop=max-depth limit=%d",
                command?command:"<null>",path?path:"params",FLIGHT_MAX_DEPTH);
        return;
    }
    if(count<0 || count>FLIGHT_MAX_CONTAINER_ITEMS){
        logf_u2("GEN2 FLIGHT DUMP command=%s path=%s enumeration=skipped count=%ld limit=%d",
                command?command:"<null>",path?path:"params",(long)count,FLIGHT_MAX_CONTAINER_ITEMS);
        return;
    }
    if(count==0)return;
    keys=(const void **)calloc((size_t)count,sizeof(*keys));
    vals=(const void **)calloc((size_t)count,sizeof(*vals));
    if(!keys||!vals){
        logf_u2("GEN2 FLIGHT DUMP command=%s path=%s enumeration=oom count=%ld",
                command?command:"<null>",path?path:"params",(long)count);
        free(keys); free(vals); return;
    }
    p_CFDictionaryGetKeysAndValues(d,keys,vals);
    for(i=0;i<count;++i){
        char kbuf[96],pbuf[256];
        const char *k="<non-string-key>";
        CFTypeRef ko=(CFTypeRef)keys[i];
        if(ko && p_CFGetTypeID(ko)==p_CFStringGetTypeID())
            k=flight_cfstring((CFStringRef)ko,kbuf,sizeof(kbuf));
        snprintf(pbuf,sizeof(pbuf),"%s.%s",path?path:"params",k);
        if(flight_sensitive_key(k)){
            logf_u2("GEN2 FLIGHT DUMP command=%s path=%s type=redacted value=REDACTED",
                    command?command:"<null>",pbuf);
            continue;
        }
        flight_dump_value(command,pbuf,(CFTypeRef)vals[i],depth+1);
    }
    free(keys); free(vals);
}

static void flight_dump_array(const char *command, const char *path, CFArrayRef a, int depth)
{
    CFIndex count,i;
    if(!a)return;
    count=p_CFArrayGetCount(a);
    logf_u2("GEN2 FLIGHT DUMP command=%s path=%s type=array count=%ld depth=%d",
            command?command:"<null>",path?path:"params",(long)count,depth);
    if(depth>=FLIGHT_MAX_DEPTH){
        logf_u2("GEN2 FLIGHT DUMP command=%s path=%s stop=max-depth limit=%d",
                command?command:"<null>",path?path:"params",FLIGHT_MAX_DEPTH);
        return;
    }
    if(count<0 || count>FLIGHT_MAX_CONTAINER_ITEMS){
        logf_u2("GEN2 FLIGHT DUMP command=%s path=%s enumeration=skipped count=%ld limit=%d",
                command?command:"<null>",path?path:"params",(long)count,FLIGHT_MAX_CONTAINER_ITEMS);
        return;
    }
    for(i=0;i<count;++i){
        char pbuf[256];
        snprintf(pbuf,sizeof(pbuf),"%s[%ld]",path?path:"params",(long)i);
        flight_dump_value(command,pbuf,p_CFArrayGetValueAtIndex(a,i),depth+1);
    }
}

static void flight_dump_value(const char *command, const char *path, CFTypeRef obj, int depth)
{
    CFTypeID t;
    if(!obj){
        logf_u2("GEN2 FLIGHT DUMP command=%s path=%s type=null",
                command?command:"<null>",path?path:"params");
        return;
    }
    t=p_CFGetTypeID(obj);
    if(t==p_CFDictionaryGetTypeID()){
        flight_dump_dict(command,path,(CFDictionaryRef)obj,depth);
    }else if(t==p_CFArrayGetTypeID()){
        flight_dump_array(command,path,(CFArrayRef)obj,depth);
    }else if(t==p_CFStringGetTypeID()){
        char b[FLIGHT_MAX_STRING+1];
        const char *v=flight_cfstring((CFStringRef)obj,b,sizeof(b));
        char safe[FLIGHT_MAX_STRING+1];
        size_t i,n=strlen(v);
        if(n>FLIGHT_MAX_STRING)n=FLIGHT_MAX_STRING;
        for(i=0;i<n;++i){
            unsigned char c=(unsigned char)v[i];
            safe[i]=(c>=32 && c<127)?(char)c:'?';
        }
        safe[n]='\0';
        logf_u2("GEN2 FLIGHT DUMP command=%s path=%s type=string value=%s%s",
                command?command:"<null>",path?path:"params",safe,strlen(v)>n?"<truncated>":"");
    }else if(t==p_CFNumberGetTypeID()){
        OSStatus e=0;
        int64_t v=p_CFGetInt64(obj,&e);
        if(!e){
            logf_u2("GEN2 FLIGHT DUMP command=%s path=%s type=int value=%lld%s",
                    command?command:"<null>",path?path:"params",(long long)v,
                    v==111?" MATCH_STREAM111":"");
        }else{
            logf_u2("GEN2 FLIGHT DUMP command=%s path=%s type=number int64_error=%d",
                    command?command:"<null>",path?path:"params",(int)e);
        }
    }else if(t==p_CFBooleanGetTypeID()){
        logf_u2("GEN2 FLIGHT DUMP command=%s path=%s type=bool value=%d",
                command?command:"<null>",path?path:"params",(int)p_CFBooleanGetValue(obj));
    }else if(t==p_CFDataGetTypeID()){
        CFIndex n=p_CFDataGetLength((CFDataRef)obj);
        (void)p_CFDataGetBytePtr((CFDataRef)obj);
        logf_u2("GEN2 FLIGHT DUMP command=%s path=%s type=data len=%ld payload=not-logged",
                command?command:"<null>",path?path:"params",(long)n);
    }else{
        logf_u2("GEN2 FLIGHT DUMP command=%s path=%s type=unknown object=%p typeid=%lu",
                command?command:"<null>",path?path:"params",(void *)obj,(unsigned long)t);
    }
}

static void flight_log_stream_descriptors(const char *command, CFDictionaryRef params)
{
    CFArrayRef a=get_streams(params);
    CFIndex i,n;
    if(!a){
        logf_u2("GEN2 FLIGHT command=%s streams=<none>",command?command:"<null>");
        return;
    }
    n=p_CFArrayGetCount(a);
    logf_u2("GEN2 FLIGHT command=%s streams=%ld",command?command:"<null>",(long)n);
    for(i=0;i<n && i<16;++i){
        CFDictionaryRef sd=(CFDictionaryRef)p_CFArrayGetValueAtIndex(a,i);
        int64_t type=0,cid=0,sid=0,port=0,display=0,screen=0;
        int ht=flight_dict_i64(sd,"type",&type);
        int hc=flight_dict_i64(sd,"streamConnectionID",&cid);
        int hs=flight_dict_i64(sd,"streamID",&sid);
        int hp=flight_dict_i64(sd,"dataPort",&port);
        int hd=flight_dict_i64(sd,"displayID",&display);
        int hscr=flight_dict_i64(sd,"screenID",&screen);
        logf_u2("GEN2 FLIGHT stream[%ld] type=%s%lld streamConnectionID=%s%llu streamID=%s%lld dataPort=%s%lld displayID=%s%lld screenID=%s%lld",
                (long)i,
                ht?"":"<absent>",(long long)(ht?type:0),
                hc?"":"<absent>",(unsigned long long)(hc?(uint64_t)cid:0),
                hs?"":"<absent>",(long long)(hs?sid:0),
                hp?"":"<absent>",(long long)(hp?port:0),
                hd?"":"<absent>",(long long)(hd?display:0),
                hscr?"":"<absent>",(long long)(hscr?screen:0));
    }
}

static void flight_log_selected_scalars(const char *command, CFDictionaryRef params)
{
    static const char *keys[]={
        "type","streamType","streamID","streamConnectionID","dataPort",
        "displayID","screenID","appStateID","state","mode","resourceMode",
        "viewArea","viewAreaIndex","visibility","reason","priority"
    };
    size_t i;
    if(!params)return;
    for(i=0;i<sizeof(keys)/sizeof(keys[0]);++i){
        int64_t v=0;
        if(flight_dict_i64(params,keys[i],&v))
            logf_u2("GEN2 FLIGHT command=%s scalar %s=%lld",
                    command?command:"<null>",keys[i],(long long)v);
    }
}

static void flight_record_platform_control(AirPlayReceiverSessionRef session, uint32_t flags,
                                           CFStringRef command, CFTypeRef qualifier,
                                           CFDictionaryRef params, CFDictionaryRef *outParams)
{
    char cbuf[96];
    const char *c=flight_command_cstr(command,cbuf,sizeof(cbuf));
    if(!audi_verbose()){ logf_u2("GEN2 PC %s",c); return; }
    logf_u2("GEN2 FLIGHT PlatformControl enter session=%p flags=%u command=%s commandRef=%p qualifier=%p params=%p outParams=%p",
            session,(unsigned)flags,c,(void *)command,qualifier,params,outParams);
    if(qualifier)
        flight_dump_value(c,"qualifier",qualifier,0);
    if(params){
        flight_log_selected_scalars(c,params);
        if(get_streams(params))
            flight_log_stream_descriptors(c,params);
        flight_dump_value(c,"params",(CFTypeRef)params,0);
    }
}

static void flight_record_session_control(AirPlayReceiverSessionRef session, uint32_t flags,
                                          CFStringRef command, CFTypeRef qualifier,
                                          CFDictionaryRef params, CFDictionaryRef *outParams)
{
    char cbuf[96];
    const char *c=flight_command_cstr(command,cbuf,sizeof(cbuf));
    if(!audi_verbose()) return;
    logf_u2("GEN2 FLIGHT SessionControl enter session=%p flags=%u command=%s commandRef=%p qualifier=%p params=%p outParams=%p",
            session,(unsigned)flags,c,(void *)command,qualifier,params,outParams);
    if(qualifier)
        flight_dump_value(c,"sessionControl.qualifier",qualifier,0);
    if(params){
        flight_log_selected_scalars(c,params);
        if(get_streams(params))
            flight_log_stream_descriptors(c,params);
        flight_dump_value(c,"sessionControl.params",(CFTypeRef)params,0);
    }
}

static void log_stock_url_capability(CFDictionaryRef stock_display, const char *name)
{
    CFStringRef key;
    CFTypeRef value;
    CFTypeID tid;
    if(!stock_display||!name)return;
    key=s_cf(name);
    if(!key)return;
    value=(CFTypeRef)p_CFDictionaryGetValue(stock_display,key);
    p_CFRelease(key);
    if(!value){
        logf_u2("GEN2 stock display URL capability %s=<absent>",name);
        return;
    }
    tid=p_CFGetTypeID(value);
    if(tid==p_CFArrayGetTypeID()){
        CFIndex i,n=p_CFArrayGetCount((CFArrayRef)value);
        logf_u2("GEN2 stock display URL capability %s=array count=%ld",name,(long)n);
        for(i=0;i<n && i<8;++i){
            CFTypeRef item=(CFTypeRef)p_CFArrayGetValueAtIndex((CFArrayRef)value,i);
            if(item && p_CFGetTypeID(item)==p_CFStringGetTypeID()){
                char b[192];
                const char *u=flight_command_cstr((CFStringRef)item,b,sizeof(b));
                logf_u2("GEN2 stock display URL capability %s[%ld]=%s",name,(long)i,u);
            }else{
                logf_u2("GEN2 stock display URL capability %s[%ld]=<type:%lu>",
                        name,(long)i,(unsigned long)(item?p_CFGetTypeID(item):0));
            }
        }
    }else{
        logf_u2("GEN2 stock display URL capability %s=<type:%lu>",
                name,(unsigned long)tid);
    }
}

/* audi-mu0678-r4 bisect helpers -------------------------------------------
 * /tmp/mibr-alt111-audi.stage : integer N. Display clone is built cumulatively:
 *   0 exact unmodified copy of the stock display appended
 *   1 + remove primaryInputDevice/edid/platformLayer/avcc/offsets/window*
 *   2 + type/maxFPS/features from profile
 *   3 + widthPixels/heightPixels/widthPhysical/heightPhysical
 *   4 + initialURL (uuid is always set to the unique GEN2 uuid, even at stage 0)
 *   5 + altScreenSuggestUIURLs
 *   6 + viewAreas (full GEN2 behaviour). Absent file == 7 (r24).
 *   7 + virtual knob HID device + primaryInputDevice (as the working MHI2Q package does)
 * /tmp/mibr-alt111-audi.verbose : re-enable full FLIGHT dumps (default quiet).
 */
static int audi_stage(void)
{
    FILE *f=fopen("/tmp/mibr-alt111-audi.stage","r");
    int v=7,t; /* r24: stage 7 (features=2 + knob HID) is the fps fix; default */
    if(f){ if(fscanf(f,"%d",&t)==1) v=t; fclose(f); }
    return v;
}
static int audi_verbose(void)
{ return access("/tmp/mibr-alt111-audi.verbose",F_OK)==0; }

/* Stage 7 (r4): mirror the working MHI2Q design - advertise a virtual knob HID device tied to
 * the cluster display (clone of the stock "Eso_Knob" entry, new uuid, displayUUID=alt uuid) and
 * point the alt display's primaryInputDevice at it. Advertise-only; no HID stream is served. */
/* r45-knobtest: test harness for sending knob HID reports to the phone.
 *  /tmp/mibr-alt111-knob          content: "<stock|clone> <buttons> <x> <y> <wheel>"  -> posts a 4-byte report
 *                                 (byte0 bits: 1=select 2=home 4=back; bytes1-3 signed: x, y, wheel), then a zero report
 *  /tmp/mibr-alt111-knob-register (marker) -> also create+register a real virtual HID device for the cluster knob
 * HIDPostReport(uuid_string, report, len) finds a registered HID device by uuid and posts to it. */
static char g_knob_stock_uuid[96];
/* r46-cfuuid: HIDPostReport takes the device uuid as a CFString (its log uses %@), not a C string */
typedef int (*r45_post_t)(CFStringRef uuid, const void *report, size_t len);
typedef int (*r45_create_t)(void **out, CFDictionaryRef props);
typedef int (*r45_reg_t)(void *dev);
/* r47-wire: a device created with HIDDeviceCreateVirtual has no event handler, so HIDPostReport on it is dropped.
 * The session attaches its handler (device+0x14) and context (device+0x18) to the stock knob; copy both onto our
 * cluster knob. The handler is called as handler(dev, 2, 0, report, len, ctx) and tags the report with dev's uuid (dev+52). */
static int r47_uuid_bytes(const char *s, unsigned char out[16])
{
    int i=0,hi=-1,v;
    for(;*s && i<16;++s){
        char ch=*s;
        if(ch=='-') continue;
        if(ch>='0'&&ch<='9') v=ch-'0'; else if(ch>='a'&&ch<='f') v=ch-'a'+10; else if(ch>='A'&&ch<='F') v=ch-'A'+10; else return -1;
        if(hi<0) hi=v; else { out[i++]=(unsigned char)(hi*16+v); hi=-1; }
    }
    return i==16 ? 0 : -1;
}
/* r49-cb: logging wrapper around the session's HID handler copied onto the clone knob (diagnostic). */
static void *g_r49_real;
static int r49_cb(void *dev, int a, int b, const unsigned char *rep, unsigned len, void *ctx)
{
    typedef int (*fn_t)(void *,int,int,const unsigned char *,unsigned,void *);
    int r=-12345; static int n;
    if(n++<24){
        unsigned char *u=(unsigned char *)dev+52;
        logf_u2("r49 cb: dev=%p a=%d b=%d len=%u rep=%02x %02x %02x %02x ctx=%p uuid=%02x%02x%02x%02x",dev,a,b,len,
                (rep&&len>0)?rep[0]:0,(rep&&len>1)?rep[1]:0,(rep&&len>2)?rep[2]:0,(rep&&len>3)?rep[3]:0,ctx,u[0],u[1],u[2],u[3]);
    }
    if(g_r49_real) r=((fn_t)g_r49_real)(dev,a,b,rep,len,ctx);
    if(n<=24) logf_u2("r49 cb: real handler returned %d",r);
    return r;
}
typedef CFArrayRef (*r47_copy_t)(int *err);
static void r47_wire_clone(void)
{
    unsigned char su[16],cu[16];
    r47_copy_t cp=(r47_copy_t)sym_next("HIDCopyDevices");
    CFArrayRef arr; int err=0,nclone=0; CFIndex i,n; unsigned char *stock=NULL;
    void *h,*cx;
    if(!cp){ logf_u2("r47 wire: HIDCopyDevices not found"); return; }
    if(r47_uuid_bytes(g_knob_stock_uuid,su)!=0 || r47_uuid_bytes("b7e6c5a0-3333-4000-8000-000000000003",cu)!=0){ logf_u2("r47 wire: uuid parse failed (stock='%s')",g_knob_stock_uuid); return; }
    arr=cp(&err);
    if(!arr){ logf_u2("r47 wire: HIDCopyDevices returned NULL err=%d",err); return; }
    n=p_CFArrayGetCount(arr);
    for(i=0;i<n;++i){
        unsigned char *dv=(unsigned char *)p_CFArrayGetValueAtIndex(arr,i);
        if(dv && memcmp(dv+52,su,16)==0) stock=dv;
    }
    if(!stock){ logf_u2("r47 wire: stock device not in list (n=%ld)",(long)n); p_CFRelease(arr); return; }
    h=*(void **)(stock+0x14); cx=*(void **)(stock+0x18);
    if(!h){ logf_u2("r47 wire: stock device has no handler yet (dev=%p)",(void*)stock); p_CFRelease(arr); return; }
    for(i=0;i<n;++i){
        unsigned char *dv=(unsigned char *)p_CFArrayGetValueAtIndex(arr,i);
        if(dv && memcmp(dv+52,cu,16)==0){ g_r49_real=h; *(void **)(dv+0x14)=(access("/tmp/mibr-alt111-knob-nowrap",F_OK)==0)?h:(void *)r49_cb; *(void **)(dv+0x18)=cx; ++nclone; }
    }
    logf_u2("r47 wire: stock dev=%p handler=%p ctx=%p -> wired %d clone device(s) of %ld listed",(void*)stock,h,cx,nclone,(long)n);
    p_CFRelease(arr);
}
static void r45_knob_poll(void)
{
    static const char *mk="/tmp/mibr-alt111-knob";
    FILE *f; char line[160], which[16]; int b=0,x=0,y=0,w=0,rc,rc2; const char *u; unsigned char rep[4]; r45_post_t post;
    if(access(mk,F_OK)!=0) return;
    line[0]=0;
    f=fopen(mk,"r");
    if(f){ if(!fgets(line,sizeof(line),f)) line[0]=0; fclose(f); }
    unlink(mk);
    which[0]=0;
    if(sscanf(line,"%15s %d %d %d %d",which,&b,&x,&y,&w)<1) return;
    u=(strcmp(which,"clone")==0) ? "b7e6c5a0-3333-4000-8000-000000000003" : g_knob_stock_uuid;
    if(!u[0]){ logf_u2("r45 knob: no uuid known for '%s'",which); return; }
    post=(r45_post_t)sym_next("HIDPostReport");
    if(!post){ logf_u2("r45 knob: HIDPostReport not found"); return; }
    if(strcmp(which,"clone")==0) r47_wire_clone();
    rep[0]=(unsigned char)(b&7); rep[1]=(unsigned char)(signed char)x; rep[2]=(unsigned char)(signed char)y; rep[3]=(unsigned char)(signed char)w;
    {   CFStringRef us=s_cf(u);
        if(!us){ logf_u2("r45 knob: CFString create failed"); return; }
        rc=post(us,rep,4);
        usleep(30000);
        memset(rep,0,sizeof(rep));
        rc2=post(us,rep,4);
        p_CFRelease(us);
    }
    logf_u2("r45 knob post %s uuid=%s b=%d x=%d y=%d w=%d rc=%d release rc=%d",which,u,b,x,y,w,rc,rc2);
}
static void audi_hid_knob(CFMutableDictionaryRef info, CFMutableDictionaryRef alt, int apply)
{
    static const char *knob_uuid="b7e6c5a0-3333-4000-8000-000000000003";
    CFStringRef kh=s_cf("hidDevices"), kn=s_cf("name");
    CFArrayRef old;
    CFMutableArrayRef arr=NULL;
    CFMutableDictionaryRef k=NULL;
    CFIndex i,n,pick=-1;
    char b[128];
    if(!kh||!kn) goto out;
    old=(CFArrayRef)p_CFDictionaryGetValue(info,kh);
    if(!old || (n=p_CFArrayGetCount(old))<=0){ logf_u2("AUDI hidDevices absent"); goto out; }
    for(i=0;i<n;++i){
        CFDictionaryRef d=(CFDictionaryRef)p_CFArrayGetValueAtIndex(old,i);
        CFTypeRef nm=d?(CFTypeRef)p_CFDictionaryGetValue(d,kn):NULL;
        const char *ns="<none>";
        if(nm && p_CFGetTypeID(nm)==p_CFStringGetTypeID()) ns=flight_cfstring((CFStringRef)nm,b,sizeof(b));
        logf_u2("AUDI hidDevices[%ld] name=%s",(long)i,ns);
        if(pick<0 && strcmp(ns,"Eso_Knob")==0) pick=i;
    }
    if(!apply) goto out;
    if(pick<0){ logf_u2("AUDI no Eso_Knob hid device found; stage 7 not applied"); goto out; }
    k=dict_clone((CFDictionaryRef)p_CFArrayGetValueAtIndex(old,pick));
    arr=p_CFArrayCreateMutableCopy(NULL,0,old);
    if(!k||!arr) goto out;
    {   /* r45: remember the stock knob uuid (changes every session) for the test harness */
        CFStringRef ku=s_cf("uuid"); CFTypeRef uv=ku?(CFTypeRef)p_CFDictionaryGetValue(k,ku):NULL;
        g_knob_stock_uuid[0]=0;
        if(uv && p_CFGetTypeID(uv)==p_CFStringGetTypeID()){ char ub[96]; const char *us=flight_cfstring((CFStringRef)uv,ub,sizeof(ub)); snprintf(g_knob_stock_uuid,sizeof(g_knob_stock_uuid),"%s",us?us:""); }
        logf_u2("r45 stock knob uuid=%s",g_knob_stock_uuid);
        if(ku)p_CFRelease(ku);
    }
    set_str(k,"name","Audi_ClusterKnob");
    set_str(k,"uuid",knob_uuid);
    set_str(k,"displayUUID",g_alt_uuid);
    if(access("/tmp/mibr-alt111-knob-register",F_OK)==0){
        r45_create_t cr=(r45_create_t)sym_next("HIDDeviceCreateVirtual"); r45_reg_t rg=(r45_reg_t)sym_next("HIDRegisterDevice");
        if(cr && rg){ void *dev=NULL; int rc=cr(&dev,(CFDictionaryRef)k); int rc2=-1; if(rc==0 && dev) rc2=rg(dev); logf_u2("r45 knob register: create rc=%d dev=%p register rc=%d",rc,dev,rc2); }
        else logf_u2("r45 knob register: symbols missing create=%p register=%p",(void*)cr,(void*)rg);
    }
    p_CFArrayAppendValue(arr,k);
    p_CFDictionarySetValue(info,kh,arr);
    set_str(alt,"primaryInputDevice",knob_uuid);
    logf_u2("AUDI stage7: cloned hidDevices[%ld] as Audi_ClusterKnob displayUUID=alt; alt.primaryInputDevice set",(long)pick);
out:
    if(arr)p_CFRelease(arr);
    if(k)p_CFRelease(k);
    if(kh)p_CFRelease(kh);
    if(kn)p_CFRelease(kn);
}

CFDictionaryRef AirPlayCopyServerInfo(AirPlayReceiverSessionRef session, CFArrayRef properties, uint8_t *mac, OSStatus *outErr)
{
    CFDictionaryRef base,stock_display;
    CFMutableDictionaryRef info=NULL,alt=NULL;
    CFStringRef kdisplays=NULL;
    CFArrayRef old=NULL;
    CFMutableArrayRef displays=NULL;

    if(!g_real_serverinfo)g_real_serverinfo=(fn_serverinfo_t)sym_next("AirPlayCopyServerInfo");
    if(!g_real_serverinfo){
        logf_u2("AirPlayCopyServerInfo stock delegate unavailable");
        if(outErr)*outErr=-1;
        return NULL;
    }
    base=g_real_serverinfo(session,properties,mac,outErr);
    if(!g_enabled||!base)return base;

    info=dict_clone(base);
    if(!info)return base;

    /* Reference order: root capability first, display transformation second. */
    apply_airplay_bit26_ab(info);
    set_reference_enabled_features(info);
    if(access(g_audi_nodisp_marker,F_OK)==0){
        logf_u2("AUDI /info: no-alt-display marker present; stock displays unchanged");
        p_CFRelease(base);
        return info;
    }

    kdisplays=s_cf("displays");
    old=(CFArrayRef)p_CFDictionaryGetValue(base,kdisplays);
    if(!old || p_CFArrayGetCount(old)<=0){
        logf_u2("IRC-parity /info: root features added but stock displays missing");
        p_CFRelease(kdisplays);
        p_CFRelease(base);
        return info;
    }

    stock_display=(CFDictionaryRef)p_CFArrayGetValueAtIndex(old,0);
    /*
     * iOS 27.2 intersects provider suggestions with receiver-advertised
     * cluster URL capabilities.  Observe the exact stock MU1440 baseline
     * before mutating the clone; do not synthesize capability arrays here.
     */
    {   static int root_dumped=0;
        if(!root_dumped){ root_dumped=1; flight_dump_value("info","root",(CFTypeRef)base,0); }
    }
    flight_dump_value("info","stockDisplay0",(CFTypeRef)stock_display,0);
    log_stock_url_capability(stock_display,"altScreenSuggestUIURLs");
    log_stock_url_capability(stock_display,"altScreenURLs");
    log_stock_url_capability(stock_display,"uiContextURLs");
    {
        OSStatus fe=0, ie=0;
        CFStringRef fk=s_cf("features"), ik=s_cf("primaryInputDevice");
        int64_t fv=p_CFDictionaryGetInt64(stock_display,fk,&fe);
        int64_t iv=p_CFDictionaryGetInt64(stock_display,ik,&ie);
        logf_u2("GEN2 stock display capability baseline: features=%s%lld primaryInputDevice=%s%lld; alt clone removes primaryInputDevice",
                fe?"<absent>":"",(long long)(fe?0:fv),
                ie?"<absent>":"",(long long)(ie?0:iv));
        p_CFRelease(ik); p_CFRelease(fk);
    }
    displays=p_CFArrayCreateMutableCopy(NULL,0,old);
    alt=dict_clone(stock_display);
    if(displays&&alt){
        char active_url[384];
        active_alt_url_copy(active_url,sizeof(active_url));
        int stage=audi_stage();
        {   /* optional runtime size override: file /tmp/mibr-alt111-audi.size = "W H" */
            FILE *sf=fopen("/tmp/mibr-alt111-audi.size","r");
            int ow=0,oh=0;
            if(sf){
                if(fscanf(sf,"%d %d",&ow,&oh)==2 && ow>=64 && oh>=64 && ow<=4096 && oh<=4096){
                    g_width=ow; g_height=oh;
                    g2_profile.width=(uint32_t)ow; g2_profile.height=(uint32_t)oh;
                    g2_profile.width_mm=200; g2_profile.height_mm=(uint32_t)((200*oh)/ow);
                    logf_u2("AUDI size override %dx%d physical=200x%u",ow,oh,g2_profile.height_mm);
                }
                fclose(sf);
            }
        }
        logf_u2("AUDI display clone stage=%d (uuid always unique)",stage);
        set_str(alt,"uuid",g_alt_uuid);
        if(stage>=1){
        /* Exact reference removal set before AltScreen-specific overrides. */
        remove_key(alt,"primaryInputDevice");
        remove_key(alt,"edid");
        remove_key(alt,"platformLayer");
        remove_key(alt,"avcc");
        remove_key(alt,"xOffset");
        remove_key(alt,"yOffset");
        remove_key(alt,"windowWidth");
        remove_key(alt,"windowHeight");
        }
        if(stage>=2){
        set_i64(alt,"type",(int64_t)g2_profile.type);
        set_i64(alt,"maxFPS",(int64_t)g2_profile.max_fps);
        {   /* r50-feat: /tmp/mibr-alt111-audi.featv holds an integer to advertise as the alt display "features" (test) */
            int64_t fv=(int64_t)g2_profile.features; FILE *ff=fopen("/tmp/mibr-alt111-audi.featv","r"); long lv;
            if(ff){ if(fscanf(ff,"%ld",&lv)==1) fv=(int64_t)lv; fclose(ff); logf_u2("r50 features override=%ld",(long)fv); }
            set_i64(alt,"features",fv);
        }
        }
        if(stage>=3){
        set_i64(alt,"widthPixels",(int64_t)g2_profile.width);
        set_i64(alt,"heightPixels",(int64_t)g2_profile.height);
        set_i64(alt,"widthPhysical",(int64_t)g2_profile.width_mm);
        set_i64(alt,"heightPhysical",(int64_t)g2_profile.height_mm);
        }
        if(stage>=4){
        set_str(alt,"uuid",g_alt_uuid);
        set_str(alt,"initialURL",active_url);
        }
        if(stage>=5) add_alt_suggest_ui_urls(alt);
        if(stage>=6 && g_viewareas)add_reference_viewarea(alt);
        audi_hid_knob(info,alt,stage>=7);

        p_CFArrayAppendValue(displays,alt);
        p_CFDictionarySetValue(info,kdisplays,displays);
        logf_u2("GEN2 /info ready: root=altScreen%s type=%u maxFPS=%u features=%u input=none geometry=%ux%u physical=%ux%u uuid=%s url=%s",
                g_viewareas?"+viewAreas":"",
                g2_profile.type,g2_profile.max_fps,g2_profile.features,
                g2_profile.width,g2_profile.height,g2_profile.width_mm,g2_profile.height_mm,
                g_alt_uuid,active_url);
    }else{
        logf_u2("IRC-parity /info: display clone failed");
    }

    if(alt)p_CFRelease(alt);
    if(displays)p_CFRelease(displays);
    p_CFRelease(kdisplays);
    p_CFRelease(base);
    return info;
}

static CFMutableDictionaryRef command_showui(void)
{
    char active_url[384];
    CFMutableDictionaryRef req=dict_new(), params=dict_new();
    if(!req||!params){if(req)p_CFRelease(req);if(params)p_CFRelease(params);return NULL;}
    active_alt_url_copy(active_url,sizeof(active_url));
    set_str(req,"type","showUI"); set_str(params,"uuid",g_alt_uuid); set_str(params,"url",active_url);
    { CFStringRef k=s_cf("params"); p_CFDictionarySetValue(req,k,params); p_CFRelease(k); }
    p_CFRelease(params); return req;
}

static CFMutableDictionaryRef command_force_keyframe(void)
{
    CFMutableDictionaryRef req=dict_new(), params=dict_new();
    if(!req||!params){if(req)p_CFRelease(req);if(params)p_CFRelease(params);return NULL;}
    set_str(req,"type","forceKeyFrame"); set_str(params,"uuid",g_alt_uuid);
    { CFStringRef k=s_cf("params"); p_CFDictionarySetValue(req,k,params); p_CFRelease(k); }
    p_CFRelease(params); return req;
}

static CFMutableDictionaryRef command_stopui(void)
{
    CFMutableDictionaryRef req=dict_new(), params=dict_new();
    if(!req||!params){if(req)p_CFRelease(req);if(params)p_CFRelease(params);return NULL;}
    set_str(req,"type","stopUI"); set_str(params,"uuid",g_alt_uuid);
    { CFStringRef k=s_cf("params"); p_CFDictionarySetValue(req,k,params); p_CFRelease(k); }
    p_CFRelease(params); return req;
}

static CFMutableDictionaryRef command_update_view(unsigned view)
{
    CFMutableDictionaryRef req=dict_new(), params=dict_new();
    if(!req||!params){if(req)p_CFRelease(req);if(params)p_CFRelease(params);return NULL;}
    set_str(req,"type","updateViewArea");
    set_str(params,"uuid",g_alt_uuid);
    set_i64(params,"viewAreaIndex",(int64_t)view);
    {
        char ab[16]; int af, an, ms=0;
        af=open("/tmp/mibr-alt111-audi.anim",O_RDONLY);
        if(af>=0){ an=(int)read(af,ab,sizeof(ab)-1u); close(af);
            if(an>0){ ab[an]='\0'; ms=atoi(ab); if(ms<0||ms>5000) ms=0; } }
        set_i64(params,"animationDurationMillis",(int64_t)ms);
        logf_u2("GEN2 updateViewArea index=%u animationMs=%d",view,ms);
    }
    {
        char mb[8]; int mf, mn, mode=1;
        mf=open("/tmp/mibr-alt111-audi.adj",O_RDONLY);
        if(mf>=0){ mn=(int)read(mf,mb,sizeof(mb)-1u); close(mf);
            if(mn>0){ mb[mn]='\0'; mode=atoi(mb); } }
        if(mode==1||mode==2){
            typedef CFTypeRef (*adj_numcreate_t)(CFTypeRef,int,const void *);
            adj_numcreate_t nc=(adj_numcreate_t)dlsym(RTLD_DEFAULT,"CFNumberCreate");
            CFMutableArrayRef adj=nc?p_CFArrayCreateMutable(NULL,0,p_array_callbacks):NULL;
            if(adj){
                int64_t list[8]; int cnt=0, ai;
                if(mode==1){ unsigned vv; for(vv=0;vv<g2_profile.view_count && cnt<8;++vv) if(vv!=view) list[cnt++]=(int64_t)vv; }
                else { list[cnt++]=0; list[cnt++]=1; }
                for(ai=0;ai<cnt;++ai){
                    CFTypeRef num=nc(NULL,4,&list[ai]);
                    if(num){ p_CFArrayAppendValue(adj,num); p_CFRelease(num); }
                }
                { CFStringRef ak=s_cf("adjacentViewAreas");
                  if(ak){ p_CFDictionarySetValue(params,ak,adj); p_CFRelease(ak); } }
                logf_u2("GEN2 updateViewArea adjacentViewAreas mode=%d count=%d",mode,cnt);
                p_CFRelease(adj);
            }
        }
    }
    { CFStringRef k=s_cf("params"); p_CFDictionarySetValue(req,k,params); p_CFRelease(k); }
    p_CFRelease(params); return req;
}

static CFMutableDictionaryRef gen2_command_dictionary(const struct alt111_command *cmd)
{
    if (!cmd) return NULL;
    switch (cmd->type) {
    case ALT111_CMD_SHOW: return command_showui();
    case ALT111_CMD_STOP: return command_stopui();
    case ALT111_CMD_VIEW: return command_update_view(cmd->view);
    case ALT111_CMD_KEYFRAME: return command_force_keyframe();
    default: return NULL;
    }
}

static void gen2_publish_status(void)
{
    char b[3072];
    int fd, n;
    struct alt111_control cs;
    struct alt111_video vs;
    struct alt111_resync rs;
    mibr_mode_state_t ms;
    unsigned mode_valid;
    uint64_t mode_sequence;
    unsigned command_ready;
    uint64_t last_dispatched, last_completed;
    int last_completion_status;
    unsigned d2_enabled, d2_pending_sources;
    unsigned nav_query_enabled;
    char nav_url[384];
    uint64_t d2_pending_due_ms, d2_last_request_ms, d2_last_idr_ms;
    uint64_t d2_event_triggers, d2_watchdog_triggers, d2_coalesced;
    pthread_mutex_lock(&g2_core_lock);
    cs = g2_control;
    vs = g2_video;
    rs = g2_resync;
    ms = g2_last_mode_state;
    mode_valid = g2_last_mode_valid;
    mode_sequence = g2_mode_sequence;
    command_ready = g2_command_ready;
    last_dispatched = g2_last_dispatched_request;
    last_completed = g2_last_completed_request;
    last_completion_status = g2_last_completion_status;
    d2_enabled = gen2_d2_enabled();
    d2_pending_sources = g2_d2_pending_sources;
    d2_pending_due_ms = g2_d2_pending_due_ms;
    d2_last_request_ms = g2_d2_last_request_ms;
    d2_last_idr_ms = g2_d2_last_idr_ms;
    d2_event_triggers = g2_d2_event_triggers;
    d2_watchdog_triggers = g2_d2_watchdog_triggers;
    d2_coalesced = g2_d2_coalesced;
    pthread_mutex_unlock(&g2_core_lock);
    nav_query_enabled = access(g_nav_query_enable_marker,F_OK)==0 ? 1u : 0u;
    active_alt_url_copy(nav_url,sizeof(nav_url));
    n = snprintf(b,sizeof(b),
        "gen2=1\ncontrol_session=%llu\ncommand_ready=%u\nprojection_desired=%u\nshown_ack=%u\nreacquiring=%u\n"
        "last_dispatched_request=%llu\nlast_completed_request=%llu\nlast_completion_status=%d\n"
        "nav_query_enabled=%u\nnav_url=%s\n"
        "stream_gen=%llu\ncodec_gen=%llu\nconsumer_gen=%llu\nconfig_valid=%u\n"
        "source_aus=%llu\nsource_idrs=%llu\nconsumer_primed=%u\ndelivered_aus=%llu\n"
        "dropped_aus=%llu\nqueue_count=%u\nqueue_bytes=%zu\n"
        "resync_enabled=%u\nresync_state=%u\nresync_reason=%u\nresync_cancel_reason=%u\n"
        "resync_epoch=%llu\nresync_stream_at_arm=%llu\nresync_codec_at_arm=%llu\n"
        "resync_consumer_at_arm=%llu\nresync_au_at_arm=%llu\nresync_idr_at_arm=%llu\n"
        "resync_requests=%llu\nresync_retries=%llu\nresync_completions=%llu\nresync_cancels=%llu\n"
        "resync_last_request_ms=%llu\nresync_next_request_ms=%llu\nresync_completed_ms=%llu\n"
        "resync_retry_ms=%u\n"
        "d2_enabled=%u\nd2_pending_sources=%u\nd2_pending_due_ms=%llu\n"
        "d2_last_request_ms=%llu\nd2_last_idr_ms=%llu\n"
        "d2_event_triggers=%llu\nd2_watchdog_triggers=%llu\nd2_coalesced=%llu\n"
        "mode_valid=%u\nmode_sequence=%llu\nmode_screen=%d\nmode_main_audio=%d\n"
        "mode_speech=%d\nmode_speech_detail=%d\nmode_phone=%d\nmode_turns=%d\n",
        (unsigned long long)cs.session,command_ready,cs.desired,cs.shown_ack,cs.reacquiring,
        (unsigned long long)last_dispatched,(unsigned long long)last_completed,last_completion_status,
        nav_query_enabled,nav_url,
        (unsigned long long)vs.stream,(unsigned long long)vs.codec,
        (unsigned long long)vs.consumer,vs.config_valid,
        (unsigned long long)vs.source_aus,(unsigned long long)vs.source_idrs,
        vs.consumer_primed,(unsigned long long)vs.delivered_aus,
        (unsigned long long)vs.dropped_aus,vs.count,vs.queued_bytes,
        rs.enabled,rs.state,rs.reason,rs.cancel_reason,
        (unsigned long long)rs.epoch,(unsigned long long)rs.stream_at_arm,
        (unsigned long long)rs.codec_at_arm,(unsigned long long)rs.consumer_at_arm,
        (unsigned long long)rs.au_at_arm,(unsigned long long)rs.idr_at_arm,
        (unsigned long long)rs.requests,(unsigned long long)rs.retries,
        (unsigned long long)rs.completions,(unsigned long long)rs.cancels,
        (unsigned long long)rs.last_request_ms,(unsigned long long)rs.next_request_ms,
        (unsigned long long)rs.completed_ms,rs.retry_ms,
        d2_enabled,d2_pending_sources,(unsigned long long)d2_pending_due_ms,
        (unsigned long long)d2_last_request_ms,(unsigned long long)d2_last_idr_ms,
        (unsigned long long)d2_event_triggers,(unsigned long long)d2_watchdog_triggers,
        (unsigned long long)d2_coalesced,
        mode_valid,(unsigned long long)mode_sequence,
        ms.screen,ms.main_audio,ms.speech,ms.speech_detail,ms.phone,ms.turns);
    if(n<=0)return;
    if((size_t)n>=sizeof(b))n=(int)sizeof(b)-1;
    pthread_mutex_lock(&g2_status_lock);
    fd=open(g2_status_path,O_WRONLY|O_CREAT|O_TRUNC,0644);
    if(fd>=0){(void)write(fd,b,(size_t)n);close(fd);}
    pthread_mutex_unlock(&g2_status_lock);
}

static void gen2_set_command_ready(unsigned ready)
{
    pthread_mutex_lock(&g2_core_lock);
    g2_command_ready = ready ? 1u : 0u;
    pthread_cond_broadcast(&g2_core_cv);
    pthread_mutex_unlock(&g2_core_lock);
    gen2_publish_status();
}

static void gen2_control_projection_on(void)
{
    int repeated = 0, rrc = ALT111_OK;
    pthread_mutex_lock(&g2_core_lock);
    if (!g2_control_session) {
        g2_control_session = alt111_control_begin(&g2_control);
    } else {
        repeated = 1;
    }
    (void)alt111_control_intent(&g2_control,1,
        g2_profile.view_count>1u ? g2_want_view : 0u);
    if (repeated)
        rrc = alt111_control_reacquire(&g2_control,g2_control_session);
    pthread_cond_broadcast(&g2_core_cv);
    pthread_mutex_unlock(&g2_core_lock);
    if (repeated)
        logf_u2("gen2 repeated stream111 SETUP -> ownership reacquire rc=%d",rrc);
    gen2_publish_status();
}

static void gen2_control_release(void)
{
    uint64_t s;
    int stop_handed_off = 0;
    pthread_mutex_lock(&g2_core_lock);
    s=g2_control_session;
    alt111_resync_cancel(&g2_resync, ALT111_RESYNC_CANCEL_PROJECTION);
    if(s)(void)alt111_control_intent(&g2_control,0,0);
    pthread_cond_broadcast(&g2_core_cv);
    pthread_mutex_unlock(&g2_core_lock);

    /*
     * Do not confuse SendCommand() queueing with the controller response.
     * During teardown we cannot wait indefinitely for the HTTP completion,
     * but we do wait until either:
     *   - stopUI has completed successfully (may_be_visible == 0), or
     *   - the STOP request has at least been handed to AirPlay's HTTP client.
     * A later callback is generation-checked by the core and may go stale
     * after alt111_control_end(), which is intentional.
     */
    if(s){
        unsigned waitn;
        for(waitn=0; waitn<100; ++waitn){
            pthread_mutex_lock(&g2_core_lock);
            if(!g2_control.may_be_visible){
                stop_handed_off = 1;
            }else if(g2_control.pending.type == ALT111_CMD_STOP &&
                     g2_last_dispatched_request == g2_control.pending.request){
                stop_handed_off = 1;
            }
            pthread_mutex_unlock(&g2_core_lock);
            if(stop_handed_off)break;
            usleep(5000);
        }
        if(!stop_handed_off)
            logf_u2("gen2 stopUI not handed off within bounded teardown window session=%llu",
                    (unsigned long long)s);
    }

    pthread_mutex_lock(&g2_core_lock);
    if(s)(void)alt111_control_end(&g2_control,s);
    g2_control_session=0;
    pthread_mutex_unlock(&g2_core_lock);
    gen2_publish_status();
}

struct gen2_command_context {
    uint64_t session;
    uint64_t request;
    unsigned type;
};

static void gen2_command_completion(OSStatus status, CFDictionaryRef response, void *opaque)
{
    struct gen2_command_context *ctx = (struct gen2_command_context *)opaque;
    int crc = ALT111_STALE;
    (void)response;
    if(!ctx)return;

    pthread_mutex_lock(&g2_core_lock);
    g2_last_completed_request = ctx->request;
    g2_last_completion_status = (int)status;
    crc = alt111_control_complete(&g2_control,ctx->session,ctx->request,
                                  status==K_NO_ERR,monotonic_ms());
    pthread_cond_broadcast(&g2_core_cv);
    pthread_mutex_unlock(&g2_core_lock);

    logf_u2("gen2 UI completion type=%u request=%llu status=%d core_rc=%d",
            ctx->type,(unsigned long long)ctx->request,(int)status,crc);
    free(ctx);
    gen2_publish_status();
}

struct gen2_diag_context {
    const char *label;
};

static void gen2_diag_completion(OSStatus status, CFDictionaryRef response, void *opaque)
{
    struct gen2_diag_context *ctx=(struct gen2_diag_context *)opaque;
    (void)response;
    logf_u2("gen2 DIAG completion command=%s status=%d",
            (ctx&&ctx->label)?ctx->label:"<unknown>",(int)status);
    if(ctx)free(ctx);
}

static void gen2_dispatch_diag_command(const char *label, CFMutableDictionaryRef req)
{
    AirPlayReceiverSessionRef s=NULL;
    struct gen2_diag_context *ctx=NULL;
    OSStatus e=-1;

    if(!req||!label){
        if(req)p_CFRelease(req);
        return;
    }

    s=retain_active_session();
    ctx=(struct gen2_diag_context *)calloc(1,sizeof(*ctx));
    if(ctx)ctx->label=label;

    if(s&&ctx){
        e=g_sendcmd(s,req,gen2_diag_completion,ctx);
        if(e==K_NO_ERR){
            logf_u2("gen2 DIAG dispatched command=%s",label);
            ctx=NULL;
        }else{
            logf_u2("gen2 DIAG dispatch failed command=%s os=%d",label,(int)e);
        }
    }else{
        logf_u2("gen2 DIAG dispatch unavailable command=%s session=%s ctx=%s",
                label,s?"yes":"no",ctx?"yes":"no");
    }

    if(ctx)free(ctx);
    if(s)p_CFRelease(s);
    p_CFRelease(req);
}

static void gen2_resync_snapshot_locked(struct alt111_resync_snapshot *snap)
{
    memset(snap,0,sizeof(*snap));
    snap->stream = g2_video.stream;
    snap->codec = g2_video.codec;
    snap->consumer = g2_video.consumer;
    snap->source_aus = g2_video.source_aus;
    snap->source_idrs = g2_video.source_idrs;
    snap->projection_desired = g2_control.desired;
    snap->config_valid = g2_video.config_valid;
    snap->consumer_primed = g2_video.consumer_primed;
}

static void gen2_resync_poll(void)
{
    struct alt111_resync_snapshot snap;
    enum alt111_resync_action action;
    uint64_t now = monotonic_ms();
    uint64_t epoch = 0, idr_at_arm = 0, source_idrs = 0;
    uint64_t requests = 0, retries = 0, completions = 0, cancels = 0;
    unsigned manual_enabled = access(g2_resync_enable_marker,F_OK)==0 ? 1u : 0u;
    unsigned d2_enabled = gen2_d2_enabled();
    unsigned enabled = (manual_enabled || d2_enabled) ? 1u : 0u;
    unsigned arm = access(g2_resync_arm_marker,F_OK)==0 ? 1u : 0u;
    unsigned before_enabled, before_state, before_cancel;
    unsigned auto_reason = ALT111_RESYNC_REASON_NONE;
    unsigned auto_sources = 0;
    int arm_rc = ALT111_WAIT;
    int auto_arm_rc = ALT111_WAIT;
    int keyframe_rc = ALT111_WAIT;
    int changed = 0;

    if(arm) (void)unlink(g2_resync_arm_marker);

    pthread_mutex_lock(&g2_core_lock);
    before_enabled = g2_resync.enabled;
    before_state = g2_resync.state;
    before_cancel = g2_resync.cancel_reason;
    alt111_resync_set_enabled(&g2_resync,enabled);
    gen2_resync_snapshot_locked(&snap);

    if(d2_enabled) {
        if(!g2_d2_was_enabled) {
            g2_d2_was_enabled = 1;
            g2_d2_last_seen_idrs = snap.source_idrs;
            g2_d2_last_idr_ms = now;
            g2_d2_pending_sources = 0;
            g2_d2_pending_due_ms = 0;
            changed = 1;
        } else if(snap.source_idrs != g2_d2_last_seen_idrs) {
            g2_d2_last_seen_idrs = snap.source_idrs;
            g2_d2_last_idr_ms = now;
            changed = 1;
        }
    } else if(g2_d2_was_enabled) {
        g2_d2_was_enabled = 0;
        g2_d2_pending_sources = 0;
        g2_d2_pending_due_ms = 0;
        changed = 1;
    }

    if(arm)
        arm_rc = alt111_resync_arm(&g2_resync,now,ALT111_RESYNC_REASON_MANUAL,&snap);

    if(d2_enabled && !arm && g2_resync.state == ALT111_RESYNC_SYNCED) {
        if(g2_d2_pending_sources && g2_d2_pending_due_ms &&
           now >= g2_d2_pending_due_ms) {
            auto_sources = g2_d2_pending_sources;
            auto_reason = (auto_sources & G2_D2_SOURCE_SUGGEST_UI) ?
                ALT111_RESYNC_REASON_SUGGEST_UI : ALT111_RESYNC_REASON_TURNS;
            auto_arm_rc = alt111_resync_arm(&g2_resync,now,auto_reason,&snap);
            if(auto_arm_rc == ALT111_OK) {
                g2_d2_pending_sources = 0;
                g2_d2_pending_due_ms = 0;
            }
        } else if(!g2_d2_pending_sources && g2_d2_last_idr_ms &&
                  now >= g2_d2_last_idr_ms + G2_D2_WATCHDOG_MS &&
                  (!g2_d2_last_request_ms ||
                   now >= g2_d2_last_request_ms + G2_D2_MIN_GAP_MS)) {
            auto_reason = ALT111_RESYNC_REASON_WATCHDOG;
            auto_arm_rc = alt111_resync_arm(&g2_resync,now,auto_reason,&snap);
            if(auto_arm_rc == ALT111_OK)
                ++g2_d2_watchdog_triggers;
        }
    }

    action = alt111_resync_tick(&g2_resync,now,&snap);
    /*
     * Candidate D recovery must share the normal GEN2 command transaction.
     * Do not send forceKeyFrame through the out-of-band diagnostic dispatcher:
     * that path has independent completion state and can overlap show/stop/view.
     *
     * alt111_control_keyframe() records demand only.  The same control worker
     * calls alt111_control_next() immediately after this poll and therefore
     * serializes the real AirPlay command behind any in-flight UI transaction.
     */
    if(action==ALT111_RESYNC_REQUEST_KEYFRAME && g2_control_session){
        /*
         * Coalesce retries while an earlier keyframe demand is still pending
         * or awaiting controller completion.  Otherwise a 1 s resync tick can
         * increment keyframe_wanted again before the first serialized request
         * has completed and leave a redundant forceKeyFrame queued even after
         * the fresh source IDR has already completed the resync epoch.
         */
        if(g2_control.keyframe_done != g2_control.keyframe_wanted)
            keyframe_rc=ALT111_BUSY;
        else
            keyframe_rc=alt111_control_keyframe(&g2_control,g2_control_session);
        if(keyframe_rc==ALT111_OK && g2_resync.reason!=ALT111_RESYNC_REASON_MANUAL)
            g2_d2_last_request_ms=now;
        pthread_cond_broadcast(&g2_core_cv);
    }
    epoch = g2_resync.epoch;
    idr_at_arm = g2_resync.idr_at_arm;
    source_idrs = snap.source_idrs;
    requests = g2_resync.requests;
    retries = g2_resync.retries;
    completions = g2_resync.completions;
    cancels = g2_resync.cancels;
    if(before_enabled!=g2_resync.enabled || before_state!=g2_resync.state ||
       before_cancel!=g2_resync.cancel_reason || arm ||
       auto_arm_rc==ALT111_OK || action!=ALT111_RESYNC_NONE)
        changed=1;
    pthread_mutex_unlock(&g2_core_lock);

    if(arm){
        if(arm_rc==ALT111_OK)
            logf_u2("GEN2 RESYNC ARM epoch=%llu reason=manual idr_at_arm=%llu source_idrs=%llu",
                    (unsigned long long)epoch,(unsigned long long)idr_at_arm,
                    (unsigned long long)source_idrs);
        else
            logf_u2("GEN2 RESYNC ARM rejected rc=%d enabled=%u projection=%u config=%u primed=%u stream=%llu codec=%llu consumer=%llu",
                    arm_rc,enabled,snap.projection_desired,snap.config_valid,snap.consumer_primed,
                    (unsigned long long)snap.stream,(unsigned long long)snap.codec,
                    (unsigned long long)snap.consumer);
    }

    if(auto_arm_rc==ALT111_OK){
        logf_u2("GEN2 D2 ARM epoch=%llu reason=%u sources=0x%x idr_at_arm=%llu source_idrs=%llu delay_ms=%u watchdog_ms=%u",
                (unsigned long long)epoch,auto_reason,auto_sources,
                (unsigned long long)idr_at_arm,(unsigned long long)source_idrs,
                G2_D2_EVENT_DELAY_MS,G2_D2_WATCHDOG_MS);
    }else if(auto_reason!=ALT111_RESYNC_REASON_NONE && auto_arm_rc!=ALT111_WAIT){
        logf_u2("GEN2 D2 ARM rejected rc=%d reason=%u sources=0x%x projection=%u config=%u primed=%u",
                auto_arm_rc,auto_reason,auto_sources,
                snap.projection_desired,snap.config_valid,snap.consumer_primed);
    }

    if(action==ALT111_RESYNC_REQUEST_KEYFRAME){
        logf_u2("GEN2 RESYNC REQUEST epoch=%llu request_count=%llu retry_count=%llu idr_at_arm=%llu source_idrs=%llu queue_rc=%d",
                (unsigned long long)epoch,(unsigned long long)requests,
                (unsigned long long)retries,(unsigned long long)idr_at_arm,
                (unsigned long long)source_idrs,keyframe_rc);
        if(keyframe_rc!=ALT111_OK)
            logf_u2("GEN2 RESYNC REQUEST not queued through serialized controller rc=%d session=%llu projection=%u",
                    keyframe_rc,(unsigned long long)g2_control_session,snap.projection_desired);
    }else if(action==ALT111_RESYNC_COMPLETED){
        logf_u2("GEN2 RESYNC COMPLETE epoch=%llu completions=%llu idr_at_arm=%llu source_idrs=%llu",
                (unsigned long long)epoch,(unsigned long long)completions,
                (unsigned long long)idr_at_arm,(unsigned long long)source_idrs);
    }else if(action==ALT111_RESYNC_CANCELLED || (changed && cancels)){
        logf_u2("GEN2 RESYNC CANCEL epoch=%llu cancels=%llu",
                (unsigned long long)epoch,(unsigned long long)cancels);
    }

    if(changed) gen2_publish_status();
}

/* r51-zoom: changeMapZoomLevel / zoomDirection are command keys found in the working MHI2Q cluster library
 * (next to updateViewArea). Test: echo "<dir> [nouuid]" > /tmp/mibr-alt111-zoom  (dir is an integer, e.g. 1 / -1 / 0 / 2). */
static CFMutableDictionaryRef command_zoom(int dir, int with_uuid)
{
    CFMutableDictionaryRef req=dict_new(), params=dict_new();
    if(!req||!params){if(req)p_CFRelease(req);if(params)p_CFRelease(params);return NULL;}
    set_str(req,"type","changeMapZoomLevel");
    if(with_uuid) set_str(params,"uuid",g_alt_uuid);
    set_i64(params,"zoomDirection",(int64_t)dir);
    { CFStringRef k=s_cf("params"); p_CFDictionarySetValue(req,k,params); p_CFRelease(k); }
    p_CFRelease(params); return req;
}
/* r52-wheel: stopgap wheel feed. Reads the DSITracer telnet stream on 127.0.0.1:1798 and turns steering-wheel scroll
 * notches (DSIKeyPanelListener.updateEncoder2 keyboard 4 / encoder 40, +1 up / -1 down) into changeMapZoomLevel steps
 * (zoomDirection 0 = in, 1 = out). Each notch is logged twice by the tracer; only the lines prefixed "1#" count.
 * /tmp/mibr-alt111-zoom-swap flips the direction. Queue is drained by r51_zoom_poll on the control worker. */
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
static volatile int g_r52_pending;
static int g_r52_started;
static void r52_line(const char *ln)
{
    const char *p; int step;
    if(ln[0]!='1'||ln[1]!='#') return;
    /* r57-komo: log what DSIKOMOView (cluster view-mode service) says (first 60 lines per boot) */
    {   static int komo_n;
        if((strstr(ln,"komoview")||strstr(ln,"KOMOView")) && komo_n<60){ ++komo_n;
            logf_u2("r57 komo: %.220s",ln); } }
    /* r55-zone: cluster map layout (changes with the MMI View button) = DSIMapViewerZoomEngine.setZoomArea Rect:x:y:w:h */
    {   const char *zp=strstr(ln,"DSIMapViewerZoomEngine#setZoomArea#");
        if(zp){ const char *rp=strstr(zp,"Rect:"); if(rp){ char rb[64]; size_t k=0; rp+=5;
                while(rp[k] && rp[k]!='}' && k<sizeof(rb)-1){ rb[k]=rp[k]; ++k; } rb[k]=0;
                logf_u2("r55 zone: setZoomArea Rect=%s",rb);
                {   /* r56: Rect "x:y:w:h" -> layout slot by visible width */
                    int rx_,ry_,rw_,rh_; 
                    if(sscanf(rb,"%d:%d:%d:%d",&rx_,&ry_,&rw_,&rh_)==4){
                        unsigned slot=(rw_<=560)?1u:((rw_<=800)?0u:2u);
                        {   /* r60: remember the last announced layout so a dio_manager restart (phone replug) starts on it */
                            int lf=open("/tmp/mibr-alt111-lastlayout",O_WRONLY|O_CREAT|O_TRUNC,0644);
                            if(lf>=0){ char lc=(char)('0'+slot); (void)write(lf,&lc,1); close(lf); } }
                        if(g2_layout_base[slot]>=0 && slot!=g2_layout_cur){
                            unsigned want; int irc=ALT111_OK;
                            pthread_mutex_lock(&g2_core_lock);
                            g2_layout_cur=slot; want=g2_view_for(g2_turns_route); g2_want_view=want;
                            if(g2_control_session) irc=alt111_control_intent(&g2_control,g2_control.desired,want);
                            pthread_cond_broadcast(&g2_core_cv);
                            pthread_mutex_unlock(&g2_core_lock);
                            logf_u2("r56 layout -> slot %u (Rect width %d) viewAreaIndex=%u intent rc=%d",slot,rw_,want,irc);
                        } else logf_u2("r56 layout slot %u (Rect width %d): %s",slot,rw_,g2_layout_base[slot]<0?"not configured, unchanged":"already current");
                    }
                } } return; } }
    /* r53-view: scroll-wheel short click (keyboard 4, key 40; states 1 pressed, 0 released, 3/4 long) is detected here (no action mapped yet).
     * The View button (key 35) is the MMI's own gauge-size control: untouched. */
    p=strstr(ln,"DSIKeyPanelListener#updateKey2#");
    if(p){
        static int wheel_long;
        int wheel_click=0;
        if(strstr(p,"|int:4|int:40|int:1|")) wheel_long=0;
        else if(strstr(p,"|int:4|int:40|int:3|")||strstr(p,"|int:4|int:40|int:4|")) wheel_long=1;
        else if(strstr(p,"|int:4|int:40|int:0|")){ wheel_click=!wheel_long; wheel_long=0; }
        if(wheel_click){
            /* Detected and logged only: no action is mapped to a short wheel click for now. */
            logf_u2("r53 wheel click detected (unmapped, no action)");
        }
        return;
    }
    p=strstr(ln,"DSIKeyPanelListener#updateEncoder2#");
    if(!p) return;
    p=strstr(p,"|int:4|int:40|int:");
    if(!p) return;
    step=atoi(p+strlen("|int:4|int:40|int:"));
    if(step==0) return;
    if(g_r52_pending<20 && g_r52_pending>-20) g_r52_pending+=(step>0)?1:-1;
}
static void *r52_reader(void *arg)
{
    char buf[1024], ln[512]; size_t ll=0;
    (void)arg;
    for(;;){
        struct sockaddr_in a; int fd=socket(AF_INET,SOCK_STREAM,0); ssize_t n;
        if(fd<0){ sleep(2); continue; }
        memset(&a,0,sizeof(a)); a.sin_family=AF_INET; a.sin_port=htons(1798); a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(connect(fd,(struct sockaddr *)&a,sizeof(a))!=0){ close(fd); sleep(2); continue; }
        logf_u2("r52 wheel: connected to tracer stream on 127.0.0.1:1798");
        ll=0;
        while((n=read(fd,buf,sizeof(buf)))>0){
            ssize_t i;
            for(i=0;i<n;++i){
                if(buf[i]=='\n'||buf[i]=='\r'){ if(ll){ ln[ll]=0; r52_line(ln); ll=0; } }
                else if(ll<sizeof(ln)-1) ln[ll++]=buf[i];
            }
        }
        close(fd); logf_u2("r52 wheel: tracer stream closed, retrying"); sleep(2);
    }
    return NULL;
}
/* r58-bp: experiment. The BufferedTelnetAppender ("bluepirat", port 20991, enable with use.appender=telnet,bluepirat)
 * may replay messages from before we connected. Log the first raw lines and every zoom-area / komoview line. */
static int g_r58_started;
static void *r58_reader(void *arg)
{
    char buf[1024], ln[512]; size_t ll=0; int nraw=0, nsel=0; long total=0;
    (void)arg;
    for(;;){
        struct sockaddr_in a; int fd=socket(AF_INET,SOCK_STREAM,0); ssize_t n;
        if(fd<0){ sleep(2); continue; }
        memset(&a,0,sizeof(a)); a.sin_family=AF_INET; a.sin_port=htons(20991); a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(connect(fd,(struct sockaddr *)&a,sizeof(a))!=0){ close(fd); sleep(3); continue; }
        logf_u2("r58 bp: connected to 127.0.0.1:20991");
        ll=0; total=0; nraw=20; nsel=0;
        {   struct timeval rt; rt.tv_sec=1; rt.tv_usec=0; (void)setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&rt,sizeof(rt)); }
        for(;;){
            ssize_t i;
            n=read(fd,buf,sizeof(buf));
            if(n<0){ if(access("/tmp/mibr-alt111-bp-reconnect",F_OK)==0){ unlink("/tmp/mibr-alt111-bp-reconnect"); logf_u2("r58 bp: reconnect requested"); break; } continue; }
            if(n==0) break;
            total+=n;
            for(i=0;i<n;++i){
                if(buf[i]=='\n'||buf[i]=='\r'){
                    if(ll){ ln[ll]=0;
                        if(nraw<25){ ++nraw; logf_u2("r58 bp raw: %.200s",ln); }
                        if((strstr(ln,"setZoomArea")||strstr(ln,"omoview")||strstr(ln,"KOMOView")) && nsel<80){ ++nsel; logf_u2("r58 bp SEL(total=%ld): %.220s",total,ln); }
                        /* r59-bpzone: cluster (instance 1) zone events from the buffered stream -> same handler as the live stream */
                        {   char *q=strchr(ln,'?');
                            if(q && strstr(q,"?1#org.dsi.ifc.map.DSIMapViewerZoomEngine#setZoomArea#")==q) r52_line(q+1); }
                        ll=0; }
                } else if(ll<sizeof(ln)-1) ln[ll++]=buf[i];
            }
        }
        logf_u2("r58 bp: stream closed after %ld bytes",total); close(fd); sleep(3);
    }
    return NULL;
}
static void r51_zoom_poll(void)
{
    if(!g_r58_started){ pthread_t t2; g_r58_started=1; if(pthread_create(&t2,NULL,r58_reader,NULL)==0) pthread_detach(t2); }
    if(!g_r52_started){ pthread_t t; g_r52_started=1; if(pthread_create(&t,NULL,r52_reader,NULL)==0) pthread_detach(t); }
    {   int guard=0;
        while(g_r52_pending!=0 && guard++<3){
            int up=(g_r52_pending>0); int swap=(access("/tmp/mibr-alt111-zoom-swap",F_OK)==0);
            g_r52_pending+= up?-1:1;
            logf_u2("r52 wheel: step %s -> zoomDirection=%d",up?"up":"down",(up!=swap)?0:1);
            gen2_dispatch_diag_command("changeMapZoomLevel",command_zoom((up!=swap)?0:1,1));
        }
    }
    FILE *f=fopen("/tmp/mibr-alt111-zoom","r"); int dir=0, n; char w[16]="";
    if(!f) return;
    n=fscanf(f,"%d %15s",&dir,w); fclose(f); unlink("/tmp/mibr-alt111-zoom");
    if(n<1) return;
    logf_u2("r51 zoom: sending changeMapZoomLevel zoomDirection=%d uuid=%s",dir,(strcmp(w,"nouuid")==0)?"no":"yes");
    gen2_dispatch_diag_command("changeMapZoomLevel",command_zoom(dir,strcmp(w,"nouuid")!=0));
}

static void gen2_process_diag_markers(void)
{
    r51_zoom_poll();
    if(access(g2_diag_keyframe_marker,F_OK)==0){
        unlink(g2_diag_keyframe_marker);
        logf_u2("gen2 DIAG marker keyframe-only");
        gen2_dispatch_diag_command("forceKeyFrame",command_force_keyframe());
    }
    if(access(g2_diag_show_marker,F_OK)==0){
        char active_url[384];
        unlink(g2_diag_show_marker);
        active_alt_url_copy(active_url,sizeof(active_url));
        logf_u2("gen2 DIAG marker show-only url=%s",active_url);
        gen2_dispatch_diag_command("showUI",command_showui());
    }
    if(access(g2_diag_stop_marker,F_OK)==0){
        unlink(g2_diag_stop_marker);
        logf_u2("gen2 DIAG marker stop-only");
        gen2_dispatch_diag_command("stopUI",command_stopui());
    }
}

static void *gen2_control_worker(void *arg)
{
    (void)arg;
    for (;;) {
        struct alt111_command cmd;
        struct gen2_command_context *ctx=NULL;
        int nrc, queued=0;
        CFMutableDictionaryRef req=NULL;
        AirPlayReceiverSessionRef s=NULL;
        OSStatus e=-1;

        pthread_mutex_lock(&g2_core_lock);
        if(!g2_command_ready){
            pthread_mutex_unlock(&g2_core_lock);
            usleep(10000);
            continue;
        }
        pthread_mutex_unlock(&g2_core_lock);

        gen2_resync_poll();
        gen2_process_diag_markers();
        r45_knob_poll();

        if(access(g2_reacquire_marker,F_OK)==0){
            unlink(g2_reacquire_marker);
            pthread_mutex_lock(&g2_core_lock);
            if(g2_control_session){
                int r=alt111_control_reacquire(&g2_control,g2_control_session);
                logf_u2("gen2 explicit reacquire requested rc=%d",r);
            }
            pthread_mutex_unlock(&g2_core_lock);
        }

        pthread_mutex_lock(&g2_core_lock);
        nrc=alt111_control_next(&g2_control,monotonic_ms(),&cmd);
        pthread_mutex_unlock(&g2_core_lock);
        if(nrc!=ALT111_OK){usleep(10000);continue;}

        req=gen2_command_dictionary(&cmd);
        s=retain_active_session();
        ctx=(struct gen2_command_context *)calloc(1,sizeof(*ctx));
        if(ctx){
            ctx->session=cmd.session;
            ctx->request=cmd.request;
            ctx->type=(unsigned)cmd.type;
        }

        if(req&&s&&ctx){
            e=g_sendcmd(s,req,gen2_command_completion,ctx);
            if(e==K_NO_ERR){
                queued=1;
                pthread_mutex_lock(&g2_core_lock);
                g2_last_dispatched_request=cmd.request;
                pthread_cond_broadcast(&g2_core_cv);
                pthread_mutex_unlock(&g2_core_lock);
                ctx=NULL; /* completion callback owns it */
            }
        }
        if(req)p_CFRelease(req);
        if(s)p_CFRelease(s);

        if(!queued){
            if(ctx)free(ctx);
            pthread_mutex_lock(&g2_core_lock);
            (void)alt111_control_complete(&g2_control,cmd.session,cmd.request,0,monotonic_ms());
            pthread_cond_broadcast(&g2_core_cv);
            pthread_mutex_unlock(&g2_core_lock);
            logf_u2("gen2 UI command dispatch failed type=%u request=%llu os=%d",
                    (unsigned)cmd.type,(unsigned long long)cmd.request,(int)e);
        }else{
            logf_u2("gen2 UI command dispatched type=%u request=%llu awaiting completion",
                    (unsigned)cmd.type,(unsigned long long)cmd.request);
        }
        gen2_publish_status();
    }
    return NULL;
}

static int gen2_start_workers(void)
{
    if(g2_workers_started)return 0;
    if(pthread_create(&g2_control_thread,NULL,gen2_control_worker,NULL)!=0)return -1;
    pthread_detach(g2_control_thread);
    if(pthread_create(&g2_output_thread,NULL,gen2_output_worker,NULL)!=0)return -1;
    pthread_detach(g2_output_thread);
    g2_workers_started=1;
    return 0;
}

/* ARM32 absolute jump trampoline: first 8 target bytes must be position-independent prologue. */
static int install_arm_hook_expected(void *target, void *replacement, void **trampoline, const char *name,
                                     uint32_t expected0, uint32_t expected1)
{
#if defined(__arm__)
    uint32_t *src=(uint32_t *)target;
    uint32_t *tr;
    long ps=sysconf(_SC_PAGESIZE);
    uintptr_t page;
    int saved_errno;

    if(ps<=0){
        logf_u2("%s hook sysconf(_SC_PAGESIZE) failed ps=%ld errno=%d %s",
                name,ps,errno,strerror(errno));
        return -1;
    }
    page=((uintptr_t)target)&~((uintptr_t)ps-1u);

    if(src[0]!=expected0||src[1]!=expected1){
        logf_u2("%s prologue mismatch %08x %08x",name,src[0],src[1]);
        return -1;
    }

    /*
     * Keep W^X: never request writable+executable memory at the same time.
     * Older QNX targets may reject RWX even though an RW -> RX transition is
     * permitted.
     */
    errno=0;
    tr=mmap(NULL,16,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANON,-1,0);
    if(tr==MAP_FAILED){
        saved_errno=errno;
        logf_u2("%s trampoline mmap RW failed errno=%d %s",
                name,saved_errno,strerror(saved_errno));
        return -1;
    }

    tr[0]=src[0];
    tr[1]=src[1];
    tr[2]=0xe51ff004u;
    tr[3]=(uint32_t)((uintptr_t)target+8u);
    __builtin___clear_cache((char *)tr,(char *)tr+16);

    errno=0;
    if(mprotect((void *)tr,16,PROT_READ|PROT_EXEC)!=0){
        saved_errno=errno;
        logf_u2("%s trampoline mprotect RX failed errno=%d %s",
                name,saved_errno,strerror(saved_errno));
        munmap(tr,16);
        return -1;
    }

    errno=0;
    if(mprotect((void *)page,(size_t)ps,PROT_READ|PROT_WRITE)!=0){
        uint8_t *shadow;
        void *fixed;
        saved_errno=errno;
        logf_u2("%s target mprotect RW denied page=%p size=%ld errno=%d %s; trying anonymous MAP_FIXED clone",
                name,(void *)page,ps,saved_errno,strerror(saved_errno));

        /*
         * QNX may refuse write permission on a file-backed executable text
         * mapping. Preserve the already-relocated live page byte-for-byte,
         * replace only that page by private anonymous RW memory at the same
         * virtual address, then restore RX after the 8-byte hook patch.
         *
         * The target functions live in libairplay; this installer executes
         * from the preload library, so replacing the target page does not
         * replace the currently executing code.
         */
        shadow=(uint8_t *)malloc((size_t)ps);
        if(!shadow){
            logf_u2("%s target page shadow allocation failed size=%ld",name,ps);
            munmap(tr,16);
            return -1;
        }
        memcpy(shadow,(const void *)page,(size_t)ps);

        errno=0;
        fixed=mmap((void *)page,(size_t)ps,PROT_READ|PROT_WRITE,
                   MAP_PRIVATE|MAP_ANON|MAP_FIXED,-1,0);
        if(fixed==MAP_FAILED || fixed!=(void *)page){
            saved_errno=errno;
            logf_u2("%s anonymous MAP_FIXED clone failed wanted=%p got=%p size=%ld errno=%d %s",
                    name,(void *)page,fixed,ps,saved_errno,strerror(saved_errno));
            free(shadow);
            munmap(tr,16);
            return -1;
        }
        memcpy(fixed,shadow,(size_t)ps);
        free(shadow);
        src=(uint32_t *)target;
        logf_u2("%s target page cloned private RW at %p size=%ld",
                name,(void *)page,ps);
    }

    src[0]=0xe51ff004u;
    src[1]=(uint32_t)(uintptr_t)replacement;
    __builtin___clear_cache((char *)src,(char *)src+8);

    errno=0;
    if(mprotect((void *)page,(size_t)ps,PROT_READ|PROT_EXEC)!=0){
        saved_errno=errno;
        logf_u2("%s target restore RX failed page=%p size=%ld errno=%d %s",
                name,(void *)page,ps,saved_errno,strerror(saved_errno));
        /* The hook is already installed. Keep the trampoline authoritative. */
    }

    *trampoline=tr;
    logf_u2("installed ARM hook %s target=%p replacement=%p trampoline=%p page=%p pagesize=%ld",
            name,target,replacement,tr,(void *)page,ps);
    return 0;
#else
    (void)target;(void)replacement;(void)trampoline;(void)name;
    (void)expected0;(void)expected1;
    return 0; /* host syntax-test only */
#endif
}

static int install_arm_hook(void *target, void *replacement, void **trampoline, const char *name)
{
    return install_arm_hook_expected(target,replacement,trampoline,name,0xe92d4ff0u,0xed2d8b02u);
}

static OSStatus call_stock_setup(AirPlayReceiverSessionRef s, CFDictionaryRef request, CFDictionaryRef *outResponse)
{
    fn_setup_t fn = g_setup_trampoline ? g_setup_trampoline : g_real_setup;
    if(!fn){
        logf_u2("FATAL no stock SessionSetup delegate");
        return -1;
    }
    return fn(s,request,outResponse);
}

static OSStatus call_stock_start(AirPlayReceiverSessionRef s, void *info)
{
    fn_start_t fn = g_start_trampoline ? g_start_trampoline : g_real_start;
    if(!fn){
        logf_u2("FATAL no stock SessionStart delegate");
        return -1;
    }
    return fn(s,info);
}

static void call_stock_teardown(AirPlayReceiverSessionRef s, CFDictionaryRef request, OSStatus reason, Boolean *outDone)
{
    fn_teardown_t fn = g_teardown_trampoline ? g_teardown_trampoline : g_real_teardown;
    if(!fn){
        logf_u2("FATAL no stock SessionTearDown delegate");
        if(outDone)*outDone=1;
        return;
    }
    fn(s,request,reason,outDone);
}


static OSStatus call_stock_session_control(AirPlayReceiverSessionRef session, uint32_t flags,
                                           CFStringRef command, CFTypeRef qualifier,
                                           CFDictionaryRef params, CFDictionaryRef *outParams)
{
    fn_session_control_t fn=g_session_control_trampoline?
        g_session_control_trampoline:g_real_session_control;
    if(!fn){
        logf_u2("FATAL no stock SessionControl delegate");
        return -1;
    }
    return fn(session,flags,command,qualifier,params,outParams);
}

static OSStatus mibr_session_control(AirPlayReceiverSessionRef session, uint32_t flags,
                                     CFStringRef command, CFTypeRef qualifier,
                                     CFDictionaryRef params, CFDictionaryRef *outParams)
{
    OSStatus e;
    char cbuf[96];
    const char *c;
    int private111_active=0;

    flight_record_session_control(session,flags,command,qualifier,params,outParams);
    e=call_stock_session_control(session,flags,command,qualifier,params,outParams);
    c=flight_command_cstr(command,cbuf,sizeof(cbuf));

    pthread_mutex_lock(&g2_core_lock);
    private111_active = g2_control_session ? 1 : 0;
    pthread_mutex_unlock(&g2_core_lock);

    logf_u2("GEN2 FLIGHT SessionControl exit session=%p command=%s commandRef=%p rc=%d out=%p private111=%d",
            session,c,(void *)command,(int)e,
            (outParams&&*outParams)?(void *)*outParams:NULL,private111_active);
    if(outParams&&*outParams)
        if(audi_verbose()) flight_dump_value(c,"sessionControl.outParams",(CFTypeRef)*outParams,0);

    /*
     * Observation-only lifecycle hook.  Exact stock RE proves inbound
     * modesChanged is handled here before PlatformControl fallback.  Do not
     * alter return values or arm Candidate D from this event yet.
     */
    if(c && strcmp(c,"modesChanged")==0){
        mibr_mode_state_t parsed;
        mibr_mode_state_t previous;
        OSStatus prc=-1;
        unsigned previous_valid=0;
        unsigned delta=0;
        uint64_t seq=0, source_idrs=0, source_aus=0;
        unsigned projection=0;

        memset(&parsed,0,sizeof(parsed));
        memset(&previous,0,sizeof(previous));
        if(g_make_mode_state && params)
            prc=g_make_mode_state(session,params,&parsed);

        pthread_mutex_lock(&g2_core_lock);
        previous_valid=g2_last_mode_valid;
        previous=g2_last_mode_state;
        if(prc==K_NO_ERR){
            if(previous_valid){
                if(previous.screen!=parsed.screen) delta|=1u<<0;
                if(previous.main_audio!=parsed.main_audio) delta|=1u<<1;
                if(previous.speech!=parsed.speech) delta|=1u<<2;
                if(previous.speech_detail!=parsed.speech_detail) delta|=1u<<3;
                if(previous.phone!=parsed.phone) delta|=1u<<4;
                if(previous.turns!=parsed.turns) delta|=1u<<5;
            }else{
                delta=0x3fu;
            }
            g2_last_mode_state=parsed;
            g2_last_mode_valid=1;
            ++g2_mode_sequence;
        }
        seq=g2_mode_sequence;
        projection=g2_control.desired;
        source_idrs=g2_video.source_idrs;
        source_aus=g2_video.source_aus;
        pthread_mutex_unlock(&g2_core_lock);

        logf_u2("GEN2 FLIGHT MODES_CHANGED parsed rc=%d seq=%llu delta=0x%02x screen=%d mainAudio=%d speech=%d speechDetail=%d phone=%d turns=%d projection=%u source_aus=%llu source_idrs=%llu private111=%d policy=OBSERVE_ONLY",
                (int)prc,(unsigned long long)seq,delta,
                parsed.screen,parsed.main_audio,parsed.speech,parsed.speech_detail,
                parsed.phone,parsed.turns,projection,
                (unsigned long long)source_aus,(unsigned long long)source_idrs,
                private111_active);
        if(prc==K_NO_ERR && g2_profile.view_count>1u){
            unsigned want; g2_turns_route=(parsed.turns==1); want=g2_view_for(g2_turns_route);
            int irc=ALT111_OK;
            pthread_mutex_lock(&g2_core_lock);
            if(g2_want_view!=want || g2_control.desired_view!=want){
                g2_want_view=want;
                if(g2_control_session)
                    irc=alt111_control_intent(&g2_control,g2_control.desired,want);
                pthread_cond_broadcast(&g2_core_cv);
            }
            pthread_mutex_unlock(&g2_core_lock);
            logf_u2("GEN2 VIEW turns=%d -> viewAreaIndex=%u intent rc=%d",
                    parsed.turns,want,irc);
        }
        if(prc==K_NO_ERR && previous_valid && (delta & (1u<<5)))
            gen2_d2_schedule(G2_D2_SOURCE_TURNS,"turns-edge");
        gen2_publish_status();
    }

    return e;
}

OSStatus AirPlayReceiverSessionControl(AirPlayReceiverSessionRef session, uint32_t flags,
                                       CFStringRef command, CFTypeRef qualifier,
                                       CFDictionaryRef params, CFDictionaryRef *outParams)
{
    return mibr_session_control(session,flags,command,qualifier,params,outParams);
}

static OSStatus call_stock_platform_control(AirPlayReceiverSessionRef session, uint32_t flags,
                                            CFStringRef command, CFTypeRef qualifier,
                                            CFDictionaryRef params, CFDictionaryRef *outParams)
{
    fn_platform_control_t fn=g_platform_control_trampoline?
        g_platform_control_trampoline:g_real_platform_control;
    if(!fn){
        logf_u2("FATAL no stock PlatformControl delegate");
        return -1;
    }
    return fn(session,flags,command,qualifier,params,outParams);
}

static OSStatus mibr_platform_control(AirPlayReceiverSessionRef session, uint32_t flags,
                                      CFStringRef command, CFTypeRef qualifier,
                                      CFDictionaryRef params, CFDictionaryRef *outParams)
{
    OSStatus e;
    char cbuf[96];
    const char *c;
    int private111_active=0;

    flight_record_platform_control(session,flags,command,qualifier,params,outParams);
    e=call_stock_platform_control(session,flags,command,qualifier,params,outParams);
    c=flight_command_cstr(command,cbuf,sizeof(cbuf));

    logf_u2("GEN2 FLIGHT PlatformControl exit session=%p command=%s commandRef=%p rc=%d out=%p",
            session,c,(void *)command,(int)e,
            (outParams&&*outParams)?(void *)*outParams:NULL);
    if(outParams&&*outParams)
        if(audi_verbose()) flight_dump_value(c,"outParams",(CFTypeRef)*outParams,0);

    /*
     * Public vehicle-proven MHI2Q AltScreen parity:
     *
     * The Yuedi/LIVI-derived receiver lets stock observe suggestUI, records
     * stock_rc, but acknowledges the command to the phone with success and
     * deliberately does NOT translate the suggested URL list into showUI.
     *
     * MU1440 stock returns kNotHandledErr (-6714) because stock has no
     * auxiliary cluster presenter.  Once our private type-111 control session
     * is active, propagating that stock-only error back to iOS is therefore
     * the wrong ownership contract.  Keep every other PlatformControl command
     * stock-authoritative.
     */
    if(g_enabled && c && strcmp(c,"suggestUI")==0){
        pthread_mutex_lock(&g2_core_lock);
        private111_active = g2_control_session ? 1 : 0;
        pthread_mutex_unlock(&g2_core_lock);
        if(private111_active){
            gen2_d2_schedule(G2_D2_SOURCE_SUGGEST_UI,"suggestUI");
            logf_u2("GEN2 PARITY SUGGEST_UI_ACCEPTED session=%p stock_rc=%d return_rc=0 policy=LIVI_NOOP urls_not_shown=1",
                    session,(int)e);
            return K_NO_ERR;
        }
    }

    return e;
}

OSStatus AirPlayReceiverSessionPlatformControl(AirPlayReceiverSessionRef session, uint32_t flags,
                                               CFStringRef command, CFTypeRef qualifier,
                                               CFDictionaryRef params, CFDictionaryRef *outParams)
{
    return mibr_platform_control(session,flags,command,qualifier,params,outParams);
}

#include "native_b.inc"

static OSStatus mibr_session_setup(AirPlayReceiverSessionRef s, CFDictionaryRef request, CFDictionaryRef *outResponse)
{
    CFDictionaryRef altDesc=NULL;
    CFDictionaryRef stockResp=NULL;
    int other=0,hasAlt=0;
    OSStatus e;
    int port=-1;
    uint64_t cid=0;

    log_stream_types("SETUP entry",request);

    /*
     * Recovered IRC ordering: pass the original request to stock first.
     * Only after a successful stock response advertise root capabilities and
     * inspect the original request for stream 111.
     */
    e=call_stock_setup(s,request,&stockResp);
    logf_u2("SETUP stock result=%d response=%s",(int)e,stockResp?"yes":"no");
    if(e==K_NO_ERR && g_enabled) set_active_session(s);
    if(e!=K_NO_ERR){
        if(outResponse)*outResponse=stockResp;
        else if(stockResp)p_CFRelease(stockResp);
        return e;
    }

    /*
     * A partially installed inline-hook set must be indistinguishable from
     * stock once the constructor disables GEN2. In particular, do not mutate
     * enabledFeatures when SessionSetup was hooked successfully but a later
     * SessionStart/TearDown hook failed.
     */
    if(!g_enabled){
        if(outResponse)*outResponse=stockResp;
        else if(stockResp)p_CFRelease(stockResp);
        return e;
    }

    if(stockResp)set_reference_enabled_features((CFMutableDictionaryRef)stockResp);

    hasAlt=g_enabled && contains_stream111(request,&altDesc,&other);
    if(!hasAlt){
        if(outResponse)*outResponse=stockResp;
        else if(stockResp)p_CFRelease(stockResp);
        return e;
    }

    cid=stream_connection_id(altDesc);
    logf_u2("GEN2 SETUP contains stream111 cid=%llu otherStreams=%d",
            (unsigned long long)cid,other);
    publish_state("setup");

    if(!cid){
        logf_u2("stream111 SETUP fail-soft: missing streamConnectionID");
        publish_state("setup_failed");
    }else{
        if(nb_marker()) port=nb_start(s,altDesc,cid);
        else port=start_alt_receiver(cid);
        if(port>0 && stockResp){
            append_alt_setup_response((CFMutableDictionaryRef)stockResp,altDesc,port);
            gen2_control_projection_on();
            logf_u2("GEN2 stream111 SETUP accepted cid=%llu dataPort=%d response=cloned-request+streamID111; UI acquisition armed",
                    (unsigned long long)cid,port);
        }else{
            if(port>0) stop_alt_receiver();
            clear_video_observer();
            logf_u2("stream111 SETUP fail-soft: receiver/key setup failed cid=%llu",
                    (unsigned long long)cid);
            publish_state("setup_failed");
        }
    }

    if(outResponse)*outResponse=stockResp;
    else if(stockResp)p_CFRelease(stockResp);
    return e;
}

static OSStatus mibr_session_start(AirPlayReceiverSessionRef s, void *info)
{
    OSStatus e=call_stock_start(s,info);
    if(e==K_NO_ERR && g_enabled) {
        set_active_session(s);
        gen2_set_command_ready(1);
    }
    /* Gen-2 ownership commands are serialized by gen2_control_worker. */
    return e;
}

static void clear_master_key(void)
{
    pthread_mutex_lock(&g_lock);
    memset(g_master_key, 0, sizeof(g_master_key));
    g_master_valid = 0;
    pthread_mutex_unlock(&g_lock);
    logf_u2("cleared captured CarPlay master AES key");
}

static void mibr_session_teardown(AirPlayReceiverSessionRef s, CFDictionaryRef request, OSStatus reason, Boolean *outDone)
{
    int other=0;
    int has_alt;

    if(!g_enabled){
        call_stock_teardown(s,request,reason,outDone);
        return;
    }

    has_alt = request && contains_stream111(request,NULL,&other);
    int has_main = request && contains_stream_type(request,110);
    /*
     * Lifecycle-decoupling probe:
     *
     * Stream 111 is a private secondary display and must not be destroyed just
     * because stock tears down screen 110.  Only an explicit 111 teardown or a
     * true whole-session teardown (NULL request) owns private111 cleanup.
     *
     * Keep the retained AirPlay session reference for the probe so showUI /
     * forceKeyFrame can still be exercised after a 110-only lifecycle event.
     * If the underlying AirPlay session is actually dead, SendCommand will
     * fail and the diagnostic log will tell us; do not hide that by rebuilding
     * 111 or by adding timing workarounds here.
     */
    int full_session = (request == NULL);

    logf_u2("GEN2 TEARDOWN entry request=%s reason=%d alt=%d main110=%d otherStreams=%d private111_decoupled=1",
            request?"dict":"NULL",(int)reason,has_alt,has_main,other);

    if(has_alt){
        logf_u2("GEN2 TEARDOWN explicit stream111 otherStreams=%d main110=%d",other,has_main);
        gen2_control_release();
        if(full_session) gen2_set_command_ready(0);
        stop_alt_receiver();
        clear_video_observer();
        publish_state("idle");
        if(other==0){if(outDone)*outDone=0;return;}
        {
            CFMutableDictionaryRef f=clone_without_111(request);
            call_stock_teardown(s,f?f:request,reason,outDone);
            if(f)p_CFRelease(f);
        }
        if(full_session) {
            set_active_session(NULL);
            clear_master_key();
        }
        return;
    }

    if(full_session) {
        logf_u2("GEN2 TEARDOWN whole-session -> private111 cleanup");
        r35_restore_native_map();
        gen2_control_release();
        gen2_set_command_ready(0);
        stop_alt_receiver();
        clear_video_observer();
        publish_state("idle");
    } else if(has_main) {
        logf_u2("GEN2 TEARDOWN stock110-only -> preserving private111 transport/control state");
    }

    call_stock_teardown(s,request,reason,outDone);

    if(full_session) {
        set_active_session(NULL);
        clear_master_key();
    }
}

OSStatus AirPlayReceiverSessionSetup(AirPlayReceiverSessionRef s, CFDictionaryRef request, CFDictionaryRef *outResponse)
{
    return mibr_session_setup(s,request,outResponse);
}

OSStatus AirPlayReceiverSessionStart(AirPlayReceiverSessionRef s, void *info)
{
    return mibr_session_start(s,info);
}

void AirPlayReceiverSessionTearDown(AirPlayReceiverSessionRef s, CFDictionaryRef request, OSStatus reason, Boolean *outDone)
{
    mibr_session_teardown(s,request,reason,outDone);
}

/* Capture the master session key at the exact stock SetSecurityInfo caller. */
__attribute__((noinline))
OSStatus AES_CBCFrame_Init(void *ctx, const uint8_t key[16], const uint8_t iv[16], Boolean encrypt)
{
    void *ra=__builtin_return_address(0);
    OSStatus e;
    if(!g_real_aes_cbc_init)g_real_aes_cbc_init=(fn_aes_cbc_init_t)sym_next("AES_CBCFrame_Init");
    e=g_real_aes_cbc_init(ctx,key,iv,encrypt);
    if(g_enabled && e==K_NO_ERR && !encrypt && g_security_fn &&
       (uintptr_t)ra>=g_security_fn && (uintptr_t)ra<g_security_fn+0x100u){
        pthread_mutex_lock(&g_lock); memcpy(g_master_key,key,16); g_master_valid=1; pthread_mutex_unlock(&g_lock);
        logf_u2("captured stock CarPlay master AES key at SetSecurityInfo caller");
    }
    return e;
}

/* r24-persist: recreate per-boot /tmp markers from persistent files in /mnt/app/root.
 *   mibr-carplay111-native-b   -> /tmp/mibr-alt111-audi.native-b and .native-b-fwd
 *   mibr-carplay111-size       -> /tmp/mibr-alt111-audi.size  (content "W H")
 * The /tmp files can still be created by hand and take effect as before. */
static void r24_persist_sync(void)
{
    if(access("/mnt/app/root/mibr-carplay111-native-b",F_OK)==0){
        int fd=open("/tmp/mibr-alt111-audi.native-b",O_WRONLY|O_CREAT,0644); if(fd>=0) close(fd);
        fd=open("/tmp/mibr-alt111-audi.native-b-fwd",O_WRONLY|O_CREAT,0644); if(fd>=0) close(fd);
    }
    {   FILE *in=fopen("/mnt/app/root/mibr-carplay111-size","r");
        if(in){
            char b[64]; size_t n=fread(b,1,sizeof(b)-1,in); fclose(in);
            if(n>0){ FILE *o=fopen("/tmp/mibr-alt111-audi.size","w"); if(o){ fwrite(b,1,n,o); fclose(o);} }
        }
    }
    logf_u2("r24-persist sync done");
}

__attribute__((constructor))
static void altscreen111_init(void)
{
    char active_url[384];
    void *setup,*start,*td,*platform,*control;
    signal(SIGPIPE,SIG_IGN);
    r24_persist_sync();
    g_enabled=env_i("ALTSCREEN111_ENABLED",1);
    g_alt_port=env_i("ALTSCREEN111_PORT",6000); if(g_alt_port!=5001 && g_alt_port!=5010 && g_alt_port!=6000 && g_alt_port!=7100) g_alt_port=6000;
    g_tee_port=env_i("ALTSCREEN111_TEE_PORT",19820);
    g_capture_port=env_i("ALTSCREEN111_CAPTURE_PORT",19821);
    g_width=env_i("ALTSCREEN111_WIDTH",1010);
    g_height=env_i("ALTSCREEN111_HEIGHT",376);
    /* Keep MU-target physical geometry; Audi 290x90 is not portable evidence. */
    g_width_mm=env_i("ALTSCREEN111_WIDTH_MM",200);
    g_height_mm=env_i("ALTSCREEN111_HEIGHT_MM",
                      g_width>0 ? (200*g_height)/g_width : 74);
    g_fps=env_i("ALTSCREEN111_FPS",30);
    /*
     * Reference IRC does not prove startup showUI/forceKeyFrame. Ignore the
     * legacy AUTO_SHOW=1 package default unless parity-specific opt-in is set.
     */
    g_auto_show=0; /* Gen-2 control worker owns UI acquisition. */
    if(access(g_autoshow_disable_marker,F_OK)==0)g_auto_show=0;
    g_viewareas=env_i("ALTSCREEN111_PARITY_VIEWAREAS",1);
    if(access(g_viewareas_marker,F_OK)==0)g_viewareas=1;
    /* Do not inherit Run117/118 legacy UUID/URL env values in the parity build. */
    env_s("ALTSCREEN111_PARITY_UUID",g_alt_uuid,sizeof(g_alt_uuid),ALT_UUID_DEFAULT);
    env_s("ALTSCREEN111_PARITY_URL",g_alt_url,sizeof(g_alt_url),ALT_URL_DEFAULT);
    clear_video_observer();
    publish_state(g_enabled ? "initializing" : "disabled");
    /*
     * AUDI: exported wrappers forward to stock through g_real_*.  Resolve them
     * even when disabled, otherwise a disabled preload leaves stock unreachable.
     */
    g_real_setup=(fn_setup_t)sym_next("AirPlayReceiverSessionSetup");
    g_real_start=(fn_start_t)sym_next("AirPlayReceiverSessionStart");
    g_real_teardown=(fn_teardown_t)sym_next("AirPlayReceiverSessionTearDown");
    g_real_platform_control=(fn_platform_control_t)sym_next("AirPlayReceiverSessionPlatformControl");
    g_real_session_control=(fn_session_control_t)sym_next("AirPlayReceiverSessionControl");
    logf_u2("build %s enabled=%d",ALT111_AUDI_BUILD_TAG,g_enabled);
    if(!g_enabled)return;
    if(init_api()!=0){
        g_enabled=0;
        publish_state("error");
        logf_u2("GEN2 disabled: API initialization failed");
        return;
    }

    alt111_profile_mu1440(&g2_profile,0);
    if(g_width<=0 || g_height<=0 || g_width_mm<=0 || g_height_mm<=0 ||
       g_fps<=0 || g_fps>60){
        logf_u2("gen2 invalid runtime geometry/fps %dx%d physical=%dx%d fps=%d",
                g_width,g_height,g_width_mm,g_height_mm,g_fps);
        publish_state("error");
        return;
    }
    if(strlen(g_alt_uuid)!=36u){
        logf_u2("gen2 invalid secondary display UUID length=%zu",strlen(g_alt_uuid));
        publish_state("error");
        return;
    }
    memcpy(g2_profile.uuid,g_alt_uuid,37u);
    g2_profile.width=(uint32_t)g_width;
    g2_profile.height=(uint32_t)g_height;
    g2_profile.width_mm=(uint32_t)g_width_mm;
    g2_profile.height_mm=(uint32_t)g_height_mm;
    g2_profile.max_fps=(uint32_t)g_fps;
    g2_profile.views[0].area.width=g2_profile.width;
    g2_profile.views[0].area.height=g2_profile.height;
    g2_profile.views[0].safe=g2_profile.views[0].area;
    {
        int ix,iy,iw,ih,rx,ry,rw,rh;
        if(load_safearea_path(g_safearea_idle_path,&ix,&iy,&iw,&ih)==0){
            rx=0;ry=0;rw=g_width;rh=g_height;
            (void)load_safearea_path(g_safearea_config_path,&rx,&ry,&rw,&rh);
            g2_profile.view_count=2;
            g2_profile.views[0].safe.x=(uint32_t)ix;
            g2_profile.views[0].safe.y=(uint32_t)iy;
            g2_profile.views[0].safe.width=(uint32_t)iw;
            g2_profile.views[0].safe.height=(uint32_t)ih;
            g2_profile.views[1].area=g2_profile.views[0].area;
            g2_profile.views[1].safe.x=(uint32_t)rx;
            g2_profile.views[1].safe.y=(uint32_t)ry;
            g2_profile.views[1].safe.width=(uint32_t)rw;
            g2_profile.views[1].safe.height=(uint32_t)rh;
            logf_u2("GEN2 two-view mode: idle=%dx%d@%d,%d route=%dx%d@%d,%d",
                    iw,ih,ix,iy,rw,rh,rx,ry);
            {   /* r56-layouts: extra layouts (big dials = narrow, third = full) from per-layout files */
                static const char *lname[3]={"","narrow","full"};
                int slot;
                for(slot=1;slot<3;++slot){
                    char pi[160],pr[160]; int jx,jy,jw,jh,kx,ky,kw,kh; unsigned vc=g2_profile.view_count;
                    snprintf(pi,sizeof(pi),"/mnt/app/root/mibr-carplay111-safearea-%s-idle.conf",lname[slot]);
                    snprintf(pr,sizeof(pr),"/mnt/app/root/mibr-carplay111-safearea-%s-route.conf",lname[slot]);
                    if(vc+2u>ALT111_MAX_VIEWS) break;
                    if(load_safearea_path(pi,&jx,&jy,&jw,&jh)!=0 || load_safearea_path(pr,&kx,&ky,&kw,&kh)!=0) continue;
                    g2_profile.views[vc].area=g2_profile.views[0].area;
                    g2_profile.views[vc].safe.x=(uint32_t)jx; g2_profile.views[vc].safe.y=(uint32_t)jy;
                    g2_profile.views[vc].safe.width=(uint32_t)jw; g2_profile.views[vc].safe.height=(uint32_t)jh;
                    g2_profile.views[vc+1u].area=g2_profile.views[0].area;
                    g2_profile.views[vc+1u].safe.x=(uint32_t)kx; g2_profile.views[vc+1u].safe.y=(uint32_t)ky;
                    g2_profile.views[vc+1u].safe.width=(uint32_t)kw; g2_profile.views[vc+1u].safe.height=(uint32_t)kh;
                    g2_layout_base[slot]=(int)vc; g2_profile.view_count=vc+2u;
                    logf_u2("r56 layout %s: idle=%dx%d@%d,%d route=%dx%d@%d,%d views=%u",lname[slot],jw,jh,jx,jy,kw,kh,kx,ky,g2_profile.view_count);
                }
                {   /* start layout (the cluster remembers the last View choice; the hook cannot read it at boot) */
                    char sb[8]; int sf=open("/mnt/app/root/mibr-carplay111-startlayout",O_RDONLY);
                    if(sf>=0){ int sn=(int)read(sf,sb,sizeof(sb)-1u); close(sf);
                        if(sn>0){ sb[sn]=0; sn=atoi(sb); if(sn>=0&&sn<3&&g2_layout_base[sn]>=0){ g2_layout_cur=(unsigned)sn; g2_want_view=g2_view_for(0); } } }
                    {   /* r60: prefer the last layout seen live (survives dio_manager restarts, not a reboot) */
                        char tb[8]; int tf=open("/tmp/mibr-alt111-lastlayout",O_RDONLY);
                        if(tf>=0){ int tn=(int)read(tf,tb,sizeof(tb)-1u); close(tf);
                            if(tn>0){ tb[tn]=0; tn=atoi(tb); if(tn>=0&&tn<3&&g2_layout_base[tn]>=0){ g2_layout_cur=(unsigned)tn; g2_want_view=g2_view_for(0); logf_u2("r60 start layout from /tmp/mibr-alt111-lastlayout=%d",tn); } } } }
                    logf_u2("r56 start layout=%u view=%u",g2_layout_cur,g2_want_view);
                }
            }
        }
    }
    if(alt111_profile_validate(&g2_profile)!=ALT111_OK){
        g_enabled=0;
        logf_u2("gen2 runtime profile validation failed; disabled fail-closed");
        publish_state("error");
        return;
    }
    if(alt111_control_init(&g2_control,g2_profile.view_count)!=ALT111_OK){
        g_enabled=0;
        logf_u2("gen2 control init failed; disabled fail-closed");
        publish_state("error");
        return;
    }
    alt111_video_init(&g2_video);
    alt111_resync_init(&g2_resync);
    if(gen2_start_workers()!=0){
        g_enabled=0;
        logf_u2("gen2 worker start failed; disabled fail-closed");
        publish_state("error");
        return;
    }
    if(start_tee_server()!=0){
        g_enabled=0;
        publish_state("error");
        logf_u2("cannot start local H264 tee; AltScreen disabled fail-closed");
        return;
    }
    /*
     * Diagnostic mirror failure is non-fatal. The production tee/remux path
     * must remain usable even if the optional capture port cannot bind.
     */
    if(start_capture_server()!=0)
        logf_u2("GEN2 diagnostic H264 capture mirror unavailable; production path unchanged");
    setup=sym_next("AirPlayReceiverSessionSetup");
    start=sym_next("AirPlayReceiverSessionStart");
    td=sym_next("AirPlayReceiverSessionTearDown");
    platform=sym_next("AirPlayReceiverSessionPlatformControl");
    control=sym_next("AirPlayReceiverSessionControl");
    g_make_mode_state=(fn_make_mode_state_t)sym_next("AirPlayReceiverSessionMakeModeStateFromDictionary");
    g_real_setup=(fn_setup_t)setup;
    g_real_start=(fn_start_t)start;
    g_real_teardown=(fn_teardown_t)td;
    g_real_platform_control=(fn_platform_control_t)platform;
    g_real_session_control=(fn_session_control_t)control;
    if(!setup||!start||!td||!platform||!control){
        logf_u2("missing direct-hook target(s) setup=%p start=%p teardown=%p platform=%p control=%p",
                setup,start,td,platform,control);
        g_enabled=0;
        publish_state("error");
        return;
    }
    logf_u2("hook destinations setup=%p start=%p teardown=%p platform=%p control=%p modeParser=%p",
            (void *)mibr_session_setup,(void *)mibr_session_start,(void *)mibr_session_teardown,
            (void *)mibr_platform_control,(void *)mibr_session_control,(void *)g_make_mode_state);
    if(install_arm_hook(setup,(void *)mibr_session_setup,(void **)&g_setup_trampoline,"SessionSetup")!=0 ||
       install_arm_hook(start,(void *)mibr_session_start,(void **)&g_start_trampoline,"SessionStart")!=0 ||
       install_arm_hook(td,(void *)mibr_session_teardown,(void **)&g_teardown_trampoline,"SessionTearDown")!=0 ||
       install_arm_hook_expected(platform,(void *)mibr_platform_control,
                                 (void **)&g_platform_control_trampoline,"PlatformControl",
                                 0xe92d4ff0u,0xed2d8b04u)!=0 ||
       install_arm_hook_expected(control,(void *)mibr_session_control,
                                 (void **)&g_session_control_trampoline,"SessionControl",
                                 0xe92d4ff0u,0xe1a06002u)!=0){
        g_enabled=0;
        publish_state("error");
        logf_u2("direct hook install failed; disabled fail-closed");
        return;
    }
    publish_state("ready");
    active_alt_url_copy(active_url,sizeof(active_url));
    logf_u2("GEN2 candidate active: 111=%dx%d@%d physical=%dx%d altPort=%d tee=%d capture=%d URL=%s uuid=%s viewAreas=%d autoShow=%d bit26Mode=%d",
            g_width,g_height,g_fps,g_width_mm,g_height_mm,g_alt_port,g_tee_port,g_capture_port,active_url,g_alt_uuid,
            g_viewareas,g_auto_show,airplay_bit26_mode());
    logf_u2("GEN2 Candidate-D manual=%s D2_keyframes=%s eventDelayMs=%u watchdogMs=%u minGapMs=%u sessionMarker=%s persistentMarker=%s",
            access(g2_resync_enable_marker,F_OK)==0 ? "enabled" : "disabled",
            gen2_d2_enabled() ? "enabled" : "disabled",
            G2_D2_EVENT_DELAY_MS,G2_D2_WATCHDOG_MS,G2_D2_MIN_GAP_MS,
            g2_d2_enable_marker,g2_d2_persist_marker);
}
