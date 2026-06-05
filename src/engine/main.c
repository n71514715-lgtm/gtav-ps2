#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <math.h>
#include <stdint.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libpad.h>

typedef struct { float x, y, z; } V3;

/* ── Pad ─────────────────────────────────────────────────── */
static char padBuf[256] __attribute__((aligned(64)));
static int  pad_ok = 0;

static void pad_init(void) {
    SifLoadModule("rom0:SIO2MAN", 0, NULL);
    SifLoadModule("rom0:PADMAN",  0, NULL);
    padInit(0);
    padPortOpen(0, 0, padBuf);
    pad_ok = 1;
}

static uint32_t pad_read(void) {
    if(!pad_ok) return 0;
    struct padButtonStatus b;
    if(padGetState(0,0)!=PAD_STATE_STABLE) return 0;
    if(padRead(0,0,&b)==0) return 0;
    return ~b.btns;
}

#define BTN_UP    0x0010
#define BTN_DOWN  0x0040
#define BTN_LEFT  0x0080
#define BTN_RIGHT 0x0020
#define BTN_TRI   0x1000
#define BTN_CROSS 0x4000
#define BTN_CIR   0x2000

/* ── Projection (our proven working version) ─────────────── */
static V3  s_cam;
static float s_ang;

static int proj(V3 p, float *sx, float *sy) {
    float dx=p.x-s_cam.x, dy=p.y-s_cam.y, dz=p.z-s_cam.z;
    float ca=cosf(s_ang), sa=sinf(s_ang);
    float rx=dx*ca+dz*sa, ry=dy, rz=-dx*sa+dz*ca;
    if(rz<0.1f) return 0;
    float fov=200.f;
    *sx=320.f+(rx/rz)*fov;
    *sy=224.f-(ry/rz)*fov;
    return 1;
}

#define CX(x) ((int)(x)<0?0:(int)(x)>639?639:(int)(x))
#define CY(y) ((int)(y)<0?0:(int)(y)>447?447:(int)(y))

static GSGLOBAL *gs;

static void quad(V3 a,V3 b,V3 c,V3 d,
                 uint8_t r,uint8_t g,uint8_t bl)
{
    float ax,ay,bx,by,cx,cy,dx2,dy2;
    if(!proj(a,&ax,&ay)) return;
    if(!proj(b,&bx,&by)) return;
    if(!proj(c,&cx,&cy)) return;
    if(!proj(d,&dx2,&dy2)) return;
    float x0=ax<bx?(ax<cx?ax:cx):(bx<cx?bx:cx);
    float x1=ax>bx?(ax>cx?ax:cx):(bx>cx?bx:cx);
    float y0=ay<by?(ay<cy?ay:cy):(by<cy?by:cy);
    float y1=ay>by?(ay>cy?ay:cy):(by>cy?by:cy);
    if(x1<=x0||y1<=y0) return;
    gsKit_prim_sprite(gs,CX(x0),CY(y0),CX(x1),CY(y1),2,
        GS_SETREG_RGBAQ(r,g,bl,128,0));
}

/* ── Buildings ───────────────────────────────────────────── */
typedef struct{float x,z,w,h,d;uint8_t r,g,b;}Bld;
#define NB 16
static Bld blds[NB]={
    {-8,10,3,8,3,80,75,70},{-8,20,3,12,3,90,85,80},
    {-8,35,4,6,4,70,70,75},{-8,48,3,10,3,85,80,75},
    {-8,62,5,14,4,75,72,68},{-8,80,3,7,3,88,83,78},
    {-8,95,4,10,3,80,76,72},{-8,110,3,13,3,86,82,77},
    {8,10,3,6,3,78,74,70},{8,22,4,10,3,92,87,82},
    {8,36,3,8,4,72,68,65},{8,50,5,15,4,86,80,76},
    {8,65,3,9,3,76,73,69},{8,82,4,11,3,89,84,79},
    {8,97,3,7,3,82,78,74},{8,112,5,14,4,91,86,81},
};

static void draw_bld(Bld *b,float loop){
    float bx=b->x,bz=b->z+loop,by=b->h;
    float hw=b->w*.5f,hd=b->d*.5f;
    quad((V3){bx-hw,0,bz-hd},(V3){bx+hw,0,bz-hd},
         (V3){bx+hw,by,bz-hd},(V3){bx-hw,by,bz-hd},
         b->r,b->g,b->b);
    quad((V3){bx+hw,0,bz-hd},(V3){bx+hw,0,bz+hd},
         (V3){bx+hw,by,bz+hd},(V3){bx+hw,by,bz-hd},
         b->r*6/10,b->g*6/10,b->b*6/10);
    float wx,wy;
    if(proj((V3){bx,by*.6f,bz-hd-.01f},&wx,&wy))
        gsKit_prim_sprite(gs,CX(wx-4),CY(wy-3),
            CX(wx+4),CY(wy+3),3,
            GS_SETREG_RGBAQ(200,185,120,128,0));
}

