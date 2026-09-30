#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "package_validate.h"
#include "package_candidate_labels.h"
#include "uid_layout.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/openat2.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
#include <sys/wait.h>
#include <unistd.h>
#define MAX_ACCOUNTS 2048u
#define MAX_TEXT (1024u * 1024u)
#define MAX_ENTRIES 1000000u
struct user { char *name; unsigned uid, gid; int shadow; };
struct group { char *name, *members; unsigned gid; int shadow; };
static int fail(int error) { errno=error;return -1; }
static int allowed(unsigned id) {
    for (size_t i=0;i<sizeof(aegis_uid_extents)/sizeof(aegis_uid_extents[0]);i++)
        if (id>=aegis_uid_extents[i].inside && id-aegis_uid_extents[i].inside<aegis_uid_extents[i].count) return 1;
    return 0;
}
static int at(int root,const char *path,int flags) {
    struct open_how how={.flags=(uint64_t)(flags|O_NOFOLLOW|O_CLOEXEC),
        .resolve=RESOLVE_BENEATH|RESOLVE_NO_SYMLINKS|RESOLVE_NO_XDEV};
    return (int)syscall(SYS_openat2,root,path,&how,sizeof(how));
}
static int name_ok(const char *s) {
    size_t n=strlen(s);if (!n || n>127) return 0;
    for (size_t i=0;i<n;i++) if (!((s[i]>='a'&&s[i]<='z') || (s[i]>='A'&&s[i]<='Z')
        || (s[i]>='0'&&s[i]<='9') || s[i]=='_' || s[i]=='-' || s[i]=='.')) return 0;
    return 1;
}
static int number(const char *s,unsigned *out) {
    unsigned value=0;if (!*s) return -1;
    for (;*s;s++) { if (*s<'0'||*s>'9'||value>6553u) return -1;value=value*10u+(unsigned)(*s-'0'); }
    if (value>65534u) return -1;*out=value;return 0;
}
static char *line(char **cursor) {
    if (!*cursor || !**cursor) return NULL;
    char *start=*cursor,*end=strchr(start,'\n');
    if (end) { *end=0;*cursor=end+1; } else *cursor=NULL;
    return start;
}
static int fields(char *row,char **values,unsigned count) {
    if (!*row || strchr(row,'\r')) return -1;
    for (unsigned i=0;i<count;i++) {
        values[i]=row;char *end=strchr(row,':');
        if (i+1==count) return end ? -1 : 0;
        if (!end) return -1;*end=0;row=end+1;
    }
    return -1;
}
static int absent_xattrs(int fd) {
    const char *names[]={"security.capability","system.posix_acl_access","system.posix_acl_default"};
    for (unsigned i=0;i<3;i++) {
        if (fgetxattr(fd,names[i],NULL,0)>=0) return fail(EPERM);
        if (errno!=ENODATA && errno!=EOPNOTSUPP) return -1;
    }
    return 0;
}
static char *text(int root,const char *name) {
    int fd=at(root,name,O_RDONLY|O_NONBLOCK);if (fd<0) return NULL;
    struct stat st;char *data=NULL;
    if (fstat(fd,&st)<0) goto done;
    if (!S_ISREG(st.st_mode) || st.st_uid || !allowed(st.st_gid) || st.st_nlink!=1
        || (st.st_mode&07022) || st.st_size<=0 || st.st_size>MAX_TEXT) { errno=EPERM;goto done; }
    data=malloc((size_t)st.st_size+1);if (!data) goto done;
    size_t read_bytes=0;
    while (read_bytes<(size_t)st.st_size) {
        ssize_t n=read(fd,data+read_bytes,(size_t)st.st_size-read_bytes);
        if (n<0&&errno==EINTR) continue;
        if (n<=0) { free(data);data=NULL;if (!n) errno=EIO;goto done; }read_bytes+=(size_t)n;
    }
    if (memchr(data,0,read_bytes)) { free(data);data=NULL;errno=EBADMSG;goto done; }
    data[read_bytes]=0;
done:;
    int saved=errno;close(fd);errno=saved;return data;
}
static int user_index(struct user *users,unsigned count,const char *name) {
    for (unsigned i=0;i<count;i++) if (!strcmp(users[i].name,name)) return (int)i;
    return -1;
}
static int group_index(struct group *groups,unsigned count,const char *name) {
    for (unsigned i=0;i<count;i++) if (!strcmp(groups[i].name,name)) return (int)i;
    return -1;
}
static int members_ok(const char *members,struct user *users,unsigned count) {
    if (!*members) return 1;
    const char *start=members;
    for (;;) {
        const char *end=strchr(start,',');size_t n=end ? (size_t)(end-start) : strlen(start);
        if (!n||n>127) return 0;
        char name[128];memcpy(name,start,n);name[n]=0;
        int index=user_index(users,count,name);
        // The generic personal runtime principal never gains supplementary
        // technical groups from mutable package account metadata.
        if (index<0 || users[index].uid==1000) return 0;
        if (!end) return 1;start=end+1;
    }
}
static char *trim(char *s) {
    while (*s==' '||*s=='\t') s++;
    size_t n=strlen(s);while (n && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r')) s[--n]=0;
    return s;
}
static int nss(char *data) {
    const char *keys[]={"passwd","group","shadow","gshadow","initgroups"};unsigned seen=0;
    char *cursor=data,*row;
    while ((row=line(&cursor))) {
        char *comment=strchr(row,'#');if (comment) *comment=0;row=trim(row);if (!*row) continue;
        char *colon=strchr(row,':');if (!colon) return -1;*colon=0;
        char *key=trim(row),*value=trim(colon+1);
        for (unsigned i=0;i<5;i++) if (!strcasecmp(key,keys[i])) {
            if ((seen&(1u<<i)) || strcmp(value,"files")) return -1;seen|=1u<<i;
        }
    }
    return (seen&15)==15 ? 0 : -1;
}
static int accounts(int root) {
    const char *names[]={"etc/passwd","etc/shadow","etc/group","etc/gshadow","etc/nsswitch.conf"};
    char *data[5]={0};struct user *users=calloc(MAX_ACCOUNTS,sizeof(*users));
    struct group *groups=calloc(MAX_ACCOUNTS,sizeof(*groups));int result=-1,saved=ENOMEM;
    if (!users||!groups) goto done;
    for (unsigned i=0;i<5;i++) if (!(data[i]=text(root,names[i]))) { saved=errno;goto done; }
    saved=EPERM;unsigned uc=0,gc=0;char *cursor=data[0],*row,*v[9];
    while ((row=line(&cursor))) {
        if (uc==MAX_ACCOUNTS || fields(row,v,7)<0 || !name_ok(v[0]) || strcmp(v[1],"x")) goto done;
        unsigned uid,gid;if (number(v[2],&uid)<0||number(v[3],&gid)<0||!allowed(uid)||!allowed(gid)) goto done;
        if ((!strcmp(v[0],"root"))!=(uid==0) || (!strcmp(v[0],"runtime"))!=(uid==1000)) goto done;
        if (uid==0 && gid!=0) goto done;
        if (uid==1000 && (gid!=1000 || strcmp(v[5],"/home/user") || strcmp(v[6],"/bin/bash"))) goto done;
        for (unsigned i=0;i<uc;i++) if (!strcmp(users[i].name,v[0])||users[i].uid==uid) goto done;
        users[uc++]=(struct user){v[0],uid,gid,0};
    }
    if (user_index(users,uc,"root")<0 || user_index(users,uc,"runtime")<0) goto done;
    cursor=data[1];
    while ((row=line(&cursor))) {
        if (fields(row,v,9)<0 || (v[1][0]!='!'&&v[1][0]!='*')) goto done;
        int i=user_index(users,uc,v[0]);if (i<0||users[i].shadow) goto done;users[i].shadow=1;
    }
    for (unsigned i=0;i<uc;i++) if (!users[i].shadow) goto done;
    cursor=data[2];
    while ((row=line(&cursor))) {
        if (gc==MAX_ACCOUNTS||fields(row,v,4)<0||!name_ok(v[0])||strcmp(v[1],"x")) goto done;
        unsigned gid;if (number(v[2],&gid)<0||!allowed(gid)) goto done;
        if ((!strcmp(v[0],"root"))!=(gid==0) || (!strcmp(v[0],"runtime"))!=(gid==1000)
            || !members_ok(v[3],users,uc)) goto done;
        for (unsigned i=0;i<gc;i++) if (!strcmp(groups[i].name,v[0])||groups[i].gid==gid) goto done;
        groups[gc++]=(struct group){v[0],v[3],gid,0};
    }
    for (unsigned i=0;i<uc;i++) {
        unsigned g;for (g=0;g<gc && groups[g].gid!=users[i].gid;g++);
        if (g==gc) goto done;
    }
    cursor=data[3];
    while ((row=line(&cursor))) {
        if (fields(row,v,4)<0||(v[1][0]!='!'&&v[1][0]!='*')||*v[2]) goto done;
        int i=group_index(groups,gc,v[0]);
        if (i<0||groups[i].shadow||strcmp(groups[i].members,v[3])) goto done;groups[i].shadow=1;
    }
    for (unsigned i=0;i<gc;i++) if (!groups[i].shadow) goto done;
    if (nss(data[4])<0) goto done;
    result=0;saved=0;
done:
    for (unsigned i=0;i<5;i++) free(data[i]);free(users);free(groups);errno=saved;return result;
}
static int tree(int root,dev_t device,unsigned depth,unsigned *count) {
    if (depth>128) return fail(E2BIG);
    int scan=at(root,".",O_RDONLY|O_DIRECTORY);if (scan<0) return -1;
    DIR *entries=fdopendir(scan);if (!entries) { int saved=errno;close(scan);return fail(saved); }
    int error=0;struct dirent *entry;
    for (;;) {
        errno=0;entry=readdir(entries);if (!entry) { error=errno;break; }
        if (!strcmp(entry->d_name,".")||!strcmp(entry->d_name,"..")) continue;
        if (++*count>MAX_ENTRIES) { error=E2BIG;break; }
        struct stat st;
        if (fstatat(root,entry->d_name,&st,AT_SYMLINK_NOFOLLOW)<0) { error=errno;break; }
        // These are the exact ephemeral mounts checked by namespace PID1.
        // No other filesystem crossing is accepted or followed.
        if (!depth && (!strcmp(entry->d_name,"dev")||!strcmp(entry->d_name,"proc")
            ||!strcmp(entry->d_name,"tmp")||!strcmp(entry->d_name,"run"))) continue;
        if (st.st_dev!=device || !allowed(st.st_uid)||!allowed(st.st_gid)
            || !(S_ISDIR(st.st_mode)||S_ISREG(st.st_mode)||S_ISLNK(st.st_mode))
            || (st.st_mode&(S_ISUID|S_ISGID))) { error=EPERM;break; }
        if (S_ISLNK(st.st_mode)) continue; // Never resolve a candidate link in the host.
        int fd=at(root,entry->d_name,O_RDONLY|O_NONBLOCK|(S_ISDIR(st.st_mode)?O_DIRECTORY:0));
        if (fd<0) { error=errno;break; }
        struct stat opened;
        if (fstat(fd,&opened)<0) error=errno;
        else if (opened.st_dev!=st.st_dev||opened.st_ino!=st.st_ino) error=ESTALE;
        else if (absent_xattrs(fd)<0) error=errno;
        else if (S_ISDIR(st.st_mode)&&tree(fd,device,depth+1,count)<0) error=errno;
        close(fd);if (error) break;
    }
    closedir(entries);return error ? fail(error) : 0;
}
int aegis_package_validate(int root) {
    struct stat st;if (fstat(root,&st)<0) return -1;
    if (getpid()!=1 || st.st_mode!=(S_IFDIR|0755)||st.st_uid||st.st_gid) return fail(EPERM);
    int status;errno=0;
    if (waitpid(-1,&status,WNOHANG)!=-1 || errno!=ECHILD) return fail(EBUSY);
    if (aegis_package_candidate_labels(root,1)<0 || accounts(root)<0 || absent_xattrs(root)<0) return -1;
    unsigned count=0;return tree(root,st.st_dev,0,&count);
}
