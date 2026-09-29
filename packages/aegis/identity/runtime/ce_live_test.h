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
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
namespace aegis_ce_test {
struct Identity { uint32_t user=0, serial=0; };
inline bool Number(const char* name,uint32_t* out) {
    const char* value=getenv(name);if(!value || !*value)return false;
    errno=0;char* end=nullptr;unsigned long n=strtoul(value,&end,10);
    if(errno || !end || *end || n>INT32_MAX || std::to_string(n)!=value)return false;
    *out=n;return true;
}
// AOSP persists users as ABX. Decode only these fixed, synthetic test records
// using AOSP's own reader, without rewriting the authoritative metadata.
inline bool ReadXml(int fd,const std::string& path,std::string* xml) {
    std::array<char,16385> bytes;
    ssize_t n=pread(fd,bytes.data(),bytes.size(),0);
    if(n<=0 || n>=static_cast<ssize_t>(bytes.size()))return false;
    if(n<4 || memcmp(bytes.data(),"ABX\0",4)) {
        xml->assign(bytes.data(),n);return true;
    }
    int pipes[2];if(pipe2(pipes,O_CLOEXEC)<0)return false;
    android::base::unique_fd input(pipes[0]),output(pipes[1]);
    pid_t child=fork();if(child<0)return false;
    if(child==0) {
        input.reset();
        if(dup2(output.get(),STDOUT_FILENO)<0)_exit(126);
        output.reset();
        execl("/system/bin/abx2xml","abx2xml",path.c_str(),"-",static_cast<char*>(nullptr));
        _exit(127);
    }
    output.reset();size_t used=0;bool valid=true;
    for(;;) {
        n=read(input.get(),bytes.data()+used,bytes.size()-used);
        if(n<0 && errno==EINTR)continue;
        if(n<0){valid=false;break;}
        if(n==0)break;
        used+=n;
        if(used==bytes.size()){valid=false;break;}
    }
    input.reset();
    if(!valid)kill(child,SIGKILL);
    int status=0;pid_t waited;
    do { waited=waitpid(child,&status,0); } while(waited<0 && errno==EINTR);
    if(!valid || waited!=child || !WIFEXITED(status) || WEXITSTATUS(status)!=0 || !used)return false;
    xml->assign(bytes.data(),used);return true;
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
    std::string xml;
    if(!ReadXml(fd.get(),path,&xml))return false;
    if(xml.find("id=\""+std::to_string(value.user)+"\"")==std::string::npos
            || xml.find("<name>"+std::string(expected)+"</name>")==std::string::npos
            || xml.find("serialNumber=\""+std::to_string(value.serial)+"\"")==std::string::npos)return false;
    *output=value;return true;
}
}
#endif
