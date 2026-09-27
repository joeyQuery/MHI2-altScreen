#include "mu0678_interpose.h"
#include "mu0678_cluster.h"

extern int mprotect(void *, unsigned int, int);
extern long sysconf(int);
extern void *dlopen(const char *, int);
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
static void *g_airplay_handle;
static void *g_dio_handle;
static void *g_original_server_copy_property;
static void *g_original_session_finalized;

static void *resolve_dio(const char *name)
{
    if(!g_dio_handle) g_dio_handle=dlopen((const char *)0,0x80000100);
    return g_dio_handle?dlsym(g_dio_handle,name):(void *)0;
}

static void *server_copy_property_hook(void *server, void *key,
                                       const void *value, int *out_int, void *context)
{
    typedef void *(*CopyInfoFn)(void *);
    typedef void *(*DeepCopyFn)(void *,void *,unsigned int);
    typedef void *(*StringFn)(void *,const char *,unsigned int);
    typedef int (*EqualFn)(void *,void *);
    typedef void (*SetIntFn)(void *,void *,unsigned int,unsigned int);
    typedef void (*ReleaseFn)(void *);
    CopyInfoFn copy=(CopyInfoFn)mu0678_resolve_airplay("AirPlayCopyServerInfo");
    DeepCopyFn deep=(DeepCopyFn)mu0678_resolve_airplay("CFPropertyListCreateDeepCopy");
    StringFn make=(StringFn)mu0678_resolve_airplay("CFStringCreateWithCString");
    EqualFn equal=(EqualFn)mu0678_resolve_airplay("CFEqual");
    SetIntFn set=(SetIntFn)mu0678_resolve_airplay("CFDictionarySetInt64");
    ReleaseFn release=(ReleaseFn)mu0678_resolve_airplay("CFRelease");
    void *info,*copy_info,*k;
    (void)value; (void)out_int; (void)context;
    if(!copy||!deep||!make||!equal||!set) goto stock;
    k=make((void *)0,"info",0x08000100u);
    if(!k || !key || !equal(key,k)) {
        if(k&&release) release(k);
        goto stock;
    }
    info=copy(server);
    if(!info) { if(release) release(k); goto stock; }
    copy_info=deep((void *)0,info,2u);
    if(release) release(info);
    if(!copy_info) { if(release) release(k); goto stock; }
    {
        void *ak=make((void *)0,"altScreen",0x08000100u);
        if(ak) {
            set(copy_info,ak,1u,0u);
            if(release) release(ak);
        }
    }
    if(release) release(k);
    return copy_info;
stock:
    if(g_original_server_copy_property) {
        typedef void *(*Fn)(void *,void *,const void *,int *,void *);
        return ((Fn)g_original_server_copy_property)(server,key,value,out_int,context);
    }
    return (void *)0;
}

static void session_finalized_hook(void *session, void *context)
{
    typedef void (*Fn)(void *,void *);
    mu0678_interpose_session_finalized(session);
    if(g_original_session_finalized)
        ((Fn)g_original_session_finalized)(session,context);
}


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

void *mu0678_resolve_airplay(const char *name)
{
    if(!name) return (void *)0;
    if(!g_airplay_handle) g_airplay_handle=dlopen((const char *)0,0x80000100);
    return g_airplay_handle?dlsym(g_airplay_handle,name):(void *)0;
}

