#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "package_candidate_labels.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/openat2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <unistd.h>
#define MAX_DEPTH 128u
#define MAX_ENTRIES 1000000u
static int fail(int error) { errno=error;return -1; }
static int at(int root,const char *name) {
    struct open_how how={.flags=O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC,
        .resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_XDEV};
    return (int)syscall(SYS_openat2,root,name,&how,sizeof(how));
}
static int exact(const char *label,ssize_t count) {
    if(count<0)return -1;
    if(count && label[count-1]=='\0')--count;
    const char expected[]=AEGIS_PACKAGE_CANDIDATE_CONTEXT;
    return count==(ssize_t)sizeof(expected)-1 && !memcmp(label,expected,sizeof(expected)-1)
        ? 0 : fail(EPERM);
}
static int root_label(int root) {
    char label[128];ssize_t count=fgetxattr(root,"security.selinux",label,sizeof(label));
    return exact(label,count);
}
static int entry_label(int root,const char *name) {
    /* lgetxattr follows our held DIRECTORY's procfs magic link, but never the
     * final candidate component. O_PATH/fgetxattr cannot inspect symlink fds.
     * name comes from one readdir entry and cannot supply another component. */
    if(!*name || strchr(name,'/') || !strcmp(name,".") || !strcmp(name,".."))return fail(EPROTO);
    char path[NAME_MAX+64],label[128];
    int n=snprintf(path,sizeof(path),"/proc/self/fd/%d/%s",root,name);
    if(n<0 || (size_t)n>=sizeof(path))return fail(ENAMETOOLONG);
    ssize_t count=lgetxattr(path,"security.selinux",label,sizeof(label));
    return exact(label,count);
}
static int same(const struct stat *a,const struct stat *b) {
    return a->st_dev==b->st_dev && a->st_ino==b->st_ino
        && (a->st_mode&S_IFMT)==(b->st_mode&S_IFMT);
}
static int runtime_root(const char *name) {
    return !strcmp(name,"dev") || !strcmp(name,"proc") || !strcmp(name,"tmp") || !strcmp(name,"run");
}
static int tree(int root,dev_t device,int runtime_mounts,unsigned depth,unsigned *count) {
    if(depth>MAX_DEPTH)return fail(E2BIG);
    int scan=at(root,".");if(scan<0)return -1;
    DIR *entries=fdopendir(scan);
    if(!entries) { int saved=errno;close(scan);return fail(saved); }
    int error=0;
    for(;;) {
        errno=0;struct dirent *entry=readdir(entries);
        if(!entry) { error=errno;break; }
        if(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,".."))continue;
        if(++*count>MAX_ENTRIES) { error=E2BIG;break; }
        struct stat before,after;
        if(fstatat(root,entry->d_name,&before,AT_SYMLINK_NOFOLLOW)<0) { error=errno;break; }
        if(before.st_dev!=device) {
            if(!depth && runtime_mounts && S_ISDIR(before.st_mode) && runtime_root(entry->d_name))continue;
            error=EXDEV;break;
        }
        if(!(S_ISDIR(before.st_mode)||S_ISREG(before.st_mode)||S_ISLNK(before.st_mode))) { error=EPERM;break; }
        if(entry_label(root,entry->d_name)<0 || fstatat(root,entry->d_name,&after,AT_SYMLINK_NOFOLLOW)<0) {
            error=errno;break;
        }
        if(!same(&before,&after)) { error=ESTALE;break; }
        if(S_ISDIR(before.st_mode)) {
            int child=at(root,entry->d_name);if(child<0) { error=errno;break; }
            if(fstat(child,&after)<0)error=errno;
            else if(!same(&before,&after))error=ESTALE;
            else if(root_label(child)<0 || tree(child,device,0,depth+1,count)<0)error=errno;
            close(child);if(error)break;
        }
    }
    closedir(entries);return error ? fail(error) : 0;
}
int aegis_package_candidate_labels(int root,int runtime_mounts) {
    if(runtime_mounts!=0 && runtime_mounts!=1)return fail(EINVAL);
    struct stat st;if(fstat(root,&st)<0)return -1;
    if(!S_ISDIR(st.st_mode))return fail(ENOTDIR);
    if(root_label(root)<0)return -1;
    unsigned count=0;
    if(tree(root,st.st_dev,runtime_mounts,0,&count)<0)return -1;
    return root_label(root);
}
