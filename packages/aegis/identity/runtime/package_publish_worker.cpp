// Only trusted validation/copy/publication runs here. Never execute package
// contents or maintainer scripts. Production installation/policy is not wired.
#include "package_publish_protocol.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/prctl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <memory>
using namespace aegis;
namespace wire = aegis::publication;

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
        std::unique_ptr<PackageStore> store(PackageStore::Open(wire::kStore,owner,
                                                             request.flags&wire::kCreate));
        if (!store) reply.error=errno;
        else {
            auto previous=wire::Generation(request.previous);
            auto candidate=wire::Generation(request.candidate);
            const std::atomic_bool proceed{false};
            auto result=store->Publish(request.flags&wire::kPrevious ? &previous : nullptr,
                                       wire::kSource,candidate,proceed);
            reply.result=static_cast<int32_t>(result);
            reply.error=result==PackagePublish::Confirmed ? 0 : errno;
        }
    } // Release store lock and ALL store/image refs before publishing a response.
    close(wire::kSource); close(wire::kStore);
    ssize_t sent;
    do { sent=send(wire::kReply,&reply,sizeof(reply),MSG_NOSIGNAL); } while(sent<0&&errno==EINTR);
    close(wire::kReply);
    return sent==static_cast<ssize_t>(sizeof(reply)) ? 0 : 121;
}
