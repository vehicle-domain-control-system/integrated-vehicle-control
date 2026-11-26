#include "network/net_codec.h"

NetStatus net_can_validate_capture(const NetFrame *frame)
{
    if (frame == NULL || frame->link != NET_LINK_CAN) {
        return NET_INVALID_ARGUMENT;
    }
    /* Generic CAN/CAN FD capture storage can represent 0..64 bytes and a
     * standard or extended numeric identifier. Product flags/DLC/profile
     * are deliberately not inferred from this transport-only container. */
    if (frame->length > 64u || frame->length > NET_FRAME_CAPACITY ||
        frame->id > UINT32_C(0x1fffffff)) {
        return NET_REJECTED;
    }
    return NET_OK;
}
