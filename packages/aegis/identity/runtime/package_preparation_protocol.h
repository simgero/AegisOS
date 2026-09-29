#ifndef AEGIS_PACKAGE_PREPARATION_PROTOCOL_H
#define AEGIS_PACKAGE_PREPARATION_PROTOCOL_H
#include "package_preparer.h"
#include "package_execution_protocol.h"
namespace aegis::preparation {
constexpr uint32_t kMagic=0x41455052,kVersion=1;
constexpr int kStage=3,kSource=4,kReply=5,kRequest=6,kExecutable=7,kArchive=8;
struct Input { uint64_t bytes; char hash[65]; };
struct Request {
    uint32_t magic,version;
    aegis_package_execution_request execution;
    Input image,archives[AEGIS_PACKAGE_EXEC_ITEMS];
};
struct Reply { uint32_t magic,version,user,serial; uint64_t job; int32_t error; char plan[65]; };
inline bool InputValid(const Input& in,uint64_t max) {
    return in.bytes && in.bytes<=max && aegis_package_hash(in.hash);
}
inline bool Valid(const Request& r) {
    if(r.magic!=kMagic || r.version!=kVersion || !aegis_package_execution_valid(&r.execution)
       || !InputValid(r.image,uint64_t{32}<<30) || r.image.bytes%4096)return false;
    uint64_t total=0;
    for(unsigned i=0;i<AEGIS_PACKAGE_EXEC_ITEMS;++i) {
        if(r.execution.kind==AEGIS_PACKAGE_ARCHIVES && i<r.execution.count) {
            if(!InputValid(r.archives[i],uint64_t{2}<<30))return false;
            total+=r.archives[i].bytes;
        } else if(r.archives[i].bytes || !aegis_package_zero(r.archives[i].hash,65))return false;
    }
    return total<=(uint64_t{8}<<30);
}
}
#endif
