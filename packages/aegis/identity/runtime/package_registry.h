#ifndef AEGIS_PACKAGE_REGISTRY_H
#define AEGIS_PACKAGE_REGISTRY_H
#include <map>
#include <set>
#include <string>
#include <tuple>
namespace aegis {
// Canonical installed version, architecture and dpkg selection from a frozen
// status file. Reused by the resolver and the independent execution guard.
using PackageInstalledRegistry=std::map<std::string,std::tuple<std::string,std::string,std::string>>;
int PackageReadInstalledRegistry(const std::string& text,PackageInstalledRegistry* output);
int PackageReadAutomaticRegistry(const std::string& text,std::set<std::string>* output);
}
#endif