/* ── Car ─────────────────────────────────────────────────── */
static void draw_car(V3 p,float a){
    float ca=cosf(a),sa=sinf(a);
    #define T(lx,ly,lz) (V3){p.x+(lx)*ca+(lz)*sa,p.y+(ly),p.z-(lx)*sa+(lz)*ca}
    quad(T(-1,0,2),T(1,0,2),T(1,.8f,2),T(-1,.8f,2),40,50,120);
    quad(T(-1,0,-2),T(1,0,-2),T(1,.8f,-2),T(-1,.8f,-2),30,40,100);
    quad(T(-1,0,-2),T(-1,0,2),T(-1,.8f,2),T(-1,.8f,-2),25,35,90);
    quad(T(1,0,-2),T(1,0,2),T(1,.8f,2),T(1,.8f,-2),25,35,90);
    quad(T(-.8f,.8f,-1.2f),T(.8f,.8f,-1.2f),
         T(.8f,1.3f,.8f),T(-.8f,1.3f,.8f),50,60,130);
    quad(T(-.8f,.8f,.8f),T(.8f,.8f,.8f),
         T(.8f,1.3f,.8f),T(-.8f,1.3f,.8f),120,160,200);
    quad(T(-1.1f,0,1.2f),T(-1.1f,0,.4f),
         T(-1.1f,.4f,.4f),T(-1.1f,.4f,1.2f),30,30,30);
    quad(T(1.1f,0,1.2f),T(1.1f,0,.4f),
         T(1.1f,.4f,.4f),T(1.1f,.4f,1.2f),30,30,30);
    quad(T(-1.1f,0,-.4f),T(-1.1f,0,-1.2f),
         T(-1.1f,.4f,-1.2f),T(-1.1f,.4f,-.4f),30,30,30);
    quad(T(1.1f,0,-.4f),T(1.1f,0,-1.2f),
         T(1.1f,.4f,-1.2f),T(1.1f,.4f,-.4f),30,30,30);
    #undef T
}

/* ── Characters ──────────────────────────────────────────── */
static void draw_chr(V3 p,uint8_t tr,uint8_t tg,uint8_t tb,
                         uint8_t hr,uint8_t hg,uint8_t hb){
    /* legs */
    quad((V3){p.x-.3f,p.y,p.z},(V3){p.x+.3f,p.y,p.z},
         (V3){p.x+.3f,p.y+.8f,p.z},(V3){p.x-.3f,p.y+.8f,p.z},
         30,30,50);
    /* torso */
    quad((V3){p.x-.3f,p.y+.8f,p.z},(V3){p.x+.3f,p.y+.8f,p.z},
         (V3){p.x+.3f,p.y+1.6f,p.z},(V3){p.x-.3f,p.y+1.6f,p.z},
         tr,tg,tb);
    /* head */
    quad((V3){p.x-.2f,p.y+1.6f,p.z},(V3){p.x+.2f,p.y+1.6f,p.z},
         (V3){p.x+.2f,p.y+1.9f,p.z},(V3){p.x-.2f,p.y+1.9f,p.z},
         hr,hg,hb);
}

/* Michael=blue suit, Trevor=orange shirt, Franklin=green hoodie */
#define MICHAEL(p) draw_chr(p, 40,40,60,   200,160,120)
#define TREVOR(p)  draw_chr(p, 160,80,20,  190,150,110)
#define FRANKLIN(p)draw_chr(p, 30,80,30,   160,120,90)

/* ── HUD helper ──────────────────────────────────────────── */
static void hud(int x0,int y0,int x1,int y1,
                uint8_t r,uint8_t g,uint8_t b){
    gsKit_prim_sprite(gs,x0,y0,x1,y1,5,
        GS_SETREG_RGBAQ(r,g,b,128,0));
}

