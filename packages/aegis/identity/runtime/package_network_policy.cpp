#include "package_network.h"
#include <cstring>
namespace aegis::network {
bool Destination(const char* host,unsigned port) {
    return host && (port==80||port==443)
        && (!strcmp(host,"deb.debian.org")||!strcmp(host,"security.debian.org"));
}
bool PublicAddress(uint32_t n) {
    unsigned a=n>>24,b=(n>>16)&255,c=(n>>8)&255;
    return a!=0 && a!=10 && a!=127 && a<224
        && !(a==100&&b>=64&&b<=127) && !(a==169&&b==254)
        && !(a==172&&b>=16&&b<=31) && !(a==192&&(b==168||b==0||(b==88&&c==99)))
        && !(a==198&&(b==18||b==19||(b==51&&c==100)))
        && !(a==203&&b==0&&c==113);
}
}
