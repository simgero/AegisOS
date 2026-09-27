/* Minimal ARM64 Linux init for the local QEMU secure_env helper.
 * Compiled on aegis-build with -nostdlib; no host services or network exposed.
 * This is a development TPM emulator, not a hardware-backed production TEE.
 */
typedef unsigned long usize;
static long call(long n,long a,long b,long c,long d,long e,long f) {
    register long x8 __asm__("x8")=n;
    register long x0 __asm__("x0")=a, x1 __asm__("x1")=b, x2 __asm__("x2")=c;
    register long x3 __asm__("x3")=d, x4 __asm__("x4")=e, x5 __asm__("x5")=f;
    __asm__ volatile("svc 0" : "+r"(x0) : "r"(x8),"r"(x1),"r"(x2),"r"(x3),"r"(x4),"r"(x5) : "memory");
    return x0;
}
void *memset(void *p,int c,usize n) { unsigned char *q=p;while(n--)*q++=c;return p; }
void *memcpy(void *p,const void *s,usize n) { unsigned char *q=p;const unsigned char *r=s;while(n--)*q++=*r++;return p; }
static void say(const char *s) { usize n=0;while(s[n])n++;call(64,2,(long)s,n,0,0,0); }
__attribute__((noreturn)) static void die(const char *s) { say(s);for(;;){long t[2]={60,0};call(101,(long)t,0,0,0,0,0);} }
static void check(long r,const char *s) { if(r<0)die(s); }
static void mountfs(const char *type,const char *where) {
    check(call(40,(long)type,(long)where,(long)type,0,0,0),"helper: mount failed\n");
}
static long openfile(const char *s,long flags) { return call(56,-100,(long)s,flags,0600,0,0); }
static void dupfd(long from,int to) { if(from!=to)check(call(24,from,to,0,0,0,0),"helper: dup failed\n"); }
static void module(const char *path) {
    long fd=openfile(path,0);check(fd,"helper: missing kernel module\n");
    long r=call(273,fd,(long)"",0,0,0,0);
    if(r<0 && r!=-17)die("helper: finit_module failed\n");
    call(57,fd,0,0,0,0,0);
}
static void channel(const char *path,int in,int out) {
    long fd=-1;
    for(int i=0;i<50 && fd<0;i++) {
        fd=openfile(path,2|0400);
        if(fd<0){long t[2]={0,100000000};call(101,(long)t,0,0,0,0,0);}
    }
    check(fd,"helper: missing virtio console channel\n");
    struct { unsigned int input,output,control,local; unsigned char line,cc[19]; } term;
    check(call(29,fd,0x5401,(long)&term,0,0,0),"helper: TCGETS failed\n");
    term.input=0;term.output=0;term.control=0x8b0;term.local=0;
    term.cc[5]=0;term.cc[6]=1;
    check(call(29,fd,0x5402,(long)&term,0,0,0),"helper: TCSETS failed\n");
    dupfd(fd,in);dupfd(fd,out);call(57,fd,0,0,0,0,0);
}
__attribute__((noreturn)) void _start(void) {
    mountfs("devtmpfs","/dev");mountfs("proc","/proc");mountfs("sysfs","/sys");
    long console=openfile("/dev/console",2);check(console,"helper: no console\n");
    dupfd(console,0);dupfd(console,1);dupfd(console,2);
    if(console>2)call(57,console,0,0,0,0,0);
    say("AegisOS secure_env helper init\n");
    module("/lib/modules/virtio_pci_modern_dev.ko");
    module("/lib/modules/virtio_pci_legacy_dev.ko");
    module("/lib/modules/virtio_pci.ko");
    module("/lib/modules/virtio_console.ko");
    channel("/dev/hvc0",10,11);channel("/dev/hvc1",12,13);
    channel("/dev/hvc2",14,15);channel("/dev/hvc3",16,17);
    int pair[2];check(call(199,1,1,0,(long)pair,0,0),"helper: socketpair failed\n");
    dupfd(pair[0],20);dupfd(pair[1],21);call(57,pair[0],0,0,0,0,0);call(57,pair[1],0,0,0,0,0);
    check(call(59,(long)pair,0,0,0,0,0),"helper: pipe failed\n");
    dupfd(pair[0],30);dupfd(pair[1],31);call(57,pair[0],0,0,0,0,0);call(57,pair[1],0,0,0,0,0);
    long sock=call(198,1,1,0,0,0,0);check(sock,"helper: socket failed\n");
    struct {unsigned short family;char path[108];} address={1,"/state/confui.sock"};
    check(call(200,sock,(long)&address,sizeof(address),0,0,0),"helper: bind failed\n");
    check(call(201,sock,4,0,0,0,0),"helper: listen failed\n");dupfd(sock,32);call(57,sock,0,0,0,0,0);
    check(call(49,(long)"/state",0,0,0,0,0),"helper: chdir failed\n");
    char *argv[]={"/host/bin/secure_env","--keymint_fd_in=10","--keymint_fd_out=11",
      "--gatekeeper_fd_in=12","--gatekeeper_fd_out=13","--keymaster_fd_in=14","--keymaster_fd_out=15",
      "--oemlock_fd_in=16","--oemlock_fd_out=17","--snapshot_control_fd=20","--kernel_events_fd=30",
      "--confui_server_fd=32","--tpm_impl=in_memory","--keymint_impl=tpm","--gatekeeper_impl=tpm","--oemlock_impl=tpm",0};
    char *env[]={"LD_LIBRARY_PATH=/host/lib64","HOME=/state","TMPDIR=/tmp",
      "CUTTLEFISH_CONFIG_FILE=/state/cuttlefish_config.json","CUTTLEFISH_INSTANCE=1","PATH=/host/bin",0};
    say("helper: executing original Cuttlefish secure_env\n");
    call(221,(long)argv[0],(long)argv,(long)env,0,0,0);
    die("helper: secure_env exec failed\n");
}
