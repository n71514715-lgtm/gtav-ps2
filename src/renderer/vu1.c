#include "vu1.h"
#include <draw.h>
#include <draw3d.h>
#include <packet2.h>
#include <packet2_utils.h>
#include <dma.h>
#include <string.h>

static MATRIX s_world;
static MATRIX s_view;
static MATRIX s_proj;
static MATRIX s_mvp;

void vu1_init(void) {
    dma_channel_initialize(DMA_CHANNEL_VIF1, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_VIF1);
}

void vu1_set_mvp(MATRIX world, MATRIX view, MATRIX proj) {
    matrix_copy(s_world, world);
    matrix_copy(s_view,  view);
    matrix_copy(s_proj,  proj);
    MATRIX tmp;
    matrix_multiply(tmp,   s_view, s_world);
    matrix_multiply(s_mvp, s_proj, tmp);
}

void vu1_draw_batch(VECTOR *verts, u64 *colours, int count) {
    if(count<=0||count>128) return;

    packet2_t *packet = packet2_create(256,
                                       P2_TYPE_NORMAL,
                                       P2_MODE_CHAIN, 1);

    /* Upload MVP matrix — 4 rows, one at a time */
    packet2_utils_vu_open_unpack(packet, 0, 1);
    u128 row0, row1, row2, row3;
    memcpy(&row0, &s_mvp[0],  16);
    memcpy(&row1, &s_mvp[4],  16);
    memcpy(&row2, &s_mvp[8],  16);
    memcpy(&row3, &s_mvp[12], 16);
    packet2_add_u128(packet, row0);
    packet2_add_u128(packet, row1);
    packet2_add_u128(packet, row2);
    packet2_add_u128(packet, row3);

    /* Upload vertices */
    for(int i=0;i<count;i++){
        u128 v;
        memcpy(&v, verts[i], 16);
        packet2_add_u128(packet, v);
    }

    packet2_utils_vu_close_unpack(packet);
    packet2_utils_vu_add_start_program(packet, 0);
    packet2_utils_vu_add_end_tag(packet);

    dma_channel_wait(DMA_CHANNEL_VIF1, 0);
    dma_channel_send_packet2(packet, DMA_CHANNEL_VIF1, 1);
    dma_channel_wait(DMA_CHANNEL_VIF1, 0);

    packet2_free(packet);
}
