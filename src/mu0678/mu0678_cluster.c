#include "mu0678_cluster.h"
#include "mu0678_interpose.h"

extern void *dlsym(void *, const char *);
extern int pthread_create(unsigned long *, const void *, void *(*)(void *), void *);
extern int pthread_join(unsigned long, void **);
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
extern int spawnl(int,const char *,const char *,const char *,...);

#define AF_INET 2
#define SOCK_STREAM 1
#define SOL_SOCKET 0xffff
#define SO_REUSEADDR 4
#define AAF1_PORT 15557
#define AAF1_HELPER "/eso/bin/apps/mhi2q-aa-cluster-display"

typedef void *(*ScreenCreateFn)(void *,void *);
typedef int (*ScreenSetupFn)(void *,void *,void *);
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
typedef void (*SocketDeleteFn)(void *);

static ScreenCreateFn ScreenCreate;
static ScreenSetupFn ScreenSetup;
static UpdateFn UpdateState;
static TimeFn SetTime;
static SecurityFn SetSecurity;
static StartFn StartSession;
static FramesFn ProcessFrames;
static StopFn StopSession;
static DeleteFn DeleteScreen;
typedef void (*StreamStopFn)(void *);
typedef int (*SendCommandFn)(void *,void *);
typedef int (*ForceKeyFrameFn)(void *);
typedef void *(*CopyServerInfoFn)(void *);
typedef void *(*CFArrayFn)(void *,unsigned int,void *,void *);
typedef unsigned int (*CFArrayCountFn)(void *);
typedef void *(*CFArrayIndexFn)(void *,unsigned int);
typedef int (*CFArrayAppendFn)(void *,void *);
typedef void *(*CFDictCreateFn)(void *,unsigned int,void *,void *);
typedef void *(*CFDictGetFn)(void *,void *);
typedef void (*CFDictSetFn)(void *,void *,void *);
typedef uint64_t (*CFDictGetIntFn)(void *,void *,void *);
typedef void (*CFDictSetIntFn)(void *,void *,unsigned int,unsigned int);
typedef void *(*CFStringCreateFn)(void *,const char *,unsigned int);
typedef void *(*CFDataCreateFn)(void *,void *,unsigned int);
typedef unsigned int (*CFDataLenFn)(void *);
typedef const unsigned char *(*CFDataBytesFn)(void *);
typedef void (*CFReleaseFn)(void *);
typedef void *(*HIDVirtualFn)(void *);
typedef int (*HIDRegisterFn)(void *,void *);
typedef int (*HIDDeregisterFn)(void *);
typedef int (*HIDPostFn)(void *,void *,unsigned int);
static StreamStopFn StreamStop;
static SendCommandFn SendCommand;
static ForceKeyFrameFn ForceKeyFrame;
static CopyServerInfoFn CopyServerInfo;
static CFArrayFn CFArrayCreateMutable;
static CFArrayFn CFArrayCreateMutableCopy;
static CFArrayCountFn CFArrayGetCount;
static CFArrayIndexFn CFArrayGetValueAtIndex;
static CFArrayAppendFn CFArrayAppendValue;
static CFDictCreateFn CFDictionaryCreateMutable;
static CFDictGetFn CFDictionaryGetValue;
static CFDictSetFn CFDictionarySetValue;
static CFDictGetIntFn CFDictionaryGetInt64;
static CFDictSetIntFn CFDictionarySetInt64;
static CFStringCreateFn CFStringCreateWithCString;
static CFDataCreateFn CFPropertyListCreateData;
static CFDataLenFn CFDataGetLength;
static CFDataBytesFn CFDataGetBytePtr;
static CFReleaseFn CFRelease;
static HIDVirtualFn HIDDeviceCreateVirtual;
static HIDRegisterFn HIDRegisterDevice;
static HIDDeregisterFn HIDDeregisterDevice;
static HIDPostFn HIDDevicePostReport;
static DeriveFn DeriveAES;
static WrapFn WrapSocket;
static ServerOpenFn ServerOpen;
static SocketAcceptFn SocketAccept;
static SocketDeleteFn DeleteSocket;

static void *g_session;
static void *g_screen_b;
static int g_viewer=-1;
static int g_sink=-1;
static int g_helper_started;
static unsigned long g_sink_thread;
static volatile int g_running;
static volatile int g_capture_active;
static void *g_secondary_video_impl;
static void *g_secondary_stream;
static unsigned long g_media_thread;
static volatile int g_media_thread_running;
static int g_media_listener=-1;
static unsigned char g_media_ctx[12];

