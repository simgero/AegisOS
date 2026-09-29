#ifndef AEGIS_PACKAGE_PUBLISH_PROTOCOL_H
#define AEGIS_PACKAGE_PUBLISH_PROTOCOL_H
// Fixed private exec contract. Never accepted over the public broker socket.
#include "package_publisher.h"
#include <cstring>
#include <climits>
namespace aegis::publication {
constexpr uint32_t kMagic = 0x41455050, kVersion = 1;
constexpr uint32_t kPersonal = 1, kCreate = 2, kPrevious = 4;
constexpr int kStore = 3, kSource = 4, kReply = 5, kRequest = 6, kExecutable = 7;
struct Image { uint64_t bytes; char sha256[65], base[65]; };
struct Request {
    uint32_t magic, version, user, serial, flags, reserved;
    uint64_t job;
    char plan[65];
    Image previous, candidate;
};
struct Reply { uint64_t job; int32_t result, error; char plan[65]; };
inline bool Hash(const char text[65]) {
    if (text[64]) return false;
    for (unsigned i=0;i<64;++i)
        if (!((text[i]>='0'&&text[i]<='9')||(text[i]>='a'&&text[i]<='f'))) return false;
    return true;
}
inline bool Empty(const char text[65]) {
    for (unsigned i=0;i<65;++i) if (text[i]) return false;
    return true;
}
inline bool ValidImage(const Image& value, bool personal) {
    return value.bytes && value.bytes <= (uint64_t{32}<<30) && Hash(value.sha256)
            && (personal ? Hash(value.base) : Empty(value.base));
}
inline bool Valid(const Request& value) {
    bool personal = value.flags & kPersonal;
    return value.magic==kMagic && value.version==kVersion && !value.reserved
        && !(value.flags&~(kPersonal|kCreate|kPrevious))
        && value.user>=10 && value.user<21473 && value.serial<=INT32_MAX
        && value.job && value.job<=INT64_MAX && Hash(value.plan)
        && ValidImage(value.candidate,personal)
        && ((value.flags&kPrevious) ? ValidImage(value.previous,personal)
                                   : value.previous.bytes==0 && Empty(value.previous.sha256) && Empty(value.previous.base))
        && !((value.flags&kCreate)&&(value.flags&kPrevious));
}
inline PackageGeneration Generation(const Image& value) {
    return {value.sha256,value.base,value.bytes};
}
} // namespace aegis::publication
#endif
