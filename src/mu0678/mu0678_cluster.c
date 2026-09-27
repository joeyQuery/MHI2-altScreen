#include "mu0678_cluster.h"

extern void *dlsym(void *, const char *);
extern int pthread_create(unsigned long *, const void *, void *(*)(void *), void *);
extern int socket(int,int,int);
extern int bind(int,const void *,unsigned int);
extern int listen(int,int);
extern int accept(int,void *,unsigned int *);
extern int poll(void *,unsigned int,int);
extern int close(int);
extern int setsockopt(int,int,int,const void *,unsigned int);
extern int write(int,const void *,unsigned int);
extern void *memcpy(void *,const void *,unsigned int);
extern void *memset(void *,int,unsigned int);

#define AF_INET 2
#define SOCK_STREAM 1
#define SOL_SOCKET 0xffff
#define SO_REUSEADDR 4
#define AAF1_PORT 15557

typedef void *(*ScreenCreateFn)(void *,void *);
typedef void *(*ScreenSetupFn)(void *,void *,void *);
typedef void (*UpdateFn)(void *,int);
typedef void (*TimeFn)(void *,void *);
typedef void (*SecurityFn)(void *,void *,void *);
typedef int (*StartFn)(void *);
typedef int (*FramesFn)(void *,void *,void *);
typedef void (*StopFn)(void *);
typedef void (*DeleteFn)(void *);
typedef void (*DeriveFn)(void *,int,unsigned int,unsigned int,void *,void *);
typedef void *(*WrapFn)(int,int *);
typedef int (*ServerOpenFn)(unsigned int,unsigned int,unsigned int,int,int *,int,int *);
typedef int (*SocketAcceptFn)(int,unsigned int,int *,void *);

static ScreenCreateFn ScreenCreate;
static ScreenSetupFn ScreenSetup;
static UpdateFn UpdateState;
static TimeFn SetTime;
static SecurityFn SetSecurity;
static StartFn StartSession;
static FramesFn ProcessFrames;
static StopFn StopSession;
static DeleteFn DeleteScreen;
static DeriveFn DeriveAES;
static WrapFn WrapSocket;
static ServerOpenFn ServerOpen;
static SocketAcceptFn SocketAccept;

static void *g_session;
static void *g_screen_b;
static int g_viewer=-1;
static int g_sink=-1;
static unsigned long g_thread;
static volatile int g_running;

static void *resolve(const char *n) { return dlsym((void *)0,n); }

static int resolve_all(void) {
    ScreenCreate=(ScreenCreateFn)resolve("AirPlayReceiverSessionScreen_Create");
    ScreenSetup=(ScreenSetupFn)resolve("AirPlayReceiverSessionScreen_Setup");
    UpdateState=(UpdateFn)resolve("AirPlayReceiverSessionScreen_UpdateState");
    SetTime=(TimeFn)resolve("AirPlayReceiverSessionScreen_SetTimeSynchronizer");
    SetSecurity=(SecurityFn)resolve("AirPlayReceiverSessionScreen_SetSecurityInfo");
    StartSession=(StartFn)resolve("AirPlayReceiverSessionScreen_StartSession");
    ProcessFrames=(FramesFn)resolve("AirPlayReceiverSessionScreen_ProcessFrames");
    StopSession=(StopFn)resolve("AirPlayReceiverSessionScreen_StopSession");
    DeleteScreen=(DeleteFn)resolve("AirPlayReceiverSessionScreen_Delete");
    DeriveAES=(DeriveFn)resolve("AirPlay_DeriveAESKeySHA512ForScreen");
    WrapSocket=(WrapFn)resolve("NetSocket_CreateWithNative");
    ServerOpen=(ServerOpenFn)resolve("ServerSocketOpen");
    SocketAccept=(SocketAcceptFn)resolve("SocketAccept");
    return ScreenCreate&&ScreenSetup&&UpdateState&&SetTime&&SetSecurity&&
           StartSession&&ProcessFrames&&StopSession&&DeleteScreen&&DeriveAES&&WrapSocket&&ServerOpen&&SocketAccept;
}

static void be32(unsigned char *p,unsigned int v) {
    p[0]=(unsigned char)(v>>24); p[1]=(unsigned char)(v>>16);
    p[2]=(unsigned char)(v>>8); p[3]=(unsigned char)v;
}

static void aaf1(unsigned char h[32],unsigned char mode,unsigned char flags,
                 unsigned int v18,unsigned int v04,unsigned int v08,
                 unsigned int v0c,unsigned int v14,unsigned int v10) {
    h[0]='A'; h[1]='A'; h[2]='F'; h[3]='1';
    h[4]=2; h[5]=mode; h[6]=flags; h[7]=32;
    be32(h+8,v18); be32(h+12,v04); be32(h+16,v0c);
    be32(h+20,v08); be32(h+24,v14); be32(h+28,v10);
}

