#ifndef MU0678_CLUSTER_H
#define MU0678_CLUSTER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef void *MU0678Session;
typedef void *MU0678SessionScreen;
typedef void *MU0678Stream;
int mu0678_alt111_create(MU0678Session session, MU0678Stream stream, uint32_t stream_index, uint64_t negotiated64, void *setup_result);
int mu0678_alt111_accept(MU0678Session session, MU0678SessionScreen screen_b, int native_fd);
int mu0678_alt111_publish_h264(const uint8_t *data, uint32_t length, uint8_t stream_mode, uint8_t flags, uint32_t object_18, uint32_t object_04, uint32_t object_08, uint32_t object_0c, uint32_t object_14, uint32_t object_10);
void mu0678_alt111_stop(MU0678Session session);
int mu0678_alt111_init(void);
#ifdef __cplusplus
}
#endif
#endif
