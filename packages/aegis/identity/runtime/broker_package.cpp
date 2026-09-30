#include "broker_owner_package.h"
#include <json/json.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

namespace {
int fail(int error) { errno=error;return -1; }
Json::Value intent(const aegis::PackageIntent& value,const std::string& digest,uint64_t expires) {
    Json::Value result(Json::objectValue);
    result["action"]=static_cast<unsigned>(value.action);
    result["scope"]=value.personal?1:2;
    result["package"]=value.package;result["version"]=value.version;
    result["digest"]=digest;result["validUntil"]=Json::UInt64(expires);
    return result;
}
int encode(const Json::Value& value,unsigned kind,aegis_broker_package_reply* output) {
    Json::StreamWriterBuilder writer;writer["indentation"]="";writer["emitUTF8"]=true;
    std::string bytes=Json::writeString(writer,value);
    if(bytes.empty() || bytes.size()>sizeof(output->data))return fail(EOVERFLOW);
    memcpy(output->data,bytes.data(),bytes.size());output->bytes=bytes.size();output->kind=kind;
    return 0;
}
}
extern "C" int aegis_broker_owner_package(aegis_broker_owner* owner,const aegis_broker_call* call,
                                          aegis_broker_package_reply* output) {
    if(!call || !output || !aegis_broker_package_operation(call->request.operation))return fail(EINVAL);
    *output={};output->job=call->command;
    const auto& r=call->request;
    if(r.operation==AEGIS_BROKER_PACKAGE_BEGIN) {
        if(call->command || call->package_action<1 || call->package_action>3
           || call->package_scope<1 || call->package_scope>2
           || strnlen(call->package_name,129)==129 || strnlen(call->package_version,129)==129)return fail(EINVAL);
        const aegis::PackageIntent request{static_cast<aegis::PackageAction>(call->package_action),
            call->package_name,call->package_version,call->package_scope==1};
        return aegis::BrokerBeginConfiguredPackage(owner,r.user,r.serial,request,r.deadline_ns,&output->job);
    }
    if(!call->command || call->command>INT64_MAX)return fail(EINVAL);
    switch(r.operation) {
    case AEGIS_BROKER_PACKAGE_PLAN:
        return aegis::BrokerContinueConfiguredPackagePlanning(owner,r.user,r.serial,call->command,r.deadline_ns);
    case AEGIS_BROKER_PACKAGE_PREPARE:
        return aegis::BrokerPrepareConfiguredTransaction(owner,r.user,r.serial,call->command,r.deadline_ns);
    case AEGIS_BROKER_PACKAGE_CANCEL:
        return aegis::BrokerCancelConfiguredPackage(owner,r.user,r.serial,call->command,r.deadline_ns);
    case AEGIS_BROKER_PACKAGE_START:
        if(strnlen(call->package_digest,65)!=64)return fail(EINVAL);
        return aegis::BrokerStartConfiguredPackage(owner,r.user,r.serial,call->command,call->package_digest,r.deadline_ns);
    case AEGIS_BROKER_PACKAGE_STATUS: {
        aegis::ConfiguredPackageStatus status;
        if(aegis::BrokerPollConfiguredPackage(owner,r.user,r.serial,call->command,&status)<0)return -1;
        auto data=intent(status.intent,status.plan_sha256,status.valid_until_unix);
        data["phase"]=static_cast<unsigned>(status.phase);
        data["outcome"]=static_cast<unsigned>(status.result.outcome);
        data["waitStatus"]=status.result.status;data["error"]=status.result.error;
        Json::Value generation(Json::objectValue);
        generation["image"]=status.result.generation.image_sha256;
        generation["sharedBase"]=status.result.generation.shared_base_sha256;
        generation["bytes"]=Json::UInt64(status.result.generation.bytes);
        data["generation"]=std::move(generation);
        return encode(data,1,output);
    }
    case AEGIS_BROKER_PACKAGE_REVIEW: {
        aegis::PackageBoundPlan bound;
        if(aegis::BrokerReviewConfiguredPackage(owner,r.user,r.serial,call->command,r.deadline_ns,&bound)<0)return -1;
        const auto& review=bound.reviewed;
        if(review.changes.size()>64)return fail(EOVERFLOW);
        aegis::PackageIntent request{review.action,review.requested_package,review.requested_version,review.personal};
        auto data=intent(request,bound.preparation.execution.plan_sha256,bound.valid_until_unix);
        Json::Value changes(Json::arrayValue);
        for(const auto& change:review.changes) {
            Json::Value item(Json::objectValue);
            item["name"]=change.name;item["architecture"]=change.architecture;
            item["before"]=change.before_version;item["after"]=change.after_version;
            item["reason"]=static_cast<unsigned>(change.reason);changes.append(std::move(item));
        }
        data["changes"]=std::move(changes);
        return encode(data,2,output);
    }
    default:return fail(EINVAL);
    }
}
