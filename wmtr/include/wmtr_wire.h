/* wmtr request encoding shared by client and host.
 *
 *   struct wmtr_req | u32 relocs[n_reloc] | struct wmtr_out outs[n_out] | (pad to 8) | payload
 *   payload = params (params_size, padded to 8) || extra
 *
 * relocs: payload offsets of 8-byte fields holding a WMTR_REF_* value; the host rewrites them to
 * host pointers before calling the implementation. outs: payload ranges the host copies into the
 * reply after the params, in order. */
#ifndef WMTR_WIRE_H
#define WMTR_WIRE_H
#include "wmtr_proto.h"

struct wmtr_req_ext {
  struct wmtr_req h;
  uint16_t n_reloc, n_out;
  uint32_t payload_size; /* params (padded) + extra */
};
struct wmtr_out { uint32_t off, size; };

#define WMTR_ALIGN8(x) (((x) + 7u) & ~7u)

static inline uint32_t
wmtr_req_payload_offset(const struct wmtr_req_ext *r) {
  return WMTR_ALIGN8((uint32_t)sizeof(*r) + 4u * r->n_reloc + (uint32_t)sizeof(struct wmtr_out) * r->n_out);
}

#endif
