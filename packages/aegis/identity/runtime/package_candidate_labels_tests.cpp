#include "package_candidate_labels.h"
#include <gtest/gtest.h>
#include <android-base/unique_fd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
namespace {
using android::base::unique_fd;
constexpr const char* kForeign="u:object_r:shell_data_file:s0";
class RuntimePackageCandidateLabels : public ::testing::Test {
 protected:
    std::string directory,path;
    unique_fd root;
    std::vector<std::pair<std::string,bool>> entries;
    void SetUp() override {
        ASSERT_EQ(0u,getuid());
        struct sigaction action={};action.sa_handler=SIG_DFL;sigemptyset(&action.sa_mask);
        ASSERT_EQ(0,sigaction(SIGCHLD,&action,nullptr));
        char pattern[]="/data/local/tmp/aegis-candidate-labels-XXXXXX";
        ASSERT_NE(nullptr,mkdtemp(pattern));directory=pattern;path=directory+"/candidate";
        ASSERT_EQ(0,mkdir(path.c_str(),0700));
        root.reset(open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(root.ok());
        ASSERT_EQ(0,fsetxattr(root.get(),"security.selinux",AEGIS_PACKAGE_CANDIDATE_CONTEXT,
                             sizeof(AEGIS_PACKAGE_CANDIDATE_CONTEXT),0));
    }
    void TearDown() override {
        root.reset();
        if(HasFailure()) { fprintf(stderr,"Candidate-label fixture retained: %s\n",directory.c_str());return; }
        for(auto i=entries.rbegin();i!=entries.rend();++i)
            EXPECT_EQ(0,i->second?rmdir(i->first.c_str()):unlink(i->first.c_str()));
        EXPECT_EQ(0,rmdir(path.c_str()));EXPECT_EQ(0,rmdir(directory.c_str()));
    }
    void Label(const std::string& name,const char* context=AEGIS_PACKAGE_CANDIDATE_CONTEXT) {
        ASSERT_EQ(0,lsetxattr((path+"/"+name).c_str(),"security.selinux",context,strlen(context)+1,0));
    }
    void Dir(const std::string& name) {
        std::string target=path+"/"+name;ASSERT_EQ(0,mkdir(target.c_str(),0700));entries.emplace_back(target,true);Label(name);
    }
    void File(const std::string& name) {
        std::string target=path+"/"+name;
        unique_fd fd(open(target.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600));ASSERT_TRUE(fd.ok());
        entries.emplace_back(target,false);ASSERT_EQ(4,write(fd.get(),"keep",4));Label(name);
    }
    void Link(const char* name,const char* target) {
        ASSERT_EQ(0,symlinkat(target,root.get(),name));entries.emplace_back(path+"/"+name,false);Label(name);
    }
    int Fds() {
        DIR* d=opendir("/proc/self/fd");if(!d)return -1;int n=0;
        while(auto* e=readdir(d))if(strcmp(e->d_name,".")&&strcmp(e->d_name,".."))++n;
        closedir(d);return n;
    }
};
TEST_F(RuntimePackageCandidateLabels, AcceptsOnlyCandidateInodesWithoutFollowingLinkTargets) {
    Dir("sub");File("sub/file");Link("absent","/does-not-exist");Link("outside","/system/bin/sh");
    ASSERT_FALSE(HasFatalFailure());int before=Fds();
    EXPECT_EQ(0,aegis_package_candidate_labels(root.get(),0))<<strerror(errno);
    EXPECT_EQ(before,Fds());
}
TEST_F(RuntimePackageCandidateLabels, RejectsForeignRootAndInvalidInputs) {
    ASSERT_EQ(0,fsetxattr(root.get(),"security.selinux",kForeign,strlen(kForeign)+1,0));
    EXPECT_EQ(-1,aegis_package_candidate_labels(root.get(),0));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ(-1,aegis_package_candidate_labels(root.get(),2));EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ(-1,aegis_package_candidate_labels(-1,0));EXPECT_EQ(EBADF,errno);
}
TEST_F(RuntimePackageCandidateLabels, RejectsMixedFilesAndUnmountedRuntimeNames) {
    Dir("sub");File("sub/file");Dir("tmp");ASSERT_FALSE(HasFatalFailure());
    Label("sub/file",kForeign);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,aegis_package_candidate_labels(root.get(),0));EXPECT_EQ(EPERM,errno);
    Label("sub/file");Label("tmp",kForeign);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,aegis_package_candidate_labels(root.get(),1));EXPECT_EQ(EPERM,errno);
    Label("tmp");ASSERT_FALSE(HasFatalFailure());EXPECT_EQ(0,aegis_package_candidate_labels(root.get(),0));
}
TEST_F(RuntimePackageCandidateLabels, RejectsForeignSymlinkInodeWithoutReadingItsTarget) {
    Link("foreign","/system/bin/sh");ASSERT_FALSE(HasFatalFailure());Label("foreign",kForeign);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,aegis_package_candidate_labels(root.get(),0));EXPECT_EQ(EPERM,errno);
    char target[64];ssize_t n=readlinkat(root.get(),"foreign",target,sizeof(target));
    ASSERT_EQ(14,n);EXPECT_EQ(std::string("/system/bin/sh"),std::string(target,n));
    Label("foreign");ASSERT_FALSE(HasFatalFailure());EXPECT_EQ(0,aegis_package_candidate_labels(root.get(),0));
}
TEST_F(RuntimePackageCandidateLabels, RejectsSpecialNodesWithoutOpeningOrBlocking) {
    ASSERT_EQ(0,mkfifoat(root.get(),"pipe",0600));entries.emplace_back(path+"/pipe",false);
    EXPECT_EQ(-1,aegis_package_candidate_labels(root.get(),0));EXPECT_EQ(EPERM,errno);
}
TEST_F(RuntimePackageCandidateLabels, BoundsRecursionAndClosesEveryDirectoryOnFailure) {
    std::string nested;for(unsigned i=0;i<130;++i) { if(i)nested+="/";nested+="d";Dir(nested);ASSERT_FALSE(HasFatalFailure()); }
    int before=Fds();EXPECT_EQ(-1,aegis_package_candidate_labels(root.get(),0));EXPECT_EQ(E2BIG,errno);EXPECT_EQ(before,Fds());
}
TEST_F(RuntimePackageCandidateLabels, RejectsForeignMountsExceptExplicitRuntimeAnchors) {
    Dir("tmp");Dir("other");ASSERT_FALSE(HasFatalFailure());
    pid_t child=fork();ASSERT_GE(child,0);
    if(!child) {
        if(unshare(CLONE_NEWNS)<0 || mount(nullptr,"/",nullptr,MS_REC|MS_PRIVATE,nullptr)<0)_exit(1);
        unique_fd current(open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));if(!current.ok())_exit(2);
        if(mount("tmpfs",(path+"/tmp").c_str(),"tmpfs",MS_NOSUID|MS_NODEV|MS_NOEXEC,"size=1048576")<0)_exit(3);
        if(aegis_package_candidate_labels(current.get(),0)!=-1 || errno!=EXDEV)_exit(4);
        if(aegis_package_candidate_labels(current.get(),1))_exit(5);
        if(mount("tmpfs",(path+"/other").c_str(),"tmpfs",MS_NOSUID|MS_NODEV|MS_NOEXEC,"size=1048576")<0)_exit(6);
        if(aegis_package_candidate_labels(current.get(),1)!=-1 || errno!=EXDEV)_exit(7);
        _exit(0); // Kernel destroys only this child's private mount namespace.
    }
    int status=0;ASSERT_EQ(child,TEMP_FAILURE_RETRY(waitpid(child,&status,0)));
    ASSERT_TRUE(WIFEXITED(status));EXPECT_EQ(0,WEXITSTATUS(status));
    EXPECT_EQ(0,aegis_package_candidate_labels(root.get(),0));
}
} // namespace
