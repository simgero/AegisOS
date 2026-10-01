#ifndef AEGIS_PACKAGE_PLANNING_PROTOCOL_H
#define AEGIS_PACKAGE_PLANNING_PROTOCOL_H
#include "package_execution_protocol.h"
#include "package_plan.h"
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#define AEGIS_PLANNING_MAGIC UINT32_C(0x41504c4e)
#define AEGIS_PLANNING_VERSION 7u
#define AEGIS_PLANNING_INPUT_FDS 8u
struct aegis_planning_request {
    uint32_t magic,version,user,serial;
    uint64_t job;
    uint32_t action,internet,reconciliation;
    char package[129],version_text[129];
    uint8_t padding[10];
};
struct aegis_planning_repository {
    char id[129],release[65],index[65];
    uint64_t valid_until;
};
struct aegis_planning_change {
    aegis_package_expected_effect effect;
    char repository[129],hash[65];
    uint64_t bytes;
};
struct aegis_planning_evidence {
    uint32_t repositories,changes,apt_presence,reconciliation;
    uint64_t apt_bytes;
    char policy[65],status[65],apt_hash[65];
    char private_choices[AEGIS_PACKAGE_CHOICES_BYTES];
    aegis_planning_repository sources[16];
    aegis_planning_change effects[AEGIS_PACKAGE_EXEC_ITEMS];
};
struct aegis_planning_reply {
    uint32_t magic,version,user,serial;
    uint64_t job;
    uint32_t phase,status,error,effects;
    aegis_planning_evidence evidence;
};
namespace aegis::planning_wire {
template<size_t N> inline bool Text(char (&to)[N],const std::string& from) {
    if(from.size()>=N || from.find('\0')!=std::string::npos)return false;
    memcpy(to,from.data(),from.size());return true;
}
inline int Encode(const PackageResolvedPlan& p,aegis_planning_evidence* output) {
    aegis_planning_evidence e={};
    if(p.repositories.size()>16 || p.changes.size()>AEGIS_PACKAGE_EXEC_ITEMS) { errno=E2BIG;return -1; }
    e.reconciliation=p.reconciliation;e.repositories=p.repositories.size();e.changes=p.changes.size();
    if(!Text(e.private_choices,p.initial_private_choices))return errno=EINVAL,-1;
    e.apt_presence=uint32_t(p.initial_apt_state_presence);e.apt_bytes=p.initial_apt_state.bytes;
    if(!Text(e.policy,p.policy_sha256)||!Text(e.status,p.initial_status_sha256)||!Text(e.apt_hash,p.initial_apt_state.sha256))return errno=EINVAL,-1;
    for(size_t i=0;i<p.repositories.size();++i) {
        auto& r=e.sources[i];const auto& source=p.repositories[i];r.valid_until=source.valid_until_unix;
        if(!Text(r.id,source.id)||!Text(r.release,source.release_sha256)||!Text(r.index,source.index_sha256))return errno=EINVAL,-1;
    }
    for(size_t i=0;i<p.changes.size();++i) {
        auto& c=e.effects[i];const auto& change=p.changes[i];c.bytes=change.archive.bytes;c.effect.reason=uint8_t(change.reason);
        if(!Text(c.effect.name,change.name)||!Text(c.effect.architecture,change.architecture)
           ||!Text(c.effect.before,change.before_version)||!Text(c.effect.after,change.after_version)
           ||!Text(c.repository,change.repository)||!Text(c.hash,change.archive.sha256))return errno=EINVAL,-1;
    }
    *output=e;return 0;
}
template<size_t N> inline bool Fixed(const char (&s)[N]) { return aegis_package_fixed(s,N,1); }
inline int Decode(const aegis_planning_evidence& e,PackageResolvedPlan* output) {
    if(e.repositories>16||e.changes>AEGIS_PACKAGE_EXEC_ITEMS||!Fixed(e.policy)||!Fixed(e.status)||!Fixed(e.apt_hash))return errno=EPROTO,-1;
    if(!aegis_package_choices_text(e.private_choices))return errno=EPROTO,-1;
    if(e.reconciliation>1)return errno=EPROTO,-1;
    PackageResolvedPlan p;p.reconciliation=e.reconciliation;p.initial_private_choices=e.private_choices;p.policy_sha256=e.policy;p.initial_status_sha256=e.status;
    p.initial_apt_state_presence=static_cast<PackageStatePresence>(e.apt_presence);p.initial_apt_state={e.apt_bytes,e.apt_hash};
    for(unsigned i=0;i<e.repositories;++i) {
        const auto& r=e.sources[i];if(!Fixed(r.id)||!Fixed(r.release)||!Fixed(r.index))return errno=EPROTO,-1;
        p.repositories.push_back({r.id,r.release,r.index,r.valid_until});
    }
    for(unsigned i=0;i<e.changes;++i) {
        const auto& c=e.effects[i];const auto& f=c.effect;
        if(!Fixed(f.name)||!Fixed(f.architecture)||!Fixed(f.before)||!Fixed(f.after)||!Fixed(c.repository)||!Fixed(c.hash))return errno=EPROTO,-1;
        p.changes.push_back({f.name,f.architecture,f.before,f.after,c.repository,{c.bytes,c.hash},static_cast<PackageInstallReason>(f.reason)});
    }
    aegis_planning_evidence canonical={};
    if(Encode(p,&canonical)<0||memcmp(&canonical,&e,sizeof(e)))return errno=EPROTO,-1;
    *output=std::move(p);return 0;
}
}
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
    if(count>AEGIS_PLANNING_INPUT_FDS) { errno=EINVAL;return -1; }
    alignas(cmsghdr) char controls[CMSG_SPACE(AEGIS_PLANNING_INPUT_FDS*sizeof(int))]={};
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
