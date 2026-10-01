// Only trusted validation/copy/publication runs here. Never execute package
// contents or maintainer scripts. Production installation/policy is not wired.
#include "package_publish_protocol.h"
#include <android-base/unique_fd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <memory>
#include <array>
#include <cstdio>
#include <algorithm>
#include <openssl/sha.h>
using namespace aegis;
namespace wire = aegis::publication;

// Filesystem layout: docs.kernel.org/filesystems/ext4/super.html. This is a
// clean-unmount guard, not fsck or semantic validation. Those are separate from
// the digest; only the registered, successfully checked executor can reach here.
static int DigestSource(int fd,PackageGeneration* generation) {
    struct stat before,after;
    if(fstat(fd,&before)<0)return -1;
    if(before.st_mode!=(S_IFREG|0600) || before.st_uid || before.st_gid || before.st_nlink!=1
       || before.st_size<4096 || static_cast<uint64_t>(before.st_size)!=generation->bytes) { errno=EPERM;return -1; }
    std::array<unsigned char,1024> sb;
    if(pread(fd,sb.data(),sb.size(),1024)!=static_cast<ssize_t>(sb.size())) { errno=EIO;return -1; }
    auto le32=[&](size_t at) { return uint32_t{sb[at]}|(uint32_t{sb[at+1]}<<8)
        |(uint32_t{sb[at+2]}<<16)|(uint32_t{sb[at+3]}<<24); };
    uint64_t blocks=le32(4)|(uint64_t{le32(0x150)}<<32);
    if(sb[0x38]!=0x53 || sb[0x39]!=0xef || sb[0x3a]!=1 || sb[0x3b]
       || (le32(0x60)&4) || le32(0x18)!=2 || blocks!=generation->bytes/4096) { errno=EBUSY;return -1; }
    SHA256_CTX hash;if(!SHA256_Init(&hash)) { errno=EIO;return -1; }
    std::array<unsigned char,128*1024> buffer;
    for(uint64_t at=0;at<generation->bytes;) {
        ssize_t n=pread(fd,buffer.data(),std::min<uint64_t>(buffer.size(),generation->bytes-at),at);
        if(n<0 && errno==EINTR)continue;
        if(n<=0 || !SHA256_Update(&hash,buffer.data(),n)) { errno=EIO;return -1; }at+=n;
    }
    unsigned char digest[32];char hex[65];
    if(!SHA256_Final(digest,&hash) || fstat(fd,&after)<0) { errno=EIO;return -1; }
    if(before.st_dev!=after.st_dev || before.st_ino!=after.st_ino || before.st_size!=after.st_size
       || before.st_mtim.tv_sec!=after.st_mtim.tv_sec || before.st_mtim.tv_nsec!=after.st_mtim.tv_nsec
       || before.st_ctim.tv_sec!=after.st_ctim.tv_sec || before.st_ctim.tv_nsec!=after.st_ctim.tv_nsec) { errno=ESTALE;return -1; }
    for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",digest[i]);generation->image_sha256=hex;
    return 0;
}
int main(int argc, char**) {
    uid_t r,e,s; gid_t gr,ge,gs;
    struct stat st = {};
    wire::Request request = {};
    constexpr int seals = F_SEAL_WRITE|F_SEAL_GROW|F_SEAL_SHRINK|F_SEAL_SEAL;
    int death = 0, socket_type = 0; socklen_t socket_length = sizeof(socket_type);
    if (argc!=1 || getresuid(&r,&e,&s)<0 || getresgid(&gr,&ge,&gs)<0
            || r||e||s||gr||ge||gs || getgroups(0,nullptr)!=0
            || prctl(PR_GET_PDEATHSIG,&death)<0 || death!=SIGKILL
            || prctl(PR_SET_DUMPABLE,0)<0
            || fstat(wire::kRequest,&st)<0 || !S_ISREG(st.st_mode)
            || st.st_uid || st.st_gid || st.st_size!=static_cast<off_t>(sizeof(request))
            || fcntl(wire::kRequest,F_GET_SEALS)!=seals
            || pread(wire::kRequest,&request,sizeof(request),0)!=static_cast<ssize_t>(sizeof(request))
            || !wire::Valid(request)
            || getsockopt(wire::kReply,SOL_SOCKET,SO_TYPE,&socket_type,&socket_length)<0
            || socket_type!=SOCK_SEQPACKET) return 120;
    close(wire::kRequest);
    wire::Reply reply = {};
    reply.job=request.job; memcpy(reply.plan,request.plan,sizeof(reply.plan));
    reply.result=static_cast<int32_t>(PackagePublish::Rejected);
    {
        PackageOwner owner{bool(request.flags&wire::kPersonal),0,0};
        if (owner.personal) { owner.user_id=request.user;owner.serial=request.serial; }
        auto candidate=wire::Generation(request.candidate);
        std::unique_ptr<PackageStore> shared,store;
        if((request.flags&wire::kDerive) && DigestSource(wire::kSource,&candidate)<0)reply.error=errno;
        else {
            // Always common before private: a concurrent common publisher can
            // neither change the reviewed generation nor deadlock on our private
            // store. All hashing and lock acquisition stay in this owned worker.
            if(request.flags&wire::kFenceShared) {
                shared.reset(PackageStore::Open(wire::kSharedStore,{false,0,0},false));
                if(!shared)reply.error=errno;
                else {
                    PackageGeneration actual;
                    android::base::unique_fd image(shared->Current(&actual));
                    if(!image.ok())reply.error=errno;
                    else if(actual.image_sha256!=request.expected_shared.sha256
                         || actual.bytes!=request.expected_shared.bytes
                         || !actual.shared_base_sha256.empty())reply.error=ESTALE;
                }
            }
            if(!reply.error) {
                store.reset(PackageStore::Open(wire::kStore,owner,request.flags&wire::kCreate));
                if(!store)reply.error=errno;
            }
        }
        if (store) {
            auto previous=wire::Generation(request.previous);
            const std::atomic_bool proceed{false};
            auto result=store->Publish(request.flags&wire::kPrevious ? &previous : nullptr,
                                       wire::kSource,candidate,proceed);
            reply.result=static_cast<int32_t>(result);
            reply.error=result==PackagePublish::Confirmed ? 0 : errno;
            if(result==PackagePublish::Confirmed) {
                reply.candidate.bytes=candidate.bytes;
                memcpy(reply.candidate.sha256,candidate.image_sha256.c_str(),65);
                memcpy(reply.candidate.base,candidate.shared_base_sha256.c_str(),candidate.shared_base_sha256.size()+1);
            }
        }
    } // Release store lock and ALL store/image refs before publishing a response.
    close(wire::kSource); close(wire::kStore); close(wire::kSharedStore);
    ssize_t sent;
    do { sent=send(wire::kReply,&reply,sizeof(reply),MSG_NOSIGNAL); } while(sent<0&&errno==EINTR);
    close(wire::kReply);
    return sent==static_cast<ssize_t>(sizeof(reply)) ? 0 : 121;
}
