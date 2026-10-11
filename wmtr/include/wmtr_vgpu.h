/* wmtr over virtio-gpu: capset id and the EXECBUFFER control commands (guest -> host). */
#ifndef WMTR_VGPU_H
#define WMTR_VGPU_H
#include <stdint.h>

/* virtio-gpu capset ids 1-9 are taken (virgl, virgl2, gfxstream-vk, venus, cross-domain, drm,
 * gfxstream-magma/gles/composer); a private id below 32, because VIRTGPU_PARAM_SUPPORTED_CAPSET_IDs
 * copies only 32 bits of the mask to userspace (virtio_gpu_getparam_ioctl). */
#define WMTR_CAPSET_ID 30

enum { WMTR_VGPU_RING = 1, WMTR_VGPU_KICK = 2, WMTR_VGPU_WAIT = 3 };
struct wmtr_vgpu_cmd {
  uint32_t type;
  uint32_t slot;  /* WAIT: reply slot or WMTR_EVENT_SLOT */
  uint64_t seq;   /* WAIT: signal the fence once slot done_seq (or event_head) >= seq */
  uint64_t reserved;
};

/* capset payload returned by GET_CAPS for WMTR_CAPSET_ID */
struct wmtr_vgpu_caps { uint32_t version, page_size; uint64_t max_blob; };

#endif
