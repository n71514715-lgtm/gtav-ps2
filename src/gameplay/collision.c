#include "collision.h"

AABB g_colliders[MAX_COLLIDERS];
int  g_num_colliders = 0;

void col_init(void) { g_num_colliders = 0; }

void col_add(float x, float z, float w, float d) {
    if(g_num_colliders >= MAX_COLLIDERS) return;
    g_colliders[g_num_colliders].x  = x;
    g_colliders[g_num_colliders].z  = z;
    g_colliders[g_num_colliders].hw = w * 0.5f;
    g_colliders[g_num_colliders].hd = d * 0.5f;
    g_num_colliders++;
}

void col_resolve(float *px, float *pz, float radius) {
    for(int i = 0; i < g_num_colliders; i++) {
        AABB *b = &g_colliders[i];
        float dx = *px - b->x;
        float dz = *pz - b->z;
        float ox = b->hw + radius - (dx < 0 ? -dx : dx);
        float oz = b->hd + radius - (dz < 0 ? -dz : dz);
        if(ox <= 0 || oz <= 0) continue;
        /* Push out along smallest overlap axis */
        if(ox < oz) *px += dx < 0 ? -ox : ox;
        else        *pz += dz < 0 ? -oz : oz;
    }
}