typedef struct { int fd; short events; short revents; } pollfd_t;
static void *sink_worker(void *unused) {
    (void)unused;
    while(g_running && g_sink>=0) {
        unsigned int len=0;
        int fd=accept(g_sink,(void *)0,&len);
        if(fd<0) continue;
        g_viewer=fd;
        while(g_running && g_viewer==fd) {
            pollfd_t p; p.fd=fd; p.events=0x001; p.revents=0;
            if(poll(&p,1,1000)>0 && (p.revents & 0x011)) {
                close(fd); g_viewer=-1; break;
            }
        }
    }
    return (void *)0;
}

int mu0678_alt111_init(void) {
    if(!resolve_all()) return -1;
    if(g_sink>=0) return 0;
    g_sink=socket(AF_INET,SOCK_STREAM,0);
    if(g_sink<0) return -1;
    {
        int one=1;
        unsigned char sa[16];
        memset(sa,0,16);
        sa[0]=0; sa[1]=2;
        sa[2]=(unsigned char)(AAF1_PORT>>8); sa[3]=(unsigned char)AAF1_PORT;
        sa[4]=127; sa[5]=0; sa[6]=0; sa[7]=1;
        setsockopt(g_sink,SOL_SOCKET,SO_REUSEADDR,&one,4);
        if(bind(g_sink,sa,16)<0 || listen(g_sink,1)<0) {
            close(g_sink); g_sink=-1; return -1;
        }
    }
    g_running=1;
    return pthread_create(&g_thread,(const void *)0,sink_worker,(void *)0)==0 ? 0 : -1;
}

int mu0678_alt111_create(MU0678Session session,MU0678Stream stream,
                         unsigned int stream_index,unsigned long long negotiated64,
                         void *setup_result) {
    unsigned char a[16],b[16];
    void *material;
    (void)stream_index;
    if(!resolve_all()||!session||!stream) return -1;
    if(!g_running && mu0678_alt111_init()!=0) return -1;
    g_screen_b=ScreenCreate(session,(void *)0);
    if(!g_screen_b) return -1;
    g_session=session;
    UpdateState(g_screen_b,1);
    if(ScreenSetup(g_screen_b,stream,setup_result)!=0) return -1;
    material=(unsigned char *)session+0x1c8;
    DeriveAES(material,16,(unsigned int)negotiated64,
              (unsigned int)(negotiated64>>32),a,b);
    SetSecurity(g_screen_b,a,b);
    return 0;
}

int mu0678_alt111_set_time(MU0678SessionScreen B, void *time_blob) {
    if(!B||!time_blob||!SetTime) return -1;
    SetTime(B,time_blob);
    return 0;
}

int mu0678_alt111_open_media_listener(MU0678Session session, unsigned short *data_port, int *listener_fd) {
    unsigned int port=0; int fd=-1;
    unsigned int owner=(unsigned int)(*(unsigned char *)((unsigned char *)session+0x51));
    if(!session||!ServerOpen||!data_port||!listener_fd) return -1;
    /* Exact MU0678 stock screen/media listener recipe recovered from 0x23380. */
    if(ServerOpen(owner,1,6,0xffffe872,(int *)&port,0xfffd4000,&fd)!=0) return -1;
    *data_port=(unsigned short)port;
    *listener_fd=fd;
    return 0;
}

int mu0678_alt111_accept_media(MU0678Session session,MU0678SessionScreen B,int listener_fd) {
    int err=0; void *ns,*ctx,*arg3; unsigned int zero=0;
    if(!session||!B||listener_fd<0||!ProcessFrames||!SocketAccept) return -1;
    {
        int native=-1;
        SocketAccept(listener_fd,10,&native,(void *)0);
        if(native<0) return -1;
        ns=WrapSocket(native,&err);
    }
    if(!ns) return -1;
    ctx=*(void **)((unsigned char *)session+0x0c);
    if(!ctx) return -1;
    arg3=*(void **)((unsigned char *)ctx+0xc8);
    if(StartSession(B)!=0) return -1;
    return ProcessFrames(B,ns,arg3);
}

int mu0678_alt111_accept_viewer(void) {
    if(g_viewer>=0) return 0;
    return (g_sink>=0) ? 0 : -1;
}

int mu0678_alt111_publish_h264(const unsigned char *data,unsigned int length,
                               unsigned char mode,unsigned char flags,
                               unsigned int v18,unsigned int v04,unsigned int v08,
                               unsigned int v0c,unsigned int v14,unsigned int v10) {
    unsigned char h[32];
    int n;
    if(!data||!length||g_viewer<0) return -1;
    aaf1(h,mode?mode:1,flags,v18,v04,v08,v0c,v14,v10);
    n=write(g_viewer,h,32);
    if(n!=32) { close(g_viewer); g_viewer=-1; return -1; }
    n=write(g_viewer,data,length);
    if(n!=(int)length) { close(g_viewer); g_viewer=-1; return -1; }
    return 0;
}

void mu0678_alt111_stop(MU0678Session session) {
    (void)session;
    g_running=0;
    if(g_viewer>=0){close(g_viewer);g_viewer=-1;}
    if(g_sink>=0){close(g_sink);g_sink=-1;}
    if(g_screen_b&&StopSession) StopSession(g_screen_b);
    if(g_screen_b&&DeleteScreen) DeleteScreen(g_screen_b);
    g_screen_b=(void *)0; g_session=(void *)0;
}
