#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "package_apt_hook.h"
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#define MAX_MESSAGE 262144u
static int record(char* bytes,size_t* used) {
    *used=0;
    while(*used<MAX_MESSAGE) {
        char c;ssize_t n=read(3,&c,1);if(n<0&&errno==EINTR)continue;
        if(n!=1 || !c)return -1;
        bytes[(*used)++]=c;
        if(c=='\n') {
            if(*used<2 || bytes[*used-2]!='\n')continue;
            bytes[*used-2]=0;*used-=2;return 0;
        }
    }
    errno=EFBIG;return -1;
}
static int write_all(int fd,const char* bytes,size_t size) {
    for(size_t at=0;at<size;) { ssize_t n=write(fd,bytes+at,size-at);if(n<0&&errno==EINTR)continue;if(n<=0)return -1;at+=n; }
    return 0;
}
int aegis_apt_plan_hook(void) {
    const char* channel=getenv("APT_HOOK_SOCKET");struct stat st;
    int domain=0,type=0;socklen_t length=sizeof(int);
    if(!channel || strcmp(channel,"3") || getuid()!=0 || getpid()==1
       || fstat(3,&st)<0 || !S_ISSOCK(st.st_mode)
       || getsockopt(3,SOL_SOCKET,SO_DOMAIN,&domain,&length)<0 || domain!=AF_UNIX
       || getsockopt(3,SOL_SOCKET,SO_TYPE,&type,&length)<0 || type!=SOCK_STREAM)return 121;
    if(syscall(SYS_close_range,4u,~0u,0u)<0)return 122;
    alarm(30);char* bytes=malloc(MAX_MESSAGE+1);if(!bytes)return 123;
    int result=124;size_t size,notice_size=0;char* notice=NULL;
    const char hello[]="{\"jsonrpc\":\"2.0\",\"method\":\"org.debian.apt.hooks.hello\",";
    if(record(bytes,&size)<0 || strncmp(bytes,hello,sizeof(hello)-1)
       || !strstr(bytes,"\"0.2\""))goto done;
    const char response[]="{\"jsonrpc\":\"2.0\",\"id\":0,\"result\":{\"version\":\"0.2\"}}\n\n";
    if(write_all(3,response,sizeof(response)-1)<0 || record(bytes,&size)<0)goto done;
    const char prefix[]="{\"jsonrpc\":\"2.0\",\"method\":\"org.debian.apt.hooks.install.pre-prompt\",";
    if(!strncmp(bytes,prefix,sizeof(prefix)-1)) {
        notice=malloc(size+1);if(!notice)goto done;
        memcpy(notice,bytes,size+1);notice_size=size;
    }
    if(record(bytes,&size)<0)goto done;
    const char bye[]="{\"jsonrpc\":\"2.0\",\"method\":\"org.debian.apt.hooks.bye\",\"params\":{}}";
    if(strcmp(bytes,bye))goto done;
    if(notice) {
        const char* tmp="/run/aegis-apt-plan.tmp";
        int out=open(tmp,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC|O_NOFOLLOW,0600);
        if(out<0)goto done;
        int okay=write_all(out,notice,notice_size)==0 && fsync(out)==0;int closed=close(out);
        if(!okay||closed<0) { unlink(tmp);goto done; }
        // The consumer accepts only a complete notification and completed bye.
        // Atomic no-replace publication within this job's private scratch mount.
        // A hardlink would need extra SELinux link authority and briefly leave
        // two names for the record. Rename uses the existing scratch permissions.
        if(syscall(SYS_renameat2,AT_FDCWD,tmp,AT_FDCWD,
                   "/run/aegis-apt-plan.json",RENAME_NOREPLACE)<0) {
            unlink(tmp);goto done;
        }
    }
    result=0;
done:
    free(notice);free(bytes);return result;
}
