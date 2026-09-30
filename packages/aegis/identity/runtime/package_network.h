#ifndef AEGIS_PACKAGE_NETWORK_H
#define AEGIS_PACKAGE_NETWORK_H
#include <stdint.h>
#include "child.h"
#include "uid_layout.h"
#include "memory_group.h"
namespace aegis {
// Internal trusted planner companion. A private loopback listener belongs to
// the supplied gated planner netns; outbound sockets use Android's network.
// No policy/config/user/CE FD reaches this helper. Parent owns partial pidfd on
// failure; cancellation must kill the shared cgroup, reap both children, then
// remove it. This API never grants network to installed package programs.
int PackageNetworkStart(int netns,int executable,aegis_memory_group* group,
                        uint32_t user,uint32_t serial,uint64_t job,int timeout,
                        aegis_child** child);
namespace network {
constexpr uint32_t kMagic=0x41474e31;
constexpr unsigned kPort=1080,kAppId=AEGIS_PACKAGE_NETWORK_APP_ID;
struct Request { uint32_t magic,user,serial,reserved;uint64_t job; };
struct Ready { uint32_t magic,error; };
// Host-order IPv4. Reject special-use/local addresses, even after allowed DNS.
bool PublicAddress(uint32_t address);
bool Destination(const char* name,unsigned port);
}
}
#endif
