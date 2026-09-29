#ifndef AEGIS_CE_LIVE_TEST_H
#define AEGIS_CE_LIVE_TEST_H
// Test-only opt-in metadata for a NEW isolated QEMU profile. No credentials,
// production entry point or authentication bypass. The host driver creates
// and authenticates these synthetic AOSP users through the actual aegis CLI.
#include <android-base/unique_fd.h>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
namespace aegis_ce_test {
struct Identity { uint32_t user=0, serial=0; };
inline bool Number(const char* name,uint32_t* out) {
    const char* value=getenv(name);if(!value || !*value)return false;
    errno=0;char* end=nullptr;unsigned long n=strtoul(value,&end,10);
    if(errno || !end || *end || n>INT32_MAX || std::to_string(n)!=value)return false;
    *out=n;return true;
}
inline bool Read(Identity* output) {
    Identity value;
    const char* expected=getenv("AEGIS_CE_TEST_NAME");
    if(getuid()!=0 || getgid()!=0 || !Number("AEGIS_CE_TEST_USER",&value.user)
            || !Number("AEGIS_CE_TEST_SERIAL",&value.serial)
            || value.user<10 || value.user>=21473 || !expected
            || (strcmp(expected,"Runtime Test Alpha") && strcmp(expected,"Runtime Test Beta")))return false;
    std::string path="/data/system/users/"+std::to_string(value.user)+".xml";
    android::base::unique_fd fd(open(path.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW));
    struct stat st;
    if(!fd.ok() || fstat(fd.get(),&st)<0 || !S_ISREG(st.st_mode) || st.st_size<=0 || st.st_size>16384)return false;
    std::array<char,16385> bytes;
    ssize_t n=pread(fd.get(),bytes.data(),bytes.size(),0);if(n!=st.st_size)return false;
    std::string xml(bytes.data(),n);
    if(xml.find("<name>"+std::string(expected)+"</name>")==std::string::npos
            || xml.find("serialNumber=\""+std::to_string(value.serial)+"\"")==std::string::npos)return false;
    *output=value;return true;
}
}
#endif