static void *resolve(const char *n) { return mu0678_resolve_airplay(n); }

__attribute__((constructor))
static void mu0678_cluster_constructor(void)
{
    (void)mu0678_interpose_init();
}

static int resolve_all(void)
{
    ScreenCreate=(ScreenCreateFn)resolve("AirPlayReceiverSessionScreen_Create");
    ScreenSetup=(ScreenSetupFn)resolve("AirPlayReceiverSessionScreen_Setup");
    UpdateState=(UpdateFn)resolve("AirPlayReceiverSessionScreen_UpdateState");
    SetTime=(TimeFn)resolve("AirPlayReceiverSessionScreen_SetTimeSynchronizer");
    SetSecurity=(SecurityFn)resolve("AirPlayReceiverSessionScreen_SetSecurityInfo");
    StartSession=(StartFn)resolve("AirPlayReceiverSessionScreen_StartSession");
    ProcessFrames=(FramesFn)resolve("AirPlayReceiverSessionScreen_ProcessFrames");
    StopSession=(StopFn)resolve("AirPlayReceiverSessionScreen_StopSession");
    DeleteScreen=(DeleteFn)resolve("AirPlayReceiverSessionScreen_Delete");
    StreamStop=(StreamStopFn)resolve("ScreenStreamStop");
    SendCommand=(SendCommandFn)resolve("AirPlayReceiverSessionSendCommand");
    ForceKeyFrame=(ForceKeyFrameFn)resolve("AirPlayReceiverSessionForceKeyFrame");
    CopyServerInfo=(CopyServerInfoFn)resolve("AirPlayCopyServerInfo");
    CFArrayCreateMutable=(CFArrayFn)resolve("CFArrayCreateMutable");
    CFArrayCreateMutableCopy=(CFArrayFn)resolve("CFArrayCreateMutableCopy");
    CFArrayGetCount=(CFArrayCountFn)resolve("CFArrayGetCount");
    CFArrayGetValueAtIndex=(CFArrayIndexFn)resolve("CFArrayGetValueAtIndex");
    CFArrayAppendValue=(CFArrayAppendFn)resolve("CFArrayAppendValue");
    CFDictionaryCreateMutable=(CFDictCreateFn)resolve("CFDictionaryCreateMutable");
    CFDictionaryGetValue=(CFDictGetFn)resolve("CFDictionaryGetValue");
    CFDictionarySetValue=(CFDictSetFn)resolve("CFDictionarySetValue");
    CFDictionaryGetInt64=(CFDictGetIntFn)resolve("CFDictionaryGetInt64");
    CFDictionarySetInt64=(CFDictSetIntFn)resolve("CFDictionarySetInt64");
    CFStringCreateWithCString=(CFStringCreateFn)resolve("CFStringCreateWithCString");
    CFPropertyListCreateData=(CFDataCreateFn)resolve("CFPropertyListCreateData");
    CFDataGetLength=(CFDataLenFn)resolve("CFDataGetLength");
    CFDataGetBytePtr=(CFDataBytesFn)resolve("CFDataGetBytePtr");
    CFRelease=(CFReleaseFn)resolve("CFRelease");
    HIDDeviceCreateVirtual=(HIDVirtualFn)resolve("HIDDeviceCreateVirtual");
    HIDRegisterDevice=(HIDRegisterFn)resolve("HIDRegisterDevice");
    HIDDeregisterDevice=(HIDDeregisterFn)resolve("HIDDeregisterDevice");
    HIDDevicePostReport=(HIDPostFn)resolve("HIDDevicePostReport");
    DeriveAES=(DeriveFn)resolve("AirPlay_DeriveAESKeySHA512ForScreen");
    WrapSocket=(WrapFn)resolve("NetSocket_CreateWithNative");
    ServerOpen=(ServerOpenFn)resolve("ServerSocketOpen");
    SocketAccept=(SocketAcceptFn)resolve("SocketAccept");
    DeleteSocket=(SocketDeleteFn)resolve("NetSocket_Delete");
    return ScreenCreate&&ScreenSetup&&UpdateState&&SetTime&&SetSecurity&&
           StartSession&&ProcessFrames&&StopSession&&DeleteScreen&&StreamStop&&
           SendCommand&&ForceKeyFrame&&CopyServerInfo&&
           CFArrayCreateMutable&&CFArrayCreateMutableCopy&&CFArrayGetCount&&
           CFArrayGetValueAtIndex&&CFArrayAppendValue&&
           CFDictionaryCreateMutable&&CFDictionaryGetValue&&CFDictionarySetValue&&
           CFDictionaryGetInt64&&CFDictionarySetInt64&&CFStringCreateWithCString&&
           CFPropertyListCreateData&&CFDataGetLength&&CFDataGetBytePtr&&CFRelease&&
           HIDDeviceCreateVirtual&&HIDRegisterDevice&&HIDDeregisterDevice&&HIDDevicePostReport&&
           DeriveAES&&WrapSocket&&ServerOpen&&SocketAccept&&DeleteSocket;
}

