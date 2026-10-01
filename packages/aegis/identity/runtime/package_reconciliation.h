#ifndef AEGIS_PACKAGE_RECONCILIATION_H
#define AEGIS_PACKAGE_RECONCILIATION_H
#include "package_private_choices.h"
#include "package_apt_plan.h"
namespace aegis {
constexpr size_t kPackageReconciliationRoots=512;
struct PackageReconciliationInput {
    std::string personal_status,personal_automatic,private_choices;
    std::string previous_status,previous_automatic,current_status,current_automatic;
};
struct PackageReconciliationGoals {
    PackagePrivateChoices roots;
    std::vector<std::string> arguments;
    // Resolver scratch only: all installed non-roots become automatic so APT
    // may remove obsolete roots only when no retained root still needs them.
    // Never overwrite the initial-state evidence or the selected generation.
    std::string solver_automatic;
};
// Input must come from three verified readonly generations. Only deliberate
// private choices override current shared manual roots. Automatic dependencies
// remain solver decisions; never pin the new shared dependency versions across
// a private override. Legacy/contradictory intent fails, never guessed.
int PackageReconciliationDerive(const PackageReconciliationInput&,PackageReconciliationGoals*);
// Check the complete simulated installed set against every exact root and
// existing dpkg holds. Dependency consistency still requires trusted APT.
// These helpers neither authorize, execute, publish nor activate a generation.
int PackageReconciliationCheckEffects(const PackageReconciliationInput&,
    const PackageReconciliationGoals&,const std::vector<PackageAptEffect>&);
}
#endif
