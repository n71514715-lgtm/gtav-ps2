#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <math.h>
#include <stdint.h>

/* ── Simple 3D vector ─────────────────────────────────────── */
typedef struct { float x, y, z; } Vec3;

/* ── Project 3D point to 2D screen ───────────────────────── */
/* Basic perspective projection — no VU1 yet, pure CPU math  */
static int project(Vec3 p, float *sx, float *sy,
                   Vec3 cam, float angle)
{
    /* Translate relative to camera */
    float dx = p.x - cam.x;
    float dy = p.y - cam.y;
    float dz = p.z - cam.z;

    /* Rotate around Y axis */
    float cosA = cosf(angle);
    float sinA = sinf(angle);
    float rx =  dx * cosA + dz * sinA;
    float ry =  dy;
    float rz = -dx * sinA + dz * cosA;

    /* Behind camera — cull */
    if (rz < 0.1f) return 0;

    /* Perspective divide */
    float fov = 200.0f;
    *sx = 320.0f + (rx / rz) * fov;
    *sy = 224.0f - (ry / rz) * fov;

    return 1;
}

int main(void)
{
    GSGLOBAL *gsGlobal = gsKit_init_global();

    dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);

    gsGlobal->PSM  = GS_PSM_CT16;
    gsGlobal->PSMZ = GS_ZBUF_1;
    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal, GS_PERSISTENT);

    /* Camera */
    Vec3 cam = { 0.0f, 2.0f, 0.0f };
    float angle = 0.0f;

    /* Ground grid — Los Santos road, 10x10 squares */
    #define GRID 10
    #define CELL 4.0f

    int frame = 0;

    while(1)
    {
        frame++;

        /* Animate camera — slowly drive forward */
        cam.z += 0.05f;
        angle  = sinf(frame * 0.01f) * 0.3f;

        /* Sky — GTA V teal */
        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(15, 40, 60, 0, 0));

        /* Horizon glow */
        gsKit_prim_sprite(gsGlobal,
            0, 180, 640, 230, 1,
            GS_SETREG_RGBAQ(180, 130, 60, 128, 0));

        /* Draw ground grid */
        for (int row = 0; row < GRID; row++) {
            for (int col = 0; col < GRID; col++) {

                float wx0 = (col - GRID/2) * CELL + 
                            (float)((int)(cam.x / CELL)) * CELL;
                float wz0 = row * CELL + 
                            (float)((int)(cam.z / CELL)) * CELL;
                float wx1 = wx0 + CELL;
                float wz1 = wz0 + CELL;

                Vec3 p0 = { wx0, 0.0f, wz0 };
                Vec3 p1 = { wx1, 0.0f, wz0 };
                Vec3 p2 = { wx1, 0.0f, wz1 };
                Vec3 p3 = { wx0, 0.0f, wz1 };

                float sx0,sy0, sx1,sy1, sx2,sy2, sx3,sy3;
                int v0 = project(p0, &sx0, &sy0, cam, angle);
                int v1 = project(p1, &sx1, &sy1, cam, angle);
                int v2 = project(p2, &sx2, &sy2, cam, angle);
                int v3 = project(p3, &sx3, &sy3, cam, angle);

                if (!v0 || !v1 || !v2 || !v3) continue;

                /* Clamp to screen */
                #define CX(x) ((x)<0?0:(x)>639?639:(x))
                #define CY(y) ((y)<0?0:(y)>447?447:(y))

                /* Checkerboard — road + pavement like SA */
                int checker = (row + col) & 1;
                uint8_t r = checker ? 60 : 45;
                uint8_t g = checker ? 60 : 45;
                uint8_t b = checker ? 60 : 45;

                /* Draw as two triangles (sprite approximation) */
                gsKit_prim_sprite(gsGlobal,
                    CX(sx0), CY(sy0),
                    CX(sx2), CY(sy2),
                    1,
                    GS_SETREG_RGBAQ(r, g, b, 128, 0));
            }
        }

        /* HUD — health bar */
        gsKit_prim_sprite(gsGlobal,
            20, 20, 120, 30, 2,
            GS_SETREG_RGBAQ(20, 20, 20, 128, 0));
        gsKit_prim_sprite(gsGlobal,
            20, 20, 95, 30, 2,
            GS_SETREG_RGBAQ(220, 30, 30, 128, 0));

        /* HUD — armour bar */
        gsKit_prim_sprite(gsGlobal,
            20, 34, 120, 44, 2,
            GS_SETREG_RGBAQ(20, 20, 20, 128, 0));
        gsKit_prim_sprite(gsGlobal,
            20, 34, 80, 44, 2,
            GS_SETREG_RGBAQ(30, 100, 220, 128, 0));

        /* Wanted stars */
        gsKit_prim_sprite(gsGlobal,
            580, 20, 596, 36, 2,
            GS_SETREG_RGBAQ(255, 200, 0, 128, 0));

        /* Minimap circle (approximated) */
        gsKit_prim_sprite(gsGlobal,
            20, 360, 100, 440, 2,
            GS_SETREG_RGBAQ(0, 60, 0, 128, 0));

        /* Player dot on minimap */
        gsKit_prim_sprite(gsGlobal,
            57, 397, 63, 403, 2,
            GS_SETREG_RGBAQ(255, 220, 0, 128, 0));

        gsKit_sync_flip(gsGlobal);
        gsKit_queue_exec(gsGlobal);
    }

    return 0;
}