static void write_veneer_words(void)
{
    uint32_t *w=(uint32_t *)g_veneer;

    /* SETUP boundary: r6=session, r9=stream, r10=key-base,
       original setup_result is at [original_sp+0x68]. */
    w[0]=0xe92d5fffu;                 /* push {r0-r12,lr} */
    w[1]=0xe350006fu;                 /* cmp r0,#111 */
    w[2]=0x1a000005u;                 /* bne -> common stock return */
    w[3]=0xe1a00006u;                 /* r0 = session */
    w[4]=0xe59d10a0u;                 /* r1 = setup_result */
    w[5]=0xe1a02009u;                 /* r2 = current stream */
    w[6]=0xe1a0300au;                 /* r3 = key-base */
    w[7]=0xe59fc008u;                 /* ip = setup111 */
    w[8]=0xe12fff3cu;                 /* blx ip */
    w[9]=0xe8bd5fffu;                 /* restore caller registers */
    w[10]=0xe59ff000u;                /* pc = stock non-110 path */
    w[11]=(uint32_t)(uintptr_t)mu0678_interpose_setup111;
    w[12]=(uint32_t)(g_libairplay_base+0x231f0u);

    /* Nvss decode raw-call veneer: length is [sp+0x14]. */
    w[13]=0xe59d3014u;                /* r3 = original length */
    w[14]=0xe92d400fu;                /* push {r0-r3,lr} */
    w[15]=0xe1a02003u;                /* r2 = length */
    w[16]=0xe59fc010u;                /* ip = tee */
    w[17]=0xe12fff3cu;                /* blx ip */
    w[18]=0xe8bd400fu;                /* restore original regs/lr */
    w[19]=0xe59fe00cu;                /* lr = original continuation */
    w[20]=0xe59fc00cu;                /* ip = original NvSS PLT */
    w[21]=0xe12fff1cu;                /* bx ip */
    w[22]=(uint32_t)(uintptr_t)mu0678_alt111_publish_decode;
    w[23]=(uint32_t)(g_libairplay_base+MU0678_NVSS_DECODE_CONT);
    w[24]=(uint32_t)(g_libairplay_base+MU0678_NVSS_DECODE_PLT);

    __builtin___clear_cache((char *)g_veneer,(char *)(g_veneer+0x64));
}
static int install_code_hooks(void)
{
    uintptr_t hint=g_libairplay_base+MU0678_VENEER_HINT;
    uintptr_t v;
    volatile uint32_t *setup_word=(volatile uint32_t *)(g_libairplay_base+MU0678_SETUP_BRANCH);
    volatile uint32_t *decode_word=(volatile uint32_t *)(g_libairplay_base+MU0678_NVSS_DECODE_CALL);
    uint32_t old_setup=*setup_word;
    uint32_t old_decode=*decode_word;

    g_veneer=(unsigned char *)mmap((void *)hint,0x1000,PROT_READ|PROT_WRITE,
                                   MAP_PRIVATE|MAP_ANON,-1,0);
    if(g_veneer==MAP_FAILED){g_veneer=(unsigned char *)0;return -1;}
    v=(uintptr_t)g_veneer;
    if(v>=g_libairplay_base+0x01fffffcu || v<g_libairplay_base-0x01fffffcu){
        munmap(g_veneer,0x1000);g_veneer=(unsigned char *)0;return -2;
    }
    write_veneer_words();
    if(mprotect(g_veneer,0x1000,PROT_READ|PROT_EXEC)!=0) {
        munmap(g_veneer,0x1000); g_veneer=(unsigned char *)0; return -3;
    }
    if(!guarded_store(setup_word,0x1affffe2u,
                      arm_branch(g_libairplay_base+MU0678_SETUP_BRANCH,v,0),0))
        return -4;
    if(!guarded_store(decode_word,0xebfe6021u,
                      arm_branch(g_libairplay_base+MU0678_NVSS_DECODE_CALL,v+0x34u,1),0)) {
        guarded_store(setup_word,
                      arm_branch(g_libairplay_base+MU0678_SETUP_BRANCH,v,0),
                      old_setup,0);
        return -5;
    }
    (void)old_decode;
    return 0;
}
int mu0678_interpose_init(void)
{
    void *setup,*screen_create;
    volatile uint32_t *got;
    if(g_installed) return 0;
    setup=mu0678_resolve_airplay("AirPlayReceiverSessionSetup");
    if(!setup) return -1;
    g_libairplay_base=(uintptr_t)setup-0x22fd8u;

    screen_create=mu0678_resolve_airplay("ScreenStreamCreate");
    if(!screen_create) return -2;
    g_screen_stream_create=(ScreenStreamCreateFn)screen_create;

    got=(volatile uint32_t *)(g_libairplay_base+MU0678_GOT_SCREEN_STREAM_CREATE);
    {
        uint32_t got_old=*got;
        if(!guarded_store(got,(uint32_t)(g_libairplay_base+0x17d2cu),
                          (uint32_t)(uintptr_t)screen_stream_create_hook,
                          (uint32_t)(g_libairplay_base+0x7f544u))) return -3;
        if(install_code_hooks()!=0) {
            guarded_store(got,(uint32_t)(uintptr_t)screen_stream_create_hook,got_old,0);
            return -4;
        }
    }
    {
        void *server_cb=resolve_dio("_ZN3dio11CDIOManager28s_serverCopyPropertyCallbackEP28AirPlayReceiverServerPrivateP9CFLStringPKvPiPv");
        void *final_cb=resolve_dio("_ZN3dio11CDIOManager26s_sessionFinalizedCallbackEP29AirPlayReceiverSessionPrivatePv");
        uintptr_t dio_base;
        volatile uint32_t *server_got;
        volatile uint32_t *final_got;
        if(!server_cb||!final_cb) return -5;
        dio_base=(uintptr_t)server_cb-0x139a88u;
        server_got=(volatile uint32_t *)(dio_base+0x19d9d8u);
        final_got=(volatile uint32_t *)(dio_base+0x19da00u);
        g_original_server_copy_property=(void *)(uintptr_t)(*server_got);
        g_original_session_finalized=(void *)(uintptr_t)(*final_got);
        if(!guarded_store(server_got,(uint32_t)(uintptr_t)server_cb,
                          (uint32_t)(uintptr_t)server_copy_property_hook,0)) return -6;
        if(!guarded_store(final_got,(uint32_t)(uintptr_t)final_cb,
                          (uint32_t)(uintptr_t)session_finalized_hook,0)) {
            guarded_store(server_got,(uint32_t)(uintptr_t)server_copy_property_hook,
                          (uint32_t)(uintptr_t)g_original_server_copy_property,0);
            return -7;
        }
    }
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
    GetInt64Fn get64=(GetInt64Fn)mu0678_resolve_airplay("CFDictionaryGetInt64");
    DictCreateFn create=(DictCreateFn)mu0678_resolve_airplay("CFDictionaryCreateMutable");
    DictSetInt64Fn set64=(DictSetInt64Fn)mu0678_resolve_airplay("CFDictionarySetInt64");
    AddResponseFn add=(AddResponseFn)(g_libairplay_base+0x22f34u);
    uintptr_t r4=g_libairplay_base+0xa9418u;
    void *keys=*(void **)(r4+0xfa0u);
    void *vals=*(void **)(r4+0xf78u);
    void *key_type=(void *)(g_libairplay_base+0x941dcu);
    void *key_port=(void *)(g_libairplay_base+0x942c4u);
 
    if(!get64||!create||!set64||!add) return -10;
    negotiated=get64(stream,(unsigned char *)key_base+644u,(void *)0);
    if(mu0678_alt111_create(session,stream,0,negotiated,setup_result)!=0) return -11;
    {
        void *time_blob[3];
        time_blob[0]=setup_result;
        time_blob[1]=(void *)(g_libairplay_base+0x20650u);
        time_blob[2]=(void *)(g_libairplay_base+0x20644u);
        if(mu0678_alt111_set_time(mu0678_alt111_screen(),time_blob)!=0) return -12;
    }
    if(mu0678_alt111_open_media_listener(session,(unsigned short *)&port,&listener)!=0) return -13;

    response=create((void *)0,0,keys,vals);
    if(!response) return -14;
    set64(response,key_type,111u,0u);
    set64(response,key_port,(unsigned int)port,0u);
    if(add(response,setup_result)!=0) return -15;

    err=mu0678_alt111_start_worker(session,mu0678_alt111_screen(),listener);
    if(err!=0) return -16;
    return 0;
}

int mu0678_interpose_session_finalized(void *session)
{
    mu0678_alt111_stop(session);
    return 0;
}
