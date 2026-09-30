// Deliberately small SOCKS5 domain-only CONNECT relay; fixed product hosts.
#include "package_network.h"
#include "sandbox.h"
#include <android-base/unique_fd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <linux/capability.h>
#include <linux/nsfs.h>
#include <net/if.h>
#include <netdb.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cstring>
using android::base::unique_fd;
namespace net=aegis::network;
namespace {
int Fail(int e) { errno=e;return -1; }
int Wait(int fd,short events,int ms=30000) {
    pollfd p={fd,events,0};int n=poll(&p,1,ms);
    return n>0&&(p.revents&events)?0:Fail(n==0?ETIMEDOUT:EIO);
}
bool Receive(int fd,void* output,size_t size) {
    char* p=static_cast<char*>(output);
    while(size) {
        if(Wait(fd,POLLIN,10000)<0)return false;ssize_t n=recv(fd,p,size,0);
        if(n<0&&(errno==EINTR||errno==EAGAIN))continue;if(n<=0)return false;size-=n;p+=n;
    }return true;
}
bool Send(int fd,const void* input,size_t size) {
    const char* p=static_cast<const char*>(input);
    while(size) {
        if(Wait(fd,POLLOUT,10000)<0)return false;ssize_t n=send(fd,p,size,MSG_NOSIGNAL);
        if(n<0&&(errno==EINTR||errno==EAGAIN))continue;if(n<=0)return false;size-=n;p+=n;
    }return true;
}
void Reply(int fd,uint8_t code) { uint8_t r[]={5,code,0,1,0,0,0,0,0,0};(void)Send(fd,r,sizeof(r)); }
int Connect(const char* host,unsigned port) {
    addrinfo hints={},*list=nullptr;hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;hints.ai_protocol=IPPROTO_TCP;
    if(getaddrinfo(host,nullptr,&hints,&list)!=0)return Fail(EHOSTUNREACH);
    unique_fd result;unsigned attempts=0;
    for(auto* row=list;row&&attempts<8;row=row->ai_next) {
        if(row->ai_family!=AF_INET||row->ai_addrlen!=sizeof(sockaddr_in))continue;
        sockaddr_in address=*reinterpret_cast<sockaddr_in*>(row->ai_addr);
        if(!net::PublicAddress(ntohl(address.sin_addr.s_addr)))continue;++attempts;
        address.sin_port=htons(port);unique_fd fd(socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0));if(!fd.ok())break;
        int r=connect(fd.get(),reinterpret_cast<sockaddr*>(&address),sizeof(address));
        if(r<0&&(errno!=EINPROGRESS||Wait(fd.get(),POLLOUT,10000)<0))continue;
        int error=0;socklen_t length=sizeof(error);
        if(getsockopt(fd.get(),SOL_SOCKET,SO_ERROR,&error,&length)<0||error)continue;
        result=std::move(fd);break;
    }
    freeaddrinfo(list);return result.ok()?result.release():Fail(EHOSTUNREACH);
}
void Relay(int a,int b) {
    struct Pipe { std::array<char,32768> bytes;size_t size=0,at=0;bool eof=false,shutdown=false; } p[2];
    const int fds[]={a,b};uint64_t transferred=0;
    // Absolute lifetime also bounds slow DNS and trickle traffic. Cancellation
    // is independent: owner kills the shared cgroup including all relay children.
    alarm(900);
    for(;;) {
        pollfd events[]={{a,0,0},{b,0,0}};
        for(unsigned i=0;i<2;++i) {
            if(!p[i].eof&&p[i].size==0)events[i].events|=POLLIN;
            if(p[i].size)events[1-i].events|=POLLOUT;
            if(p[i].eof&&!p[i].size&&!p[i].shutdown) { shutdown(fds[1-i],SHUT_WR);p[i].shutdown=true; }
        }
        if(p[0].shutdown&&p[1].shutdown)return;
        int n=poll(events,2,30000);if(n<0&&errno==EINTR)continue;if(n<=0)return;
        for(unsigned i=0;i<2;++i) {
            if(events[i].revents&(POLLERR|POLLNVAL))return;
            if(!p[i].eof&&!p[i].size&&(events[i].revents&(POLLIN|POLLHUP))) {
                ssize_t count=recv(fds[i],p[i].bytes.data(),p[i].bytes.size(),0);
                if(count>0) { p[i].size=count;p[i].at=0;transferred+=count;if(transferred>(uint64_t{1}<<30))return; }
                else if(count==0)p[i].eof=true;else if(errno!=EAGAIN&&errno!=EINTR)return;
            }
            if(p[i].size&&(events[1-i].revents&POLLOUT)) {
                ssize_t count=send(fds[1-i],p[i].bytes.data()+p[i].at,p[i].size,MSG_NOSIGNAL);
                if(count>0) { p[i].at+=count;p[i].size-=count; }
                else if(count<0&&errno!=EAGAIN&&errno!=EINTR)return;
            }
        }
    }
}
void Connection(int fd) {
    alarm(45);uint8_t hello[2],methods[255];
    if(!Receive(fd,hello,2)||hello[0]!=5||!hello[1]||!Receive(fd,methods,hello[1]))return;
    bool none=false;for(unsigned i=0;i<hello[1];++i)if(methods[i]==0)none=true;
    uint8_t chosen[]={5,uint8_t(none?0:255)};if(!Send(fd,chosen,2)||!none)return;
    uint8_t request[5];if(!Receive(fd,request,4))return;
    if(request[0]!=5||request[1]!=1||request[2]||request[3]!=3) { Reply(fd,2);return; }
    if(!Receive(fd,request+4,1)||!request[4]) { Reply(fd,2);return; }
    char host[256]={};uint8_t port[2];
    if(!Receive(fd,host,request[4])||!Receive(fd,port,2))return;
    unsigned destination=(unsigned(port[0])<<8)|port[1];
    if(memchr(host,0,request[4])||!net::Destination(host,destination)) { Reply(fd,2);return; }
    unique_fd upstream(Connect(host,destination));if(!upstream.ok()) { Reply(fd,4);return; }
    Reply(fd,0);Relay(fd,upstream.get());
}
int Setup() {
    net::Request request={};struct stat st;int seals=fcntl(4,F_GET_SEALS);
    if(getuid()||geteuid()||getgid()||getegid()||getgroups(0,nullptr)!=0)return Fail(EPERM);
    if(seals!=(F_SEAL_WRITE|F_SEAL_GROW|F_SEAL_SHRINK|F_SEAL_SEAL)||fstat(4,&st)<0||st.st_size!=static_cast<ssize_t>(sizeof(request))
       ||pread(4,&request,sizeof(request),0)!=static_cast<ssize_t>(sizeof(request))||request.magic!=net::kMagic||request.reserved
       ||request.user<10||request.user>=21473||request.serial>INT32_MAX||!request.job||request.job>INT64_MAX)return Fail(EPROTO);
    int type=0;socklen_t length=sizeof(type);
    if(getsockopt(5,SOL_SOCKET,SO_TYPE,&type,&length)<0||type!=SOCK_SEQPACKET||ioctl(3,NS_GET_NSTYPE)!=CLONE_NEWNET)return Fail(EPERM);
    unique_fd host(open("/proc/self/ns/net",O_RDONLY|O_CLOEXEC));if(!host.ok())return -1;
    struct stat target,original;if(fstat(3,&target)<0||fstat(host.get(),&original)<0)return -1;
    if(target.st_ino==original.st_ino&&target.st_dev==original.st_dev)return Fail(EPERM);
    if(setns(3,CLONE_NEWNET)<0)return -1;
    unique_fd listener(socket(AF_INET,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0));if(!listener.ok())return -1;
    ifreq lo={};strcpy(lo.ifr_name,"lo");
    if(ioctl(listener.get(),SIOCGIFFLAGS,&lo)<0)return -1;lo.ifr_flags|=IFF_UP;
    if(ioctl(listener.get(),SIOCSIFFLAGS,&lo)<0)return -1;
    sockaddr_in address={};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(net::kPort);
    if(bind(listener.get(),reinterpret_cast<sockaddr*>(&address),sizeof(address))<0||listen(listener.get(),8)<0
       ||setns(host.get(),CLONE_NEWNET)<0)return -1;
    host.reset();close(3);close(4);
    // Only the listening socket survives; its netns stays private. Future
    // outbound sockets are in the helper's original Android network namespace.
    if(listener.get()!=0) { if(dup3(listener.get(),0,O_CLOEXEC)<0)return -1;listener.reset(); }else listener.release();
    for(unsigned cap=0;cap<=CAP_LAST_CAP;++cap)if(prctl(PR_CAPBSET_DROP,cap,0,0,0)<0)return -1;
    gid_t inet=3003;uid_t uid=request.user*100000+net::kAppId;
    if(setgroups(1,&inet)<0||setresgid(uid,uid,uid)<0||setresuid(uid,uid,uid)<0)return -1;
    __user_cap_header_struct header={_LINUX_CAPABILITY_VERSION_3,0};__user_cap_data_struct caps[2]={};
    if(syscall(SYS_capset,&header,caps)<0||prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)<0
       ||prctl(PR_SET_PDEATHSIG,SIGKILL,0,0,0)<0||prctl(PR_SET_DUMPABLE,0,0,0,0)<0)return -1;
    pollfd parent={6,POLLIN,0};if(poll(&parent,1,0)!=0)return Fail(ECHILD);
    if(aegis_install_filter()<0)return -1;
    net::Ready ready={net::kMagic,0};if(send(5,&ready,sizeof(ready),MSG_NOSIGNAL)!=static_cast<ssize_t>(sizeof(ready)))return -1;
    return syscall(SYS_close_range,1u,~0u,0u);
}
}
int main(int argc,char**) {
    if(argc!=1)return 110;
    if(Setup()<0) { net::Ready r={net::kMagic,unsigned(errno)};(void)send(5,&r,sizeof(r),MSG_NOSIGNAL);return 111; }
    std::array<pid_t,8> children={};
    for(;;) {
        for(auto& pid:children)if(pid) { int status;pid_t r=waitpid(pid,&status,WNOHANG);if(r==pid)pid=0;else if(r<0&&errno!=EINTR)return 112; }
        auto free=std::find(children.begin(),children.end(),0);
        pollfd ready={0,short(free==children.end()?0:POLLIN),0};int n=poll(&ready,1,100);
        if(n<0&&errno==EINTR)continue;if(n<0||ready.revents&(POLLERR|POLLNVAL|POLLHUP))return 113;
        if(!(ready.revents&POLLIN))continue;
        unique_fd socket(accept4(0,nullptr,nullptr,SOCK_CLOEXEC|SOCK_NONBLOCK));if(!socket.ok()) { if(errno==EAGAIN||errno==EINTR)continue;return 114; }
        pid_t parent=getpid(),pid=fork();if(pid<0)continue;
        if(pid==0) {
            close(0);if(prctl(PR_SET_PDEATHSIG,SIGKILL,0,0,0)<0||getppid()!=parent)_exit(115);
            Connection(socket.get());_exit(0);
        }*free=pid;
    }
}