int main(void)
{
    SifInitRpc(0);
    pad_init();

    gs=gsKit_init_global();
    dmaKit_init(D_CTRL_RELE_OFF,D_CTRL_MFD_OFF,D_CTRL_STS_UNSPEC,
                D_CTRL_STD_OFF,D_CTRL_RCYC_8,1<<DMA_CHANNEL_GIF);
    dmaKit_chan_init(DMA_CHANNEL_GIF);
    gs->PSM=GS_PSM_CT16;
    gs->PSMZ=GS_ZBUF_1;
    gsKit_init_screen(gs);
    gsKit_mode_switch(gs,GS_PERSISTENT);

    V3    pos={0,0,5};
    float facing=0,speed=0;
    int   inCar=0,wanted=0,health=100;
    int   chr=0,switchTimer=0,frame=0;

    while(1){
        frame++;
        uint32_t btns=pad_read();

        /* Movement */
        if(btns&BTN_UP)    speed+=0.008f;
        if(btns&BTN_DOWN)  speed-=0.005f;
        if(btns&BTN_LEFT)  facing-=0.04f;
        if(btns&BTN_RIGHT) facing+=0.04f;
        if(speed> .18f) speed= .18f;
        if(speed<-.08f) speed=-.08f;
        speed*=.92f;
        pos.x+=sinf(facing)*speed;
        pos.z+=cosf(facing)*speed;

        /* Character switch */
        if(btns&BTN_TRI){
            if(++switchTimer==20){chr=(chr+1)%3;switchTimer=0;}
        } else switchTimer=0;

        /* Enter/exit car */
        if((btns&BTN_CROSS)&&!(frame&0x1F)) inCar=!inCar;

        /* Wanted */
        if((btns&BTN_CIR)&&!(frame&0x1F)){
            if(++wanted>5)wanted=0;
        }

        /* Camera */
        float cd=inCar?6.f:4.f,ch=inCar?2.8f:2.2f;
        s_cam=(V3){pos.x-sinf(facing)*cd,
                   pos.y+ch,
                   pos.z-cosf(facing)*cd};
        s_ang=facing;

        /* Day/night */
        float day=(sinf(frame*.001f)+1.f)*.5f;
        uint8_t sr=(uint8_t)(15*day+2);
        uint8_t sg=(uint8_t)(40*day+4);
        uint8_t sb=(uint8_t)(60*day+10);

        gsKit_clear(gs,GS_SETREG_RGBAQ(sr,sg,sb,0,0));

        /* Horizon */
        hud(0,195,640,225,(uint8_t)(170*day),(uint8_t)(120*day),40);

        /* Ground */
        for(int r=0;r<14;r++)
            for(int c=0;c<10;c++){
                float wx=(c-5)*4.f+(float)((int)(s_cam.x/4))*4.f;
                float wz=r*4.f+(float)((int)(s_cam.z/4))*4.f;
                uint8_t gc=(uint8_t)(((r+c)&1?55:42)*day+5);
                quad((V3){wx,0,wz},(V3){wx+4,0,wz},
                     (V3){wx+4,0,wz+4},(V3){wx,0,wz+4},
                     gc,gc,gc);
            }

        /* Buildings */
        float loop=(float)((int)(s_cam.z/120))*120.f;
        for(int i=0;i<NB;i++) draw_bld(&blds[i],loop);

        /* Road lines */
        for(int i=0;i<8;i++){
            float lz=(float)((int)(s_cam.z/6))*6.f+i*6.f;
            quad((V3){-.2f,.01f,lz},(V3){.2f,.01f,lz},
                 (V3){.2f,.01f,lz+3},(V3){-.2f,.01f,lz+3},
                 220,200,50);
        }

        /* Characters + car */
        V3 tp={pos.x+sinf(facing+1.5f)*3,0,
               pos.z+cosf(facing+1.5f)*3};
        V3 fp={pos.x+sinf(facing-1.5f)*3,0,
               pos.z+cosf(facing-1.5f)*3};

        if(!inCar){
            if(chr==0){MICHAEL(pos);TREVOR(tp);FRANKLIN(fp);}
            else if(chr==1){TREVOR(pos);MICHAEL(tp);FRANKLIN(fp);}
            else{FRANKLIN(pos);MICHAEL(tp);TREVOR(fp);}
            draw_car((V3){pos.x+sinf(facing+2.f)*5,0,
                          pos.z+cosf(facing+2.f)*5},facing);
        } else {
            draw_car(pos,facing);
        }

        /* HUD */
        hud(20,20,120,30,20,20,20);
        hud(20,20,20+health,30,220,30,30);
        hud(20,34,120,44,20,20,20);
        hud(20,34,90,44,30,100,220);
        int sw=(int)(speed<0?-speed*300:speed*300);
        hud(20,48,120,56,15,15,15);
        hud(20,48,20+sw,56,80,200,80);

        /* Wanted */
        uint8_t pulse=(frame&16)?255:150;
        for(int s=0;s<5;s++){
            uint8_t wr=s<wanted?pulse:25;
            uint8_t wg=s<wanted?(uint8_t)(pulse*180/255):25;
            hud(600-s*18,18,614-s*18,32,wr,wg,0);
        }

        /* Character colour indicator */
        hud(20,60,70,74,20,20,20);
        uint8_t cr=chr==0?0:chr==1?180:30;
        uint8_t cg=chr==0?80:chr==1?80:160;
        uint8_t cb=chr==0?180:chr==1?20:30;
        hud(22,62,68,72,cr,cg,cb);

        /* Switch hint */
        if(switchTimer>5){
            int nx=(chr+1)%3;
            hud(250,200,390,220,20,20,20);
            uint8_t nr=nx==0?0:nx==1?180:30;
            uint8_t ng=nx==0?80:nx==1?80:160;
            uint8_t nb=nx==0?180:nx==1?20:30;
            hud(252,202,388,218,nr,ng,nb);
        }

        /* Minimap */
        hud(16,358,102,442,0,35,0);
        int mdx=(int)(sinf(facing)*8);
        int mdy=(int)(-cosf(facing)*8);
        hud(56+mdx,398+mdy,62+mdx,404+mdy,255,220,0);

        gsKit_sync_flip(gs);
        gsKit_queue_exec(gs);
    }
    return 0;
}
