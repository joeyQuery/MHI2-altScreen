#include "mu0678_interpose.h"
#include "mu0678_cluster.h"

extern int mprotect(void *, unsigned int, int);
extern long sysconf(int);
extern void *dlsym(void *, const char *);
extern unsigned int getpagesize(void);
extern void *mmap(void *, unsigned int, int, int, int, int);
extern int munmap(void *, unsigned int);

#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_ANON 0x20
#define _SC_PAGESIZE 11
#define MAP_FAILED ((void *)-1)

typedef void *(*ScreenStreamCreateFn)(void *,void *);
static ScreenStreamCreateFn g_screen_stream_create;
static uintptr_t g_libairplay_base;
static unsigned char *g_veneer;
static int g_installed;

static uint32_t arm_branch(uintptr_t pc,uintptr_t target,int link)
{
    int32_t delta=(int32_t)(target-(pc+8u));
    return (link?0xeb000000u:0xea000000u)|((uint32_t)(delta>>2)&0x00ffffffu);
}

static int guarded_store(volatile uint32_t *target,uint32_t expected,
                         uint32_t replacement,uint32_t alternate)
{
    uint32_t cur=*target;
    long ps;
    uintptr_t a,page;
    if(cur==replacement) return 1;
    if(cur!=expected&&(!alternate||cur!=alternate)) return 0;
    ps=sysconf(_SC_PAGESIZE);
    if(ps<=0) ps=(long)getpagesize();
    if(ps<=0) return 0;
    a=(uintptr_t)target; page=a&~((uintptr_t)ps-1u);
    if(mprotect((void *)page,(unsigned int)ps,PROT_READ|PROT_WRITE|PROT_EXEC)!=0) return 0;
    *target=replacement;
    __asm__ volatile("dmb sy" ::: "memory");
    __builtin___clear_cache((char *)page,(char *)(page+ps));
    return 1;
}

int mu0678_interpose_patch_word(volatile uint32_t *target,uint32_t expected,
                                uint32_t replacement,uint32_t alternate)
{
    return guarded_store(target,expected,replacement,alternate);
}

static void *screen_stream_create_hook(void *a,void *b)
{
    void *stream=g_screen_stream_create?g_screen_stream_create(a,b):(void *)0;
    if(stream&&mu0678_alt111_capture_active()) {
        mu0678_alt111_capture_stream(stream);
        mu0678_alt111_capture_video_impl(*(void **)((unsigned char *)stream+0x48));
    }
    return stream;
}

static void write_veneer_words(void)
{
    uint32_t *w=(uint32_t *)g_veneer;
    w[0]=0xe92d5fffu;
    w[1]=0xe350006fu;
    w[2]=0x1a000005u;
    w[3]=0xe1a00006u;
    w[4]=0xe59d1088u;
    w[5]=0xe59d209cu;
    w[6]=0xe1a0300au;
    w[7]=0xe59fc00cu;
    w[8]=0xe12fff3cu;
    w[9]=0xe8bd5fffu;
    w[10]=arm_branch((uintptr_t)g_veneer+0x28u,g_libairplay_base+0x231f0u,0);
    w[11]=0xe1a00000u;

    w[12]=0xe59d3014u;
    w[13]=0xe92d400fu;
    w[14]=0xe1a02003u;
    w[15]=0xe59fc01cu;
    w[16]=0xe12fff3cu;
    w[17]=0xe8bd400fu;
    w[18]=0xe59fe014u;
    w[19]=0xe59fc014u;
    w[20]=0xe12fff1cu;
    w[21]=0xe1a00000u;

    *(uintptr_t *)(g_veneer+0x30)=(uintptr_t)mu0678_interpose_setup111;
    *(uintptr_t *)(g_veneer+0x70)=(uintptr_t)mu0678_alt111_publish_decode;
    *(uintptr_t *)(g_veneer+0x74)=(uintptr_t)(g_libairplay_base+MU0678_NVSS_DECODE_CONT);
    *(uintptr_t *)(g_veneer+0x78)=(uintptr_t)(g_libairplay_base+MU0678_NVSS_DECODE_PLT);
    __builtin___clear_cache((char *)g_veneer,(char *)(g_veneer+0x80));
}

