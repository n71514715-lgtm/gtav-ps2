#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <math.h>
#include <stdint.h>
#include <loadfile.h>
#include <sifrpc.h>
#include <libpad.h>
#include <stdio.h>

typedef struct { float x, y, z; } Vec3;

/* ── Pad ─────────────────────────────────────────────────── */
static char padBuf[256] __attribute__((aligned(64)));

static int pad_ready = 0;

static void pad_init(void) {
    /* Load pad IRX modules */
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN",  0, NULL);
    padInit(0);
    padPortOpen(0, 0, padBuf);
    pad_ready = 1;
}

static uint32_t pad_read(void) {
    if (!pad_ready) return 0;
    struct padButtonStatus buttons;
    int state = padGetState(0, 0);
    if (state != PAD_STATE_STABLE) return 0;
    if (padRead(0, 0, &buttons) == 0) return 0;
    return ~buttons.btns;
}

#define BTN_UP      0x0010
#define BTN_DOWN    0x0040
#define BTN_LEFT    0x0080
#define BTN_RIGHT   0x0020
#define BTN_CROSS   0x4000
#define BTN_SQUARE  0x8000
#define BTN_CIRCLE  0x2000
#define BTN_TRI     0x1000
#define BTN_L1      0x0400
#define BTN_R1      0x0800
#define BTN_START   0x0008

/* ── Math ────────────────────────────────────────────────── */
static int project(Vec3 p, float *sx, float *sy, Vec3 cam, float angle)
{
    float dx=p.x-cam.x, dy=p.y-cam.y, dz=p.z-cam.z;
    float cosA=cosf(angle), sinA=sinf(angle);
    float rx=dx*cosA+dz*sinA, ry=dy, rz=-dx*sinA+dz*cosA;
    if(rz<0.1f) return 0;
    float fov=200.0f;
    *sx=320.0f+(rx/rz)*fov;
    *sy=224.0f-(ry/rz)*fov;
    return 1;
}

#define CX(x) ((int)(x)<0?0:(int)(x)>639?639:(int)(x))
#define CY(y) ((int)(y)<0?0:(int)(y)>447?447:(int)(y))

static void draw_quad(GSGLOBAL *gs, Vec3 a, Vec3 b, Vec3 c, Vec3 d,
                      Vec3 cam, float angle,
                      uint8_t r, uint8_t g, uint8_t bl, int z)
{
    float ax,ay,bx,by,cx,cy,dx2,dy2;
    if(!project(a,&ax,&ay,cam,angle)) return;
    if(!project(b,&bx,&by,cam,angle)) return;
    if(!project(c,&cx,&cy,cam,angle)) return;
    if(!project(d,&dx2,&dy2,cam,angle)) return;
    float minx=ax<bx?(ax<cx?ax:cx):(bx<cx?bx:cx);
    float maxx=ax>bx?(ax>cx?ax:cx):(bx>cx?bx:cx);
    float miny=ay<by?(ay<cy?ay:cy):(by<cy?by:cy);
    float maxy=ay>by?(ay>cy?ay:cy):(by>cy?by:cy);
    if(maxx<=minx||maxy<=miny) return;
    gsKit_prim_sprite(gs,CX(minx),CY(miny),CX(maxx),CY(maxy),z,
        GS_SETREG_RGBAQ(r,g,bl,128,0));
}

/* ── Buildings ───────────────────────────────────────────── */
typedef struct { float x,z,w,h,d; uint8_t r,g,b; } Building;
#define NUM_BUILDINGS 16
static Building buildings[NUM_BUILDINGS]={
    /* Left side */
    {-8,10,3,8,3,80,75,70},{-8,20,3,12,3,90,85,80},
    {-8,35,4,6,4,70,70,75},{-8,48,3,10,3,85,80,75},
    {-8,62,5,14,4,75,72,68},{-8,80,3,7,3,88,83,78},
    {-8,95,4,10,3,80,76,72},{-8,110,3,13,3,86,82,77},
    /* Right side */
    {8,10,3,6,3,78,74,70},{8,22,4,10,3,92,87,82},
    {8,36,3,8,4,72,68,65},{8,50,5,15,4,86,80,76},
    {8,65,3,9,3,76,73,69},{8,82,4,11,3,89,84,79},
    {8,97,3,7,3,82,78,74},{8,112,5,14,4,91,86,81},
};

