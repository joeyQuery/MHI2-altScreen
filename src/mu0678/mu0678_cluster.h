#ifndef MU0678_CLUSTER_H
#define MU0678_CLUSTER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef void *MU0678Session;
typedef void *MU0678SessionScreen;
typedef void *MU0678Stream;
void mu0678_alt111_capture_video_impl(void *video_impl);
int mu0678_alt111_capture_active(void);
int mu0678_alt111_publish_decode(void *video_impl, void *data_pp, uint32_t length);
int mu0678_alt111_start_worker(MU0678Session session, MU0678SessionScreen screen_b, int listener_fd);
MU0678SessionScreen mu0678_alt111_screen(void);
int mu0678_alt111_create(MU0678Session session, MU0678Stream stream, uint32_t stream_index, uint64_t negotiated64, void *setup_result);
int mu0678_alt111_set_time(MU0678SessionScreen screen_b, void *time_blob);
int mu0678_alt111_open_media_listener(MU0678Session session, uint16_t *data_port, int *listener_fd);
int mu0678_alt111_accept_media(MU0678Session session, MU0678SessionScreen screen_b, int listener_fd);
int mu0678_alt111_accept_viewer(void);
int mu0678_alt111_publish_h264(const uint8_t *data, uint32_t length, uint8_t stream_mode, uint8_t flags, uint32_t object_18, uint32_t object_04, uint32_t object_08, uint32_t object_0c, uint32_t object_14, uint32_t object_10);
void mu0678_alt111_stop(MU0678Session session);
int mu0678_alt111_init(void);
#ifdef __cplusplus
}
#endif
#endif
