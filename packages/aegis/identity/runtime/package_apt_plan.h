#ifndef AEGIS_PACKAGE_APT_PLAN_H
#define AEGIS_PACKAGE_APT_PLAN_H
#include "package_plan.h"
namespace aegis {
struct PackageAptEffect {
    std::string name, architecture, before_version, after_version;
    bool automatic=false;
};
// Pinned APT 3.0.3 JSON hook protocol 0.2, pre-prompt notification only.
// Input must originate in the owned trusted-planner namespace, with immutable
// APT/config/keyring and verified repositories. This parser grants no authority
// and cannot authenticate arbitrary JSON. Does not parse human terminal output.
// Exact requested command/version, complete effects and auto/manual state are
// retained. Archive evidence must still be joined to these selected versions;
// do not substitute versions.candidate for versions.install.
// Bounded to 256 KiB / 64 effects. Failure leaves output untouched. A successful
// empty vector is a resolver no-op, not an install or an approval. Purge and
// reinstall are unsupported; mixed install/remove is represented without loss.
int PackageReadAptPlan(const std::string& notification,PackageAction action,
                       const std::string& package,const std::string& version,
                       std::vector<PackageAptEffect>* output);
}
#endif
