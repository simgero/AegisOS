#ifndef AEGIS_PACKAGE_PLANNING_PROTOCOL_H
#define AEGIS_PACKAGE_PLANNING_PROTOCOL_H
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define AEGIS_PLANNING_MAGIC UINT32_C(0x41504c4e)
#define AEGIS_PLANNING_VERSION 1u
struct aegis_planning_request {
    uint32_t magic,version,user,serial;
    uint64_t job;
    uint32_t action,reserved;
    char package[129],version_text[129];
    uint8_t padding[6];
};
struct aegis_planning_reply {
    uint32_t magic,version,user,serial;
    uint64_t job;
    uint32_t phase,status,error,effects;
};
// Private SEQPACKET only. Reject/close all unexpected ancillary descriptors,
// including malformed/truncated messages; no descriptor can escape on error.
static inline int aegis_planning_receive(int channel,void* payload,size_t bytes,int* fds,size_t expected) {
    alignas(cmsghdr) char controls[CMSG_SPACE(16*sizeof(int))]={};
    iovec io={payload,bytes};msghdr m={};m.msg_iov=&io;m.msg_iovlen=1;
    m.msg_control=controls;m.msg_controllen=sizeof(controls);
    ssize_t n=recvmsg(channel,&m,MSG_CMSG_CLOEXEC|MSG_TRUNC|MSG_DONTWAIT);
    if(n<0)return -1;
    size_t count=0;bool bad=false;
    for(cmsghdr* c=CMSG_FIRSTHDR(&m);c;c=CMSG_NXTHDR(&m,c)) {
        auto* end=static_cast<unsigned char*>(m.msg_control)+m.msg_controllen;
        if(c->cmsg_len<CMSG_LEN(0)||c->cmsg_len>size_t(end-reinterpret_cast<unsigned char*>(c))) { bad=true;break; }
        if(c->cmsg_level!=SOL_SOCKET||c->cmsg_type!=SCM_RIGHTS) { bad=true;continue; }
        size_t size=c->cmsg_len-CMSG_LEN(0);if(size%sizeof(int))bad=true;
        for(size_t at=0;at<size/sizeof(int);++at) {
            int fd;memcpy(&fd,CMSG_DATA(c)+at*sizeof(fd),sizeof(fd));
            if(count<expected)fds[count++]=fd;else { close(fd);bad=true; }
        }
    }
    if(n!=ssize_t(bytes)||bad||count!=expected||(m.msg_flags&(MSG_TRUNC|MSG_CTRUNC))) {
        for(size_t i=0;i<count;++i) { close(fds[i]);fds[i]=-1; }errno=EPROTO;return -1;
    }
    return 0;
}
static inline int aegis_planning_send(int channel,const void* payload,size_t bytes,const int* fds,size_t count) {
    if(count>5) { errno=EINVAL;return -1; }
    alignas(cmsghdr) char controls[CMSG_SPACE(5*sizeof(int))]={};
    iovec io={const_cast<void*>(payload),bytes};msghdr m={};m.msg_iov=&io;m.msg_iovlen=1;
    if(count) {
        m.msg_control=controls;m.msg_controllen=CMSG_SPACE(count*sizeof(int));
        auto* c=CMSG_FIRSTHDR(&m);c->cmsg_level=SOL_SOCKET;c->cmsg_type=SCM_RIGHTS;c->cmsg_len=CMSG_LEN(count*sizeof(int));
        memcpy(CMSG_DATA(c),fds,count*sizeof(int));
    }
    ssize_t n=sendmsg(channel,&m,MSG_NOSIGNAL|MSG_DONTWAIT);
    if(n==ssize_t(bytes))return 0;if(n>=0)errno=EIO;return -1;
}
#endif
