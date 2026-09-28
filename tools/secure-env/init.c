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
static void device(const char *path,unsigned major,unsigned minor,unsigned mode) {
    unsigned dev=(major<<8)|(minor&255)|((minor&~255)<<12);
    check(call(33,-100,(long)path,mode,dev,0,0),"helper: mknod failed\n");
}
static void node(const char *path,unsigned major,unsigned minor) { device(path,major,minor,0020600); }
static void sysnode(const char *sysfile,const char *path,unsigned mode) {
    long fd=-1;
    for(int i=0;i<50 && fd<0;i++) {
        fd=openfile(sysfile,0);
        if(fd<0){long t[2]={0,100000000};call(101,(long)t,0,0,0,0,0);}
    }
    check(fd,"helper: missing tty sysfs entry\n");
    char buf[32];long n=call(63,fd,(long)buf,31,0,0,0);
    call(57,fd,0,0,0,0,0);check(n,"helper: tty sysfs read failed\n");
    unsigned major=0,minor=0;long i=0;
    while(i<n && buf[i]>='0' && buf[i]<='9')major=major*10+buf[i++]-'0';
    if(i==0 || i>=n || buf[i++]!=':')die("helper: invalid tty device\n");
    long start=i;
    while(i<n && buf[i]>='0' && buf[i]<='9')minor=minor*10+buf[i++]-'0';
    if(i==start)die("helper: invalid tty minor\n");
    device(path,major,minor,mode);
}
static void ttynode(const char *sysfile,const char *path) { sysnode(sysfile,path,0020600); }
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
static int same(const char *a,const char *b,usize length) {
    for(usize i=0;i<length;i++)if(a[i]!=b[i])return 0;
    return 1;
}
static long readfile(const char *path,char *buf,usize size) {
    long fd=openfile(path,0);if(fd<0)return fd;
    long n=call(63,fd,(long)buf,size,0,0,0);call(57,fd,0,0,0,0,0);return n;
}
static long nvsize(void) {
    long fd=openfile("/state/NVChip",0);if(fd<0)return fd;
    long n=call(62,fd,0,2,0,0,0);call(57,fd,0,0,0,0,0);return n;
}
static int persistent(void) {
    char expected[38],actual[38];
    long n=readfile("/etc/aegis-profile-id",expected,sizeof(expected));
    if(n==-2)return 0;
    if(n!=37 || expected[36]!='\n')die("helper: invalid expected profile identity\n");
    module("/lib/modules/virtio_blk.ko");
    sysnode("/sys/class/block/vda/dev","/dev/vda",0060600);
    /* MS_SYNCHRONOUS: TPM NV fflush() must reach the backing disk before
     * Android receives a reply that lets it commit data using those keys. */
    check(call(40,(long)"/dev/vda",(long)"/state",(long)"ext4",16,0,0),
          "helper: persistent state mount failed; refusing ephemeral fallback\n");
    if(readfile("/state/profile-id",actual,sizeof(actual))!=n || !same(expected,actual,n))
        die("helper: wrong persistent state profile\n");
    /* Consume this one-time provisioning marker before secure_env starts.
     * Once consumed, a missing NVChip must never manufacture replacement keys.
     */
    long fresh=openfile("/state/fresh",0);
    if(fresh>=0) {
        call(57,fresh,0,0,0,0,0);
        if(nvsize()>=0)die("helper: fresh profile already contains TPM state\n");
        check(call(35,-100,(long)"/state/fresh",0,0,0,0),"helper: cannot consume fresh marker\n");
        call(81,0,0,0,0,0,0);
    } else {
        long expected_size=0;
        if(readfile("/state/nv-size",(char*)&expected_size,sizeof(expected_size))!=sizeof(expected_size)
           || expected_size<=0 || nvsize()!=expected_size)
            die("helper: missing or invalid TPM state; refusing key regeneration\n");
    }
    /* This socket is recreated on every process start. */
    call(35,-100,(long)"/state/confui.sock",0,0,0,0);
    say("helper: persistent state profile verified\n");
    return 1;
}
__attribute__((noreturn)) static void shutdown_helper(long child,int mounted) {
    if(child>0)call(129,child,15,0,0,0,0);
    int status=0;
    for(int i=0;child>0 && i<100;i++) {
        long result=call(260,child,(long)&status,1,0,0,0);
        if(result==child || result==-10)break;
        long t[2]={0,100000000};call(101,(long)t,0,0,0,0,0);
        if(i==99) {
            call(129,child,9,0,0,0,0);
            call(260,child,(long)&status,0,0,0,0);
        }
    }
    call(81,0,0,0,0,0,0);
    if(mounted)check(call(39,(long)"/state",0,0,0,0,0),"helper: state unmount failed\n");
    say("AEGIS_HELPER_SHUTDOWN_CLEAN\n");
    call(142,0xfee1dead,0x28121969,0x4321fedc,0,0,0);
    die("helper: poweroff failed\n");
}
__attribute__((noreturn)) static void supervise(long child,int mounted) {
    /* Parent stays outside the mounted filesystem and owns shutdown. */
    if(mounted) {
        long size=-1;
        for(int i=0;i<200 && size<=0;i++) {
            size=nvsize();
            long t[2]={0,100000000};call(101,(long)t,0,0,0,0,0);
        }
        if(size<=0)die("helper: TPM did not create its persistent state\n");
        long fd=openfile("/state/nv-size",1|0100|01000);
        check(fd,"helper: cannot record TPM state size\n");
        if(call(64,fd,(long)&size,sizeof(size),0,0,0)!=sizeof(size))die("helper: state size write failed\n");
        call(57,fd,0,0,0,0,0);call(81,0,0,0,0,0,0);
    }
    say("AEGIS_HELPER_READY\n");
    char command[32];usize used=0;
    for(;;) {
        int status=0;
        if(call(260,child,(long)&status,1,0,0,0)==child) {
            say("helper: secure_env exited\n");shutdown_helper(0,mounted);
        }
        struct {int fd;short events,revents;} pollfd={0,1,0};
        long timeout[2]={1,0};
        long ready=call(73,(long)&pollfd,1,(long)timeout,0,0,0);
        if(ready<=0 || !(pollfd.revents&1))continue;
        char c;if(call(63,0,(long)&c,1,0,0,0)!=1)continue;
        if(c=='\n' || c=='\r') {
            if(used==8 && same(command,"poweroff",8))shutdown_helper(child,mounted);
            if(used==4 && same(command,"sync",4)) {call(81,0,0,0,0,0,0);say("AEGIS_HELPER_SYNCED\n");}
            used=0;
        } else if(used<sizeof(command))command[used++]=c;
    }
}
__attribute__((noreturn)) void _start(void) {
    /* Android GKI omits devtmpfs; populate this helper's private /dev. */
    mountfs("tmpfs","/dev");mountfs("proc","/proc");mountfs("sysfs","/sys");
    node("/dev/console",5,1);node("/dev/null",1,3);node("/dev/zero",1,5);
    node("/dev/random",1,8);node("/dev/urandom",1,9);
    long console=openfile("/dev/console",2);check(console,"helper: no console\n");
    dupfd(console,0);dupfd(console,1);dupfd(console,2);
    if(console>2)call(57,console,0,0,0,0,0);
    say("AegisOS secure_env helper init\n");
    module("/lib/modules/virtio_pci_modern_dev.ko");
    module("/lib/modules/virtio_pci_legacy_dev.ko");
    module("/lib/modules/virtio_pci.ko");
    module("/lib/modules/virtio_console.ko");
    int mounted=persistent();
    ttynode("/sys/class/tty/hvc0/dev","/dev/hvc0");
    ttynode("/sys/class/tty/hvc1/dev","/dev/hvc1");
    ttynode("/sys/class/tty/hvc2/dev","/dev/hvc2");
    ttynode("/sys/class/tty/hvc3/dev","/dev/hvc3");
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
    long child=call(220,17,0,0,0,0,0);check(child,"helper: fork failed\n");
    if(child) {
        /* In particular, release the parent's bound /state/confui.sock so
         * unmount is possible after the child has exited. */
        for(int fd=3;fd<=32;fd++)call(57,fd,0,0,0,0,0);
        supervise(child,mounted);
    }
    check(call(49,(long)"/state",0,0,0,0,0),"helper: chdir failed\n");
    char *argv[]={"/host/bin/secure_env","--keymint_fd_in=10","--keymint_fd_out=11",
      "--gatekeeper_fd_in=12","--gatekeeper_fd_out=13","--keymaster_fd_in=14","--keymaster_fd_out=15",
      "--oemlock_fd_in=16","--oemlock_fd_out=17","--snapshot_control_fd=20","--kernel_events_fd=30",
      "--confui_server_fd=32","--tpm_impl=in_memory","--keymint_impl=tpm","--gatekeeper_impl=tpm","--oemlock_impl=tpm",0};
    char *env[]={"LD_LIBRARY_PATH=/host/lib64","HOME=/state","TMPDIR=/tmp",
      "CUTTLEFISH_CONFIG_FILE=/state/cuttlefish_config.json","CUTTLEFISH_INSTANCE=1","PATH=/host/bin",0};
    say("helper: executing original Cuttlefish secure_env\n");
    call(221,(long)argv[0],(long)argv,(long)env,0,0,0);
    say("helper: secure_env exec failed\n");call(94,127,0,0,0,0,0);
    die("helper: exit failed\n");
}