static int install_code_hooks(void)
{
    uintptr_t hint=g_libairplay_base+MU0678_VENEER_HINT;
    uintptr_t v;
    g_veneer=(unsigned char *)mmap((void *)hint,0x1000,PROT_READ|PROT_WRITE,
                                   MAP_PRIVATE|MAP_ANON,-1,0);
    if(g_veneer==MAP_FAILED){g_veneer=(unsigned char *)0;return -1;}
    v=(uintptr_t)g_veneer;
    if(v>g_libairplay_base+0x02000000u || v+0x80u<g_libairplay_base-0x02000000u){
        munmap(g_veneer,0x1000);g_veneer=(unsigned char *)0;return -2;
    }
    write_veneer_words();
    if(mprotect(g_veneer,0x1000,PROT_READ|PROT_EXEC)!=0) return -3;
    if(!guarded_store((volatile uint32_t *)(g_libairplay_base+MU0678_SETUP_BRANCH),
                      0x1affffe2u,arm_branch(g_libairplay_base+MU0678_SETUP_BRANCH,v,0),0)) return -4;
    if(!guarded_store((volatile uint32_t *)(g_libairplay_base+MU0678_NVSS_DECODE_CALL),
                      0xebfe6021u,arm_branch(g_libairplay_base+MU0678_NVSS_DECODE_CALL,v+0x40u,1),0)) return -5;
    return 0;
}

int mu0678_interpose_init(void)
{
    void *setup,*screen_create;
    volatile uint32_t *got;
    if(g_installed) return 0;
    setup=dlsym((void *)-2,"AirPlayReceiverSessionSetup");
    if(!setup) setup=dlsym((void *)0,"AirPlayReceiverSessionSetup");
    if(!setup) return -1;
    g_libairplay_base=(uintptr_t)setup-0x22fd8u;

    screen_create=dlsym((void *)-2,"ScreenStreamCreate");
    if(!screen_create) screen_create=dlsym((void *)0,"ScreenStreamCreate");
    if(!screen_create) return -2;
    g_screen_stream_create=(ScreenStreamCreateFn)screen_create;

    got=(volatile uint32_t *)(g_libairplay_base+MU0678_GOT_SCREEN_STREAM_CREATE);
    if(!guarded_store(got,(uint32_t)(g_libairplay_base+0x17d2cu),
                      (uint32_t)(uintptr_t)screen_stream_create_hook,(uint32_t)(g_libairplay_base+0x7f544u))) return -3;
    if(install_code_hooks()!=0) return -4;
    g_installed=1;
    return 0;
}

int mu0678_interpose_setup111(void *session,void *stream,void *setup_result,void *key_base)
{
    uint64_t negotiated;
    int port=0,listener=-1,err;
    void *response;
    typedef uint64_t (*GetInt64Fn)(void *,void *,void *);
    typedef void *(*DictCreateFn)(void *,unsigned int,void *,void *);
    typedef void (*DictSetInt64Fn)(void *,void *,unsigned int,unsigned int);
    typedef int (*AddResponseFn)(void *,void *);
    GetInt64Fn get64=(GetInt64Fn)dlsym((void *)-2,"CFDictionaryGetInt64");
    DictCreateFn create=(DictCreateFn)dlsym((void *)-2,"CFDictionaryCreateMutable");
    DictSetInt64Fn set64=(DictSetInt64Fn)dlsym((void *)-2,"CFDictionarySetInt64");
    AddResponseFn add=(AddResponseFn)(g_libairplay_base+0x22f34u);
    uintptr_t r4=g_libairplay_base+0xa9418u;
    void *keys=*(void **)(r4+0xfa0u);
    void *vals=*(void **)(r4+0xf78u);
    void *key_type=(void *)(g_libairplay_base+0x941dcu);
    void *key_port=(void *)(g_libairplay_base+0x942c4u);
    unsigned int allocator=*(unsigned char *)((unsigned char *)session+0x1408);

    if(!get64||!create||!set64||!add) return -10;
    negotiated=get64(stream,(unsigned char *)key_base+644u,(void *)0);
    if(mu0678_alt111_create(session,stream,0,negotiated,setup_result)!=0) return -11;
    if(mu0678_alt111_open_media_listener(session,(unsigned short *)&port,&listener)!=0) return -12;

    response=create((void *)(uintptr_t)allocator,allocator,keys,vals);
    if(!response) return -13;
    set64(response,key_type,111u,0u);
    set64(response,key_port,(unsigned int)port,0u);
    if(add(response,setup_result)!=0) return -14;

    err=mu0678_alt111_start_worker(session,mu0678_alt111_screen(),listener);
    if(err!=0) return -15;
    return 0;
}

int mu0678_interpose_session_finalized(void *session)
{
    mu0678_alt111_stop(session);
    return 0;
}