/* ── Car mesh (Michael's Tailgater — PS2 box version) ────── */
static void draw_car(GSGLOBAL *gs, Vec3 pos, float carAngle,
                     Vec3 cam, float camAngle)
{
    float ca=cosf(carAngle), sa=sinf(carAngle);
    #define CV(lx,ly,lz) (Vec3){ \
        pos.x + (lx)*ca + (lz)*sa, \
        pos.y + (ly), \
        pos.z - (lx)*sa + (lz)*ca }

    /* Body — dark navy blue like Michael's Tailgater */
    /* Front face */
    draw_quad(gs,
        CV(-1.0f, 0.0f,  2.0f), CV( 1.0f, 0.0f,  2.0f),
        CV( 1.0f, 0.8f,  2.0f), CV(-1.0f, 0.8f,  2.0f),
        cam, camAngle, 40, 50, 120, 3);
    /* Back face */
    draw_quad(gs,
        CV(-1.0f, 0.0f, -2.0f), CV( 1.0f, 0.0f, -2.0f),
        CV( 1.0f, 0.8f, -2.0f), CV(-1.0f, 0.8f, -2.0f),
        cam, camAngle, 30, 40, 100, 3);
    /* Left face */
    draw_quad(gs,
        CV(-1.0f, 0.0f, -2.0f), CV(-1.0f, 0.0f,  2.0f),
        CV(-1.0f, 0.8f,  2.0f), CV(-1.0f, 0.8f, -2.0f),
        cam, camAngle, 25, 35, 90, 3);
    /* Right face */
    draw_quad(gs,
        CV( 1.0f, 0.0f, -2.0f), CV( 1.0f, 0.0f,  2.0f),
        CV( 1.0f, 0.8f,  2.0f), CV( 1.0f, 0.8f, -2.0f),
        cam, camAngle, 25, 35, 90, 3);
    /* Roof */
    draw_quad(gs,
        CV(-0.8f, 0.8f, -1.2f), CV( 0.8f, 0.8f, -1.2f),
        CV( 0.8f, 1.3f,  0.8f), CV(-0.8f, 1.3f,  0.8f),
        cam, camAngle, 50, 60, 130, 3);
    /* Windshield */
    draw_quad(gs,
        CV(-0.8f, 0.8f,  0.8f), CV( 0.8f, 0.8f,  0.8f),
        CV( 0.8f, 1.3f,  0.8f), CV(-0.8f, 1.3f,  0.8f),
        cam, camAngle, 120, 160, 200, 3);
    /* Headlights */
    draw_quad(gs,
        CV(-0.7f, 0.3f, 2.01f), CV(-0.3f, 0.3f, 2.01f),
        CV(-0.3f, 0.6f, 2.01f), CV(-0.7f, 0.6f, 2.01f),
        cam, camAngle, 240, 240, 180, 4);
    draw_quad(gs,
        CV( 0.3f, 0.3f, 2.01f), CV( 0.7f, 0.3f, 2.01f),
        CV( 0.7f, 0.6f, 2.01f), CV( 0.3f, 0.6f, 2.01f),
        cam, camAngle, 240, 240, 180, 4);
    /* Wheels — dark grey */
    draw_quad(gs,
        CV(-1.1f, 0.0f,  1.2f), CV(-1.1f, 0.0f,  0.4f),
        CV(-1.1f, 0.4f,  0.4f), CV(-1.1f, 0.4f,  1.2f),
        cam, camAngle, 30, 30, 30, 4);
    draw_quad(gs,
        CV( 1.1f, 0.0f,  1.2f), CV( 1.1f, 0.0f,  0.4f),
        CV( 1.1f, 0.4f,  0.4f), CV( 1.1f, 0.4f,  1.2f),
        cam, camAngle, 30, 30, 30, 4);
    draw_quad(gs,
        CV(-1.1f, 0.0f, -0.4f), CV(-1.1f, 0.0f, -1.2f),
        CV(-1.1f, 0.4f, -1.2f), CV(-1.1f, 0.4f, -0.4f),
        cam, camAngle, 30, 30, 30, 4);
    draw_quad(gs,
        CV( 1.1f, 0.0f, -0.4f), CV( 1.1f, 0.0f, -1.2f),
        CV( 1.1f, 0.4f, -1.2f), CV( 1.1f, 0.4f, -0.4f),
        cam, camAngle, 30, 30, 30, 4);
    #undef CV
}

