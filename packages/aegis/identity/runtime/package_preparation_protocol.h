#ifndef AEGIS_PACKAGE_PREPARATION_PROTOCOL_H
#define AEGIS_PACKAGE_PREPARATION_PROTOCOL_H
#include "package_preparer.h"
#include "package_execution_protocol.h"
namespace aegis::preparation {
constexpr uint32_t kMagic=0x41455052,kVersion=5;
constexpr int kStage=3,kSource=4,kReply=5,kRequest=6,kExecutable=7,kArchive=8;
struct Input { uint64_t bytes; char hash[65]; };
struct Request {
    uint32_t magic,version,selection,has_shared,has_personal;
    aegis_package_execution_request execution;
    Input image,archives[AEGIS_PACKAGE_EXEC_ITEMS];
};
struct Reply {
    uint32_t magic,version,user,serial; uint64_t job; int32_t error; char plan[65];
    uint32_t scope; Input selected; char shared_base[65]; Input shared;
};
inline bool InputValid(const Input& in,uint64_t max) {
    return in.bytes && in.bytes<=max && aegis_package_hash(in.hash);
}
inline bool Valid(const Request& r) {
    if(r.magic!=kMagic || r.version!=kVersion || r.selection>1 || r.has_shared>1 || r.has_personal>1
       || !InputValid(r.image,uint64_t{32}<<30) || r.image.bytes%4096)return false;
    if(r.selection) {
        const auto& e=r.execution;
        if(e.magic!=AEGIS_PACKAGE_EXEC_MAGIC || e.version!=AEGIS_PACKAGE_EXEC_VERSION
           || e.user<10 || e.user>=21473 || e.serial>INT32_MAX || !e.job || e.job>INT64_MAX
           || e.kind || e.count || !aegis_package_zero(e.reserved,sizeof(e.reserved))
           || memcmp(e.plan,r.image.hash,65)
           || !aegis_package_zero(e.items,sizeof(e.items))
           || !aegis_package_zero(&e.review,sizeof(e.review)))return false;
        for(const auto& a:r.archives)if(a.bytes || !aegis_package_zero(a.hash,65))return false;
        return true;
    }
    if(r.has_shared || r.has_personal || !aegis_package_execution_valid(&r.execution))return false;
    uint64_t total=0;
    for(unsigned i=0;i<AEGIS_PACKAGE_EXEC_ITEMS;++i) {
        if(aegis_package_has_archive(&r.execution,i)) {
            if(!InputValid(r.archives[i],uint64_t{2}<<30))return false;
            total+=r.archives[i].bytes;
        } else if(r.archives[i].bytes || !aegis_package_zero(r.archives[i].hash,65))return false;
    }
    return total<=(uint64_t{8}<<30);
}
}
#endif