static void be32(unsigned char *p,unsigned int v)
{
    p[0]=(unsigned char)(v>>24); p[1]=(unsigned char)(v>>16);
    p[2]=(unsigned char)(v>>8); p[3]=(unsigned char)v;
}

static void aaf1(unsigned char h[32],unsigned char mode,unsigned char flags,
                 unsigned int v18,unsigned int v04,unsigned int v08,
                 unsigned int v0c,unsigned int v14,unsigned int v10)
{
    h[0]='A'; h[1]='A'; h[2]='F'; h[3]='1';
    h[4]=2; h[5]=mode; h[6]=flags; h[7]=32;
    be32(h+8,v18); be32(h+12,v04); be32(h+16,v0c);
    be32(h+20,v08); be32(h+24,v14); be32(h+28,v10);
}

typedef struct { int fd; short events; short revents; } pollfd_t;

static void *sink_worker(void *unused)
{
    (void)unused;
    while(g_running && g_sink>=0) {
        unsigned int len=0;
        int fd=accept(g_sink,(void *)0,&len);
        if(fd<0) { if(!g_running) break; continue; }
        g_viewer=fd;
        while(g_running && g_viewer==fd) {
            pollfd_t p; p.fd=fd; p.events=1; p.revents=0;
            if(poll(&p,1,1000)>0 && (p.revents&0x011)) {
                close(fd); g_viewer=-1; break;
            }
        }
    }
    return (void *)0;
}

