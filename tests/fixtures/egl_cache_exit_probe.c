/* Pinned AOSP EGL lifetime regression. Only this test process is affected.
 * Loaded into a dedicated process. Queue a synthetic cache blob and call exit()
 * directly (toybox true skips exit callbacks), letting normal exit
 * destroy libEGL statics while an earlier exit callback keeps this process
 * alive long enough for any pending four-second cache save to finish.
 * No security check or failing mutex operation is suppressed.
 */
typedef unsigned long size_t;
extern void *dlopen(const char *, int);
extern char *getenv(const char *);
extern void *dlsym(void *, const char *);
extern int __cxa_atexit(void (*)(void *), void *, void *);
extern unsigned int sleep(unsigned int);
extern long write(int,const void *,size_t);
extern void _exit(int);
extern void exit(int);
extern int getpid(void);
extern int dprintf(int,const char *,...);
static void hold_exit(void *unused) {
    (void)unused;
    static const char message[]="EGL_CACHE_REGRESSION_LATE_EXIT\n";
    write(2,message,sizeof(message)-1);
    sleep(6);
    static const char done[]="EGL_CACHE_REGRESSION_EXIT_FINISHED\n";
    write(2,done,sizeof(done)-1);
}
__attribute__((constructor)) static void probe(void) {
    if (__cxa_atexit(hold_exit,(void *)0,(void *)0)) _exit(78);
    const char *library=getenv("AEGIS_EGL_TEST_LIBRARY");
    if (!library) library="/system/lib64/libEGL.so";
    void *lib=dlopen(library,2);
    if (!lib) _exit(79);
    void *(*get)(void)=dlsym(lib,"_ZN7android11egl_cache_t3getEv");
    void (*init)(void *,void *)=dlsym(lib,"_ZN7android11egl_cache_t10initializeEPNS_13egl_display_tE");
    void (*mode_update)(void *)=dlsym(lib,"_ZN7android11egl_cache_t10updateModeEv");
    void (*mode)(void *,int)=dlsym(lib,"_ZN7android11egl_cache_t12setCacheModeENS0_12EGLCacheModeE");
    void (*set)(void *,const void *,long,const void *,long)=dlsym(lib,"_ZN7android11egl_cache_t7setBlobEPKvlS2_l");
    if (!get||!init||!mode_update||!mode||!set) _exit(80);
    void *cache=get();
    init(cache,(void *)0); mode_update(cache); mode(cache,0);
    set(cache,"aegis-regression-key",20,"aegis-regression-value",22);
    dprintf(2,"EGL_CACHE_REGRESSION_QUEUED pid=%d cache=%p\n",getpid(),cache);
    exit(0);
}