/* ── Michael placeholder ─────────────────────────────────── */
static void draw_michael(GSGLOBAL *gs, Vec3 pos,
                          Vec3 cam, float camAngle)
{
    /* Body — dark suit */
    draw_quad(gs,
        (Vec3){pos.x-0.3f,pos.y+0.8f,pos.z},
        (Vec3){pos.x+0.3f,pos.y+0.8f,pos.z},
        (Vec3){pos.x+0.3f,pos.y+1.6f,pos.z},
        (Vec3){pos.x-0.3f,pos.y+1.6f,pos.z},
        cam, camAngle, 40, 40, 60, 4);
    /* Head */
    draw_quad(gs,
        (Vec3){pos.x-0.2f,pos.y+1.6f,pos.z},
        (Vec3){pos.x+0.2f,pos.y+1.6f,pos.z},
        (Vec3){pos.x+0.2f,pos.y+1.9f,pos.z},
        (Vec3){pos.x-0.2f,pos.y+1.9f,pos.z},
        cam, camAngle, 200, 160, 120, 4);
    /* Legs */
    draw_quad(gs,
        (Vec3){pos.x-0.3f,pos.y+0.0f,pos.z},
        (Vec3){pos.x+0.3f,pos.y+0.0f,pos.z},
        (Vec3){pos.x+0.3f,pos.y+0.8f,pos.z},
        (Vec3){pos.x-0.3f,pos.y+0.8f,pos.z},
        cam, camAngle, 30, 30, 50, 4);
}

