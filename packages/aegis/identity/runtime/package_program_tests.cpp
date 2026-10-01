#include "package_program.h"
#include <gtest/gtest.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string>
#include <vector>

TEST(RuntimePackageProgramEntry, FixedCommandsRejectTheUnpreparedTestProcess) {
    for(const char* path : {"/usr/bin/apt-get", "/usr/bin/dpkg", "/usr/bin/apt-mark"}) {
        char* args[]={const_cast<char*>(path),nullptr};
        EXPECT_EQ(126,aegis_package_program_main(1,args)) << path;
    }
    EXPECT_EQ(126,aegis_package_program_main(0,nullptr));
}
TEST(RuntimePackageProgramEntry, ProductionHelpersRejectDirectProgramEntry) {
    char self[PATH_MAX];ssize_t n=readlink("/proc/self/exe",self,sizeof(self)-1);
    ASSERT_GT(n,0);ASSERT_LT(n,static_cast<ssize_t>(sizeof(self)-1));self[n]=0;
    std::string directory(self);directory.resize(directory.rfind('/')+1);
    struct sigaction original={},normal={};normal.sa_handler=SIG_DFL;sigemptyset(&normal.sa_mask);
    ASSERT_EQ(0,sigaction(SIGCHLD,&normal,&original));
    for(const char* name : {"aegis-package-plan","aegis-package-execute"}) {
        std::string path=directory+name;
        pid_t pid=fork();
        if(pid==0) { alarm(10);execl(path.c_str(),name,"--package-program","/usr/bin/dpkg","--version",nullptr);_exit(127); }
        EXPECT_GT(pid,0);if(pid<0)continue;
        int status=0;pid_t waited;
        do { waited=waitpid(pid,&status,0); } while(waited<0 && errno==EINTR);
        EXPECT_EQ(pid,waited);EXPECT_TRUE(WIFEXITED(status));
        if(WIFEXITED(status))EXPECT_EQ(126,WEXITSTATUS(status)) << name;
    }
    EXPECT_EQ(0,sigaction(SIGCHLD,&original,nullptr));
}
TEST(RuntimePackageProgramEntry, RejectsArbitraryExecutablesBeforeReexec) {
    char* environment[]={nullptr};
    for(const char* name : {"/bin/sh","apt-get","/usr/bin/../bin/apt-get","/tmp/apt-get",""}) {
        char* arguments[]={const_cast<char*>(name),nullptr};errno=0;
        EXPECT_EQ(-1,aegis_package_exec_program(arguments,environment));EXPECT_EQ(EPERM,errno);
    }
    errno=0;EXPECT_EQ(-1,aegis_package_exec_program(nullptr,environment));EXPECT_EQ(EPERM,errno);
}
TEST(RuntimePackageProgramEntry, RefusesOversizedArgumentVectorBeforeReexec) {
    std::vector<char*> arguments(1026,const_cast<char*>("x"));
    arguments[0]=const_cast<char*>("/usr/bin/apt-get");arguments.back()=nullptr;
    char* environment[]={nullptr};errno=0;
    EXPECT_EQ(-1,aegis_package_exec_program(arguments.data(),environment));EXPECT_EQ(E2BIG,errno);
}
TEST(RuntimePackageProgramEntry, RefusesOversizedArgumentBytesBeforeReexec) {
    std::string large(262144,'x');
    char* arguments[]={const_cast<char*>("/usr/bin/apt-get"),large.data(),nullptr};
    char* environment[]={nullptr};errno=0;
    EXPECT_EQ(-1,aegis_package_exec_program(arguments,environment));EXPECT_EQ(E2BIG,errno);
}
