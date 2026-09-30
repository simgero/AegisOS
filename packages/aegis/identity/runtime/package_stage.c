#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "package_stage.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>

static int fail(int e) { errno=e;return -1; }
static int at(int parent,const char *name,int flags) {
    struct open_how how={.flags=flags|O_CLOEXEC|O_NOFOLLOW,
        .resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_XDEV};
    return syscall(SYS_openat2,parent,name,&how,sizeof(how));
}
static int directory(int fd,struct stat *st) {
    if(fstat(fd,st)<0)return -1;
    if(st->st_mode!=(S_IFDIR|0700)||st->st_uid||st->st_gid||!st->st_nlink)return fail(EPERM);
    const char *attrs[]={"system.posix_acl_access","system.posix_acl_default"};
    for(unsigned i=0;i<2;++i) {
        if(fgetxattr(fd,attrs[i],NULL,0)>=0)return fail(EPERM);
        if(errno!=ENODATA&&errno!=EOPNOTSUPP)return -1;
    }
    return 0;
}
void aegis_package_stage_close(struct aegis_package_stage *s) {
    if(!s)return;
    if(s->directory>=0)close(s->directory);
    if(s->parent>=0)close(s->parent);
    *s=(struct aegis_package_stage)AEGIS_PACKAGE_STAGE_INIT;
}
static int named(struct aegis_package_stage *s) {
    struct stat parent,current,entry;
    if(!s->inode||!s->name[0]||!memchr(s->name,0,sizeof(s->name)))return fail(ESTALE);
    if(directory(s->parent,&parent)<0)return -1;
    if(s->directory<0) {
        s->directory=at(s->parent,s->name,O_RDONLY|O_DIRECTORY);
        if(s->directory<0)return -1;
    }
    if(directory(s->directory,&current)<0 || fstatat(s->parent,s->name,&entry,AT_SYMLINK_NOFOLLOW)<0)return -1;
    if(current.st_dev!=s->device||current.st_ino!=s->inode
       ||entry.st_dev!=s->device||entry.st_ino!=s->inode||parent.st_dev!=s->device)return fail(ESTALE);
    return 0;
}
int aegis_package_stage_create(int parent,const char *name,struct aegis_package_stage *s) {
    if(!s||s->parent!=-1||s->directory!=-1||s->inode||s->removed||s->name[0]
       ||!name||strncmp(name,"job-",4)||strlen(name)>=sizeof(s->name))return fail(EINVAL);
    for(const char *p=name;*p;++p)
        if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'))return fail(EINVAL);
    int held=fcntl(parent,F_DUPFD_CLOEXEC,3);if(held<0)return -1;
    struct stat st;
    if(directory(held,&st)<0) { int e=errno;close(held);return fail(e); }
    if(mkdirat(held,name,0700)<0) { int e=errno;close(held);return fail(e); }
    // Mutation now belongs to the registered job, also on every later failure.
    s->parent=held;memcpy(s->name,name,strlen(name)+1);
    if(fstatat(held,name,&st,AT_SYMLINK_NOFOLLOW)<0)return -1;
    s->device=st.st_dev;s->inode=st.st_ino;
    return named(s);
}
int aegis_package_stage_cleanup(struct aegis_package_stage *s,uint64_t image_bytes,uint64_t request_bytes) {
    if(!s)return fail(EINVAL);
    if(s->parent<0)return s->directory<0?0:fail(EINVAL);
    if(s->removed) {
        if(fsync(s->parent)<0)return -1;
        aegis_package_stage_close(s);return 0;
    }
    if(!image_bytes||!request_bytes)return fail(EINVAL);
    if(named(s)<0)return -1;
    const char *names[]={"request","candidate.ext4"};
    const uint64_t maxima[]={request_bytes,image_bytes};
    int files[]={-1,-1};struct stat pinned[2];int error=0,scan=-1;
    DIR *entries=NULL;unsigned seen=0;
    scan=at(s->directory,".",O_RDONLY|O_DIRECTORY);
    if(scan<0)return -1;
    entries=fdopendir(scan);
    if(!entries) { error=errno;close(scan);return fail(error); }
    for(;;) {
        errno=0;struct dirent *entry=readdir(entries);
        if(!entry) { error=errno;break; }
        if(++seen>5) { error=E2BIG;break; }
        if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
        unsigned i;
        for(i=0;i<2;++i)if(!strcmp(entry->d_name,names[i]))break;
        if(i==2||files[i]>=0) { error=ENOTEMPTY;break; }
        files[i]=at(s->directory,names[i],O_RDONLY|O_NONBLOCK);
        if(files[i]<0||fstat(files[i],&pinned[i])<0) { error=errno;break; }
        const struct stat *st=&pinned[i];
        if(!S_ISREG(st->st_mode)||st->st_uid||st->st_gid||st->st_nlink!=1
           ||st->st_dev!=s->device||st->st_size<0||(uint64_t)st->st_size>maxima[i]
           ||(st->st_mode!=(S_IFREG|0600)&&(i!=0||st->st_mode!=(S_IFREG|0400)))) { error=EPERM;break; }
    }
    if(closedir(entries)<0&&!error)error=errno;
    // Validate the entire bounded inventory before unlinking even the first file.
    if(!error&&named(s)<0)error=errno;
    for(unsigned i=0;!error&&i<2;++i)if(files[i]>=0) {
        struct stat current;
        if(fstatat(s->directory,names[i],&current,AT_SYMLINK_NOFOLLOW)<0) { error=errno;break; }
        if(current.st_dev!=pinned[i].st_dev||current.st_ino!=pinned[i].st_ino
           ||current.st_mode!=pinned[i].st_mode||current.st_uid||current.st_gid
           ||current.st_nlink!=1||current.st_size!=pinned[i].st_size) { error=ESTALE;break; }
        if(unlinkat(s->directory,names[i],0)<0)error=errno;
    }
    for(unsigned i=0;i<2;++i)if(files[i]>=0)close(files[i]);
    if(error)return fail(error);
    if(fsync(s->directory)<0||named(s)<0||unlinkat(s->parent,s->name,AT_REMOVEDIR)<0)return -1;
    s->removed=1;
    if(fsync(s->parent)<0)return -1;
    aegis_package_stage_close(s);return 0;
}
