#ifndef AEGIS_PACKAGE_PRIVATE_CHOICES_H
#define AEGIS_PACKAGE_PRIVATE_CHOICES_H
#include "package_plan.h"
#include <map>
namespace aegis {
struct PackagePrivateChoice {
    std::string architecture, version;
    bool operator==(const PackagePrivateChoice& o) const { return architecture==o.architecture && version==o.version; }
};
using PackagePrivateChoices=std::map<std::string,PackagePrivateChoice>;
constexpr const char* kPrivateChoicesPath="var/lib/aegis/private-choices";
// Canonical, bounded generation metadata. Empty input means no manifest;
// a present empty selection has a version header. No caller path or authority.
int PackagePrivateChoicesDecode(const std::string& text,PackagePrivateChoices* output);
int PackagePrivateChoicesEncode(const PackagePrivateChoices& choices,std::string* output);
// Apply an explicitly approved intent to already validated complete effects.
// Unchanged explicit installs use canonical frozen installed version evidence;
// never infer ownership from a file diff.
// Only after a successful APT simulation has returned no package changes.
// A requested already-installed dependency becomes an explicitly manual root.
int PackageSameVersionSelection(const std::string& status,const std::string& automatic,
    const std::string& requested,const std::string& version,PackageChange* output);
int PackagePrivateChoicesApply(const std::string& before,PackageAction action,
    const std::string& requested,const std::string& version,const std::vector<PackageChange>& changes,
    std::string* after);
}
#endif
