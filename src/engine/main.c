#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <math.h>
#include <stdint.h>

typedef struct { float x, y, z; } Vec3;

static int project(Vec3 p, float *sx, float *sy, Vec3 cam, float angle)
{
    float dx=p.x-cam.x, dy=p.y-cam.y, dz=p.z-cam.z;
    float cosA=cosf(angle), sinA=sinf(angle);
    float rx= dx*cosA+dz*sinA, ry=dy, rz=-dx*sinA+dz*cosA;
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

typedef struct { float x,z,w,h,d; uint8_t r,g,b; } Building;
#define NUM_BUILDINGS 12
static Building buildings[NUM_BUILDINGS]={
    {-8,10,3,8,3,80,75,70},{-8,20,3,12,3,90,85,80},
    {-8,35,4,6,4,70,70,75},{-8,48,3,10,3,85,80,75},
    {-8,62,5,14,4,75,72,68},{-8,80,3,7,3,88,83,78},
    {8,10,3,6,3,78,74,70},{8,22,4,10,3,92,87,82},
    {8,36,3,8,4,72,68,65},{8,50,5,15,4,86,80,76},
    {8,65,3,9,3,76,73,69},{8,82,4,11,3,89,84,79},
};

int main(void)
{
    GSGLOBAL *gsGlobal=gsKit_init_global();
    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gsGlobal->PSM=GS_PSM_CT16;
    gsGlobal->PSMZ=GS_ZBUF_1;
    gsKit_init_screen(gsGlobal);
    gsKit_mode_switch(gsGlobal,GS_PERSISTENT);

    Vec3 cam={0.0f,2.5f,0.0f};
    float angle=0.0f;
    int frame=0;
    int wanted=1;

    while(1)
    {
        frame++;

        /* Auto drive forward with gentle sway */
        cam.z+=0.06f;
        angle=sinf(frame*0.008f)*0.15f;

        /* Sky */
        gsKit_clear(gsGlobal,GS_SETREG_RGBAQ(15,40,60,0,0));

        /* Horizon */
        gsKit_prim_sprite(gsGlobal,0,195,640,228,1,
            GS_SETREG_RGBAQ(180,130,60,128,0));

        /* Ground */
        for(int row=0;row<14;row++){
            for(int col=0;col<10;col++){
                float wx0=(col-5)*4.0f+(float)((int)(cam.x/4))*4.0f;
                float wz0=row*4.0f+(float)((int)(cam.z/4))*4.0f;
                Vec3 p0={wx0,0,wz0},p2={wx0+4,0,wz0+4};
                float sx0,sy0,sx2,sy2;
                if(!project(p0,&sx0,&sy0,cam,angle))continue;
                if(!project(p2,&sx2,&sy2,cam,angle))continue;
                int ck=(row+col)&1;
                gsKit_prim_sprite(gsGlobal,
                    CX(sx0),CY(sy0),CX(sx2),CY(sy2),1,
                    GS_SETREG_RGBAQ(ck?58:45,ck?58:45,ck?58:45,128,0));
            }
        }

        /* Buildings */
        for(int i=0;i<NUM_BUILDINGS;i++){
            Building *bld=&buildings[i];
            float bx=bld->x;
            float bz=bld->z+(float)((int)(cam.z/90))*90.0f;
            float by=bld->h,hw=bld->w*0.5f,hd=bld->d*0.5f;
            Vec3 fl={bx-hw,0,bz-hd},fr={bx+hw,0,bz-hd};
            Vec3 tr={bx+hw,by,bz-hd},tl={bx-hw,by,bz-hd};
            draw_quad(gsGlobal,fl,fr,tr,tl,cam,angle,
                bld->r,bld->g,bld->b,2);
            Vec3 sr0={bx+hw,0,bz-hd},sr1={bx+hw,0,bz+hd};
            Vec3 sr2={bx+hw,by,bz+hd},sr3={bx+hw,by,bz-hd};
            draw_quad(gsGlobal,sr0,sr1,sr2,sr3,cam,angle,
                bld->r*6/10,bld->g*6/10,bld->b*6/10,2);
            float wsx0,wsy0;
            Vec3 wc={bx,by*0.6f,bz-hd-0.01f};
            if(project(wc,&wsx0,&wsy0,cam,angle))
                gsKit_prim_sprite(gsGlobal,
                    CX(wsx0-4),CY(wsy0-3),
                    CX(wsx0+4),CY(wsy0+3),3,
                    GS_SETREG_RGBAQ(200,190,140,128,0));
        }

        /* Road lines */
        for(int i=0;i<8;i++){
            float lz=(float)((int)(cam.z/6))*6.0f+i*6.0f;
            Vec3 la={-0.2f,0.01f,lz},lb={0.2f,0.01f,lz+3.0f};
            float lax,lay,lbx,lby;
            if(project(la,&lax,&lay,cam,angle)&&
               project(lb,&lbx,&lby,cam,angle))
                gsKit_prim_sprite(gsGlobal,
                    CX(lax),CY(lay),CX(lbx),CY(lby),3,
                    GS_SETREG_RGBAQ(220,200,50,128,0));
        }

        /* HUD - health */
        gsKit_prim_sprite(gsGlobal,20,20,120,30,4,
            GS_SETREG_RGBAQ(20,20,20,128,0));
        gsKit_prim_sprite(gsGlobal,20,20,95,30,4,
            GS_SETREG_RGBAQ(220,30,30,128,0));

        /* HUD - armour */
        gsKit_prim_sprite(gsGlobal,20,34,120,44,4,
            GS_SETREG_RGBAQ(20,20,20,128,0));
        gsKit_prim_sprite(gsGlobal,20,34,80,44,4,
            GS_SETREG_RGBAQ(30,100,220,128,0));

        /* Wanted stars */
        uint8_t pulse=(frame&16)?255:180;
        for(int s=0;s<5;s++){
            uint8_t sr=s<wanted?pulse:30;
            uint8_t sg=s<wanted?pulse*200/255:30;
            gsKit_prim_sprite(gsGlobal,
                600-s*18,18,614-s*18,32,4,
                GS_SETREG_RGBAQ(sr,sg,0,128,0));
        }

        /* Minimap */
        gsKit_prim_sprite(gsGlobal,16,358,102,442,4,
            GS_SETREG_RGBAQ(0,40,0,128,0));
        int mdx=(int)(sinf(angle)*8);
        int mdy=(int)(-cosf(angle)*8);
        gsKit_prim_sprite(gsGlobal,
            56+mdx,398+mdy,62+mdx,404+mdy,4,
            GS_SETREG_RGBAQ(255,220,0,128,0));

        gsKit_sync_flip(gsGlobal);
        gsKit_queue_exec(gsGlobal);
    }
    return 0;
}
