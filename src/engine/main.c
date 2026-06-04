#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>

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

    while(1)
    {
        /* Black background */
        gsKit_clear(gsGlobal, GS_SETREG_RGBAQ(0,0,0,0,0));

        /* GTA V sky gradient at bottom — teal to gold */
        gsKit_prim_sprite(gsGlobal,
            0, 380, 640, 448, 1,
            GS_SETREG_RGBAQ(20, 60, 80, 128, 0));

        /* Horizon glow */
        gsKit_prim_sprite(gsGlobal,
            0, 350, 640, 385, 1,
            GS_SETREG_RGBAQ(180, 140, 80, 128, 0));

        /* GRAND THEFT AUTO V — title bar */
        gsKit_prim_sprite(gsGlobal,
            160, 180, 480, 200, 1,
            GS_SETREG_RGBAQ(255, 200, 0, 128, 0));

        /* Subtitle bar — PS2 DEMAKE */
        gsKit_prim_sprite(gsGlobal,
            210, 210, 430, 224, 1,
            GS_SETREG_RGBAQ(200, 200, 200, 128, 0));

        /* Loading bar background */
        gsKit_prim_sprite(gsGlobal,
            100, 300, 540, 316, 1,
            GS_SETREG_RGBAQ(30, 30, 30, 128, 0));

        /* Loading bar fill — animated */
        gsKit_prim_sprite(gsGlobal,
            100, 300, 380, 316, 1,
            GS_SETREG_RGBAQ(255, 180, 0, 128, 0));

        gsKit_sync_flip(gsGlobal);
        gsKit_queue_exec(gsGlobal);
    }

    return 0;
}