int main(void)
{
    /* Load pad modules from ROM */
    SifInitRpc(0);
    pad_init();

    GSGLOBAL *gsGlobal=gsKit_init_global();
    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsGlobal->PSM=GS_PSM_CT16;
    gsGlobal->PSMZ=GS_ZBUF_1;
    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal,GS_PERSISTENT);

    /* Player state */
    Vec3 playerPos = {0.0f, 0.0f, 5.0f};
    float playerAngle = 0.0f;
    float playerSpeed = 0.0f;
    int   inCar       = 0;
    int   wanted      = 0;
    int   health      = 100;
    int   frame       = 0;

    /* Camera follows player */
    Vec3  cam;
    float camAngle = 0.0f;

    while(1)
    {
        frame++;

        /* ── Input ───────────────────────────────────────── */
        uint32_t btns = pad_read();

        if(btns & BTN_UP)    playerSpeed += 0.008f;
        if(btns & BTN_DOWN)  playerSpeed -= 0.005f;
        if(btns & BTN_LEFT)  playerAngle -= 0.04f;
        if(btns & BTN_RIGHT) playerAngle += 0.04f;

        /* Clamp + friction */
        if(playerSpeed >  0.18f) playerSpeed =  0.18f;
        if(playerSpeed < -0.08f) playerSpeed = -0.08f;
        playerSpeed *= 0.92f;

        /* Move player */
        playerPos.x += sinf(playerAngle) * playerSpeed;
        playerPos.z += cosf(playerAngle) * playerSpeed;

        /* Enter/exit car with Triangle */
        if((btns & BTN_TRI) && !(frame & 0x1F)) {
            inCar = !inCar;
            if(inCar) playerSpeed = 0.0f;
        }

        /* Wanted level with Circle */
        if((btns & BTN_CIRCLE) && !(frame & 0x1F)) {
            wanted++;
            if(wanted > 5) wanted = 0;
        }

        /* Camera follows player from behind */
        float camDist = inCar ? 6.0f : 4.0f;
        float camHeight = inCar ? 2.5f : 2.0f;
        camAngle += (playerAngle - camAngle) * 0.1f;
        cam.x = playerPos.x - sinf(camAngle) * camDist;
        cam.y = playerPos.y + camHeight;
        cam.z = playerPos.z - cosf(camAngle) * camDist;

        /* ── Render ──────────────────────────────────────── */

        /* Day/night cycle */
        float dayT = (sinf(frame * 0.002f) + 1.0f) * 0.5f;
        uint8_t skyR = (uint8_t)(15  * dayT);
        uint8_t skyG = (uint8_t)(40  * dayT);
        uint8_t skyB = (uint8_t)(60  * dayT + 10);

        gsKit_clear(gsGlobal,GS_SETREG_RGBAQ(skyR,skyG,skyB,0,0));

        /* Horizon */
        uint8_t hR=(uint8_t)(180*dayT);
        uint8_t hG=(uint8_t)(130*dayT);
        gsKit_prim_sprite(gsGlobal,0,195,640,228,1,
            GS_SETREG_RGBAQ(hR,hG,40,128,0));

        /* Ground */
        for(int row=0;row<14;row++){
            for(int col=0;col<10;col++){
                float wx0=(col-5)*4.0f+(float)((int)(cam.x/4))*4.0f;
                float wz0=row*4.0f+(float)((int)(cam.z/4))*4.0f;
                Vec3 p0={wx0,0,wz0},p2={wx0+4,0,wz0+4};
                float sx0,sy0,sx2,sy2;
                if(!project(p0,&sx0,&sy0,cam,camAngle))continue;
                if(!project(p2,&sx2,&sy2,cam,camAngle))continue;
                int ck=(row+col)&1;
                uint8_t gc=(uint8_t)((ck?58:45)*dayT+5);
                gsKit_prim_sprite(gsGlobal,
                    CX(sx0),CY(sy0),CX(sx2),CY(sy2),1,
                    GS_SETREG_RGBAQ(gc,gc,gc,128,0));
            }
        }

        /* Buildings */
        for(int i=0;i<NUM_BUILDINGS;i++){
            Building *bld=&buildings[i];
            float bx=bld->x;
            float bz=bld->z+(float)((int)(cam.z/120))*120.0f;
            float by=bld->h,hw=bld->w*0.5f,hd=bld->d*0.5f;
            Vec3 fl={bx-hw,0,bz-hd},fr={bx+hw,0,bz-hd};
            Vec3 tr={bx+hw,by,bz-hd},tl={bx-hw,by,bz-hd};
            draw_quad(gsGlobal,fl,fr,tr,tl,cam,camAngle,
                bld->r,bld->g,bld->b,2);
            Vec3 sr0={bx+hw,0,bz-hd},sr1={bx+hw,0,bz+hd};
            Vec3 sr2={bx+hw,by,bz+hd},sr3={bx+hw,by,bz-hd};
            draw_quad(gsGlobal,sr0,sr1,sr2,sr3,cam,camAngle,
                (uint8_t)(bld->r*6/10),
                (uint8_t)(bld->g*6/10),
                (uint8_t)(bld->b*6/10),2);
            /* Windows light up at night */
            float wsx0,wsy0;
            Vec3 wc={bx,by*0.6f,bz-hd-0.01f};
            if(project(wc,&wsx0,&wsy0,cam,camAngle)){
                uint8_t wr=dayT<0.4f?220:200;
                uint8_t wg=dayT<0.4f?200:190;
                uint8_t wb=dayT<0.4f?100:140;
                gsKit_prim_sprite(gsGlobal,
                    CX(wsx0-4),CY(wsy0-3),
                    CX(wsx0+4),CY(wsy0+3),3,
                    GS_SETREG_RGBAQ(wr,wg,wb,128,0));
            }
        }

        /* Road lines */
        for(int i=0;i<8;i++){
            float lz=(float)((int)(cam.z/6))*6.0f+i*6.0f;
            Vec3 la={-0.2f,0.01f,lz},lb={0.2f,0.01f,lz+3.0f};
            float lax,lay,lbx,lby;
            if(project(la,&lax,&lay,cam,camAngle)&&
               project(lb,&lbx,&lby,cam,camAngle))
                gsKit_prim_sprite(gsGlobal,
                    CX(lax),CY(lay),CX(lbx),CY(lby),3,
                    GS_SETREG_RGBAQ(220,200,50,128,0));
        }

        /* Draw car */
        Vec3 carPos = {2.5f, 0.0f,
                       playerPos.z + 3.0f};
        if(!inCar) {
            draw_car(gsGlobal, carPos, 0.0f, cam, camAngle);
            draw_michael(gsGlobal, playerPos, cam, camAngle);
        } else {
            draw_car(gsGlobal, playerPos, playerAngle,
                     cam, camAngle);
        }

        /* ── HUD ─────────────────────────────────────────── */
        /* Health */
        gsKit_prim_sprite(gsGlobal,20,20,120,30,5,
            GS_SETREG_RGBAQ(20,20,20,128,0));
        gsKit_prim_sprite(gsGlobal,20,20,20+health,30,5,
            GS_SETREG_RGBAQ(220,30,30,128,0));

        /* Armour */
        gsKit_prim_sprite(gsGlobal,20,34,120,44,5,
            GS_SETREG_RGBAQ(20,20,20,128,0));
        gsKit_prim_sprite(gsGlobal,20,34,90,44,5,
            GS_SETREG_RGBAQ(30,100,220,128,0));

        /* Wanted stars */
        uint8_t pulse=(frame&16)?255:160;
        for(int s=0;s<5;s++){
            uint8_t sr=s<wanted?pulse:25;
            uint8_t sg=s<wanted?(uint8_t)(pulse*180/255):25;
            gsKit_prim_sprite(gsGlobal,
                600-s*18,18,614-s*18,32,5,
                GS_SETREG_RGBAQ(sr,sg,0,128,0));
        }

        /* Minimap */
        gsKit_prim_sprite(gsGlobal,16,358,102,442,5,
            GS_SETREG_RGBAQ(0,35,0,128,0));
        int mdx=(int)(sinf(camAngle)*8);
        int mdy=(int)(-cosf(camAngle)*8);
        gsKit_prim_sprite(gsGlobal,
            56+mdx,398+mdy,62+mdx,404+mdy,5,
            GS_SETREG_RGBAQ(255,220,0,128,0));

        /* Speed bar */
        int spd_w=(int)(playerSpeed*300.0f);
        if(spd_w<0)spd_w=-spd_w;
        gsKit_prim_sprite(gsGlobal,20,48,120,56,5,
            GS_SETREG_RGBAQ(15,15,15,128,0));
        gsKit_prim_sprite(gsGlobal,20,48,20+spd_w,56,5,
            GS_SETREG_RGBAQ(80,200,80,128,0));

        /* In car indicator */
        if(inCar){
            gsKit_prim_sprite(gsGlobal,270,430,370,444,5,
                GS_SETREG_RGBAQ(40,40,40,128,0));
            gsKit_prim_sprite(gsGlobal,272,432,310,442,5,
                GS_SETREG_RGBAQ(255,180,0,128,0));
        }

        gsKit_sync_flip(gsGlobal);
        gsKit_queue_exec(gsGlobal);
    }
    return 0;
}