int mu0678_alt111_init(void)
{
    if(!resolve_all()) return -1;
    if(g_sink>=0) return 0;
    g_sink=socket(AF_INET,SOCK_STREAM,0);
    if(g_sink<0) return -1;
    {
        int one=1; unsigned char sa[16];
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
    if(!g_helper_started) {
        if(spawnl(1,AAF1_HELPER,AAF1_HELPER,"--live-carplay",(char *)0)<0) {
            close(g_sink); g_sink=-1; g_running=0; return -1;
        }
        g_helper_started=1;
    }
    if(pthread_create(&g_sink_thread,(const void *)0,sink_worker,(void *)0)!=0) {
        close(g_sink); g_sink=-1; g_running=0; return -1;
    }
    return 0;
}

int mu0678_alt111_create(MU0678Session session,MU0678Stream stream,
                         unsigned int stream_index,unsigned long long negotiated64,
                         void *setup_result)
{
    unsigned char a[16],b[16];
    if(!resolve_all()||!session||!stream) return -1;
    if(!g_running && mu0678_alt111_init()!=0) return -1;
    g_screen_b=ScreenCreate(session,(void *)0);
    if(!g_screen_b) return -1;
    g_session=session;
    UpdateState(g_screen_b,1);
    if(ScreenSetup(g_screen_b,stream,setup_result)!=0) return -1;
    DeriveAES((unsigned char *)session+0x1c8,16,
              (unsigned int)negotiated64,(unsigned int)(negotiated64>>32),a,b);
    SetSecurity(g_screen_b,a,b);
    return 0;
}

int mu0678_alt111_set_time(MU0678SessionScreen screen_b,void *time_blob)
{
    if(!screen_b||!time_blob||!SetTime) return -1;
    SetTime(screen_b,time_blob);
    return 0;
}

int mu0678_alt111_open_media_listener(MU0678Session session,
                                      unsigned short *data_port,int *listener_fd)
{
    int port=0,fd=-1;
    unsigned int owner;
    if(!session||!ServerOpen||!data_port||!listener_fd) return -1;
    owner=(unsigned int)(*(unsigned char *)((unsigned char *)session+0x51));
    if(ServerOpen(owner,1,6,0xffffe872,&port,0xfffd4000,&fd)!=0) return -1;
    *data_port=(unsigned short)port;
    *listener_fd=fd;
    return 0;
}

void mu0678_alt111_capture_video_impl(void *video)
{
    if(g_capture_active) g_secondary_video_impl=video;
}

void mu0678_alt111_capture_stream(void *stream)
{
    if(g_capture_active) g_secondary_stream=stream;
}

MU0678SessionScreen mu0678_alt111_screen(void)
{
    return (MU0678SessionScreen)g_screen_b;
}

int mu0678_alt111_capture_active(void)
{
    return g_capture_active;
}

int mu0678_alt111_publish_decode(void *video_impl,void *data_pp,unsigned int length)
{
    const unsigned char *data;
    if(!video_impl||video_impl!=g_secondary_video_impl||!data_pp||!length) return 0;
    data=*(const unsigned char **)data_pp;
    if(!data) return 0;
    return mu0678_alt111_publish_h264(data,length,1,0,0,0,0,0,0,0);
}

static void *media_worker(void *arg)
{
    MU0678Session session=*(MU0678Session *)(arg+0);
    MU0678SessionScreen screen=*(MU0678SessionScreen *)(arg+4);
    int listener=*(int *)(arg+8);
    while(g_media_thread_running) {
        int native=-1,err=0; void *ns,*ctx,*arg3;
        if(SocketAccept(listener,10,&native,(void *)0)!=0 || native<0) continue;
        ns=WrapSocket(native,&err);
        if(!ns) { close(native); continue; }
        g_capture_active=1;
        if(StartSession(screen)!=0) {
            g_capture_active=0; DeleteSocket(ns); continue;
        }
        g_capture_active=0;
        ctx=*(void **)((unsigned char *)session+0x0c);
        arg3=ctx?*(void **)((unsigned char *)ctx+0xc8):(void *)0;
        if(ctx&&arg3) ProcessFrames(screen,ns,arg3);
        DeleteSocket(ns);
    }
    return (void *)0;
}

int mu0678_alt111_start_worker(MU0678Session session,
                               MU0678SessionScreen screen,int listener_fd)
{
    if(!session||!screen||listener_fd<0) return -1;
    *(MU0678Session *)(g_media_ctx+0)=session;
    *(MU0678SessionScreen *)(g_media_ctx+4)=screen;
    *(int *)(g_media_ctx+8)=listener_fd;
    g_media_listener=listener_fd;
    g_media_thread_running=1;
    if(pthread_create(&g_media_thread,(const void *)0,media_worker,g_media_ctx)!=0) {
        g_media_thread_running=0; g_media_listener=-1; return -1;
    }
    return 0;
}

int mu0678_alt111_accept_viewer(void)
{
    if(g_viewer>=0) return 0;
    return g_sink>=0 ? 0 : -1;
}

int mu0678_alt111_publish_h264(const unsigned char *data,unsigned int length,
                               unsigned char mode,unsigned char flags,
                               unsigned int v18,unsigned int v04,unsigned int v08,
                               unsigned int v0c,unsigned int v14,unsigned int v10)
{
    unsigned char h[32]; int n;
    if(!data||!length||g_viewer<0) return -1;
    aaf1(h,mode?mode:1,flags,v18,v04,v08,v0c,v14,v10);
    n=write(g_viewer,h,32);
    if(n!=32) { close(g_viewer); g_viewer=-1; return -1; }
    n=write(g_viewer,data,length);
    if(n!=(int)length) { close(g_viewer); g_viewer=-1; return -1; }
    return 0;
}

void mu0678_alt111_stop(MU0678Session session)
{
    (void)session;
    g_running=0; g_capture_active=0; g_media_thread_running=0;
    if(g_media_listener>=0){close(g_media_listener);g_media_listener=-1;}
    if(g_media_thread) { pthread_join(g_media_thread,(void **)0); g_media_thread=0; }
    if(g_viewer>=0){close(g_viewer);g_viewer=-1;}
    if(g_sink>=0){close(g_sink);g_sink=-1;}
    if(g_sink_thread) { pthread_join(g_sink_thread,(void **)0); g_sink_thread=0; }
    if(g_secondary_stream&&StreamStop) StreamStop(g_secondary_stream);
    if(g_screen_b&&StopSession) StopSession(g_screen_b);
    if(g_screen_b&&DeleteScreen) DeleteScreen(g_screen_b);
    g_screen_b=(void *)0; g_session=(void *)0; g_secondary_video_impl=(void *)0; g_secondary_stream=(void *)0;
}
