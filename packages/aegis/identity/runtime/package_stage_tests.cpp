#include "package_stage.h"
#include <gtest/gtest.h>
#include <android-base/unique_fd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <cerrno>
#include <cstdlib>
namespace {
using android::base::unique_fd;
class RuntimePackageStage : public ::testing::Test {
 protected:
    std::string path;
    unique_fd parent;
    aegis_package_stage stage=AEGIS_PACKAGE_STAGE_INIT;
    void SetUp() override {
        char pattern[]="/data/local/tmp/aegis-stage-XXXXXX";
        const char* dir=mkdtemp(pattern);ASSERT_NE(nullptr,dir);path=dir;
        parent.reset(open(path.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_GE(parent.get(),0);
    }
    void TearDown() override {
        if(!HasFailure())EXPECT_EQ(0,aegis_package_stage_cleanup(&stage,4096,4096));
        aegis_package_stage_close(&stage);
        if(!HasFailure())EXPECT_EQ(0,rmdir(path.c_str()));
    }
    void Write(int dir,const char* name,const char* value,mode_t mode=0600) {
        unique_fd file(openat(dir,name,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600));ASSERT_GE(file.get(),0);
        ASSERT_EQ(ssize_t(strlen(value)),write(file.get(),value,strlen(value)));ASSERT_EQ(0,fchmod(file.get(),mode));
    }
    bool Exists(int dir,const char* name) { struct stat st;return fstatat(dir,name,&st,AT_SYMLINK_NOFOLLOW)==0; }
    int Count() {
        DIR* d=opendir("/proc/self/fd");if(!d)return -1;int count=0;
        while(auto* e=readdir(d))if(strcmp(e->d_name,".")&&strcmp(e->d_name,".."))++count;
        closedir(d);return count;
    }
};
TEST_F(RuntimePackageStage, CleansFixedFilesAndRetainsUnrelatedSiblings) {
    Write(parent.get(),"keep","keep");ASSERT_FALSE(HasFatalFailure());int before=Count();
    ASSERT_EQ(0,aegis_package_stage_create(parent.get(),"job-1-abc",&stage));
    Write(stage.directory,"request","request",0400);Write(stage.directory,"candidate.ext4","image");ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis_package_stage_cleanup(&stage,16,16));EXPECT_EQ(before,Count());
    EXPECT_FALSE(Exists(parent.get(),"job-1-abc"));EXPECT_TRUE(Exists(parent.get(),"keep"));
    EXPECT_EQ(-1,stage.directory);EXPECT_EQ(-1,stage.parent);ASSERT_EQ(0,unlinkat(parent.get(),"keep",0));
}
TEST_F(RuntimePackageStage, NeverAdoptsExistingDirectoriesOrUntrustedParents) {
    ASSERT_EQ(0,mkdirat(parent.get(),"job-1-abc",0700));int before=Count();
    EXPECT_EQ(-1,aegis_package_stage_create(parent.get(),"job-1-abc",&stage));EXPECT_EQ(EEXIST,errno);
    EXPECT_EQ(-1,stage.parent);EXPECT_EQ(before,Count());EXPECT_TRUE(Exists(parent.get(),"job-1-abc"));
    ASSERT_EQ(0,fchmod(parent.get(),0770));
    EXPECT_EQ(-1,aegis_package_stage_create(parent.get(),"job-2-def",&stage));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ(-1,stage.parent);EXPECT_FALSE(Exists(parent.get(),"job-2-def"));
    ASSERT_EQ(0,fchmod(parent.get(),0700));ASSERT_EQ(0,unlinkat(parent.get(),"job-1-abc",AT_REMOVEDIR));
}
TEST_F(RuntimePackageStage, UnknownEntryPreventsAllDeletionAndCanBeRetried) {
    ASSERT_EQ(0,aegis_package_stage_create(parent.get(),"job-1-abc",&stage));
    Write(stage.directory,"request","request");Write(stage.directory,"candidate.ext4","image");
    Write(stage.directory,"unexpected","keep");ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,aegis_package_stage_cleanup(&stage,16,16));
    EXPECT_TRUE(Exists(stage.directory,"request"));EXPECT_TRUE(Exists(stage.directory,"candidate.ext4"));
    EXPECT_TRUE(Exists(stage.directory,"unexpected"));EXPECT_GE(stage.parent,0);
    ASSERT_EQ(0,unlinkat(stage.directory,"unexpected",0));ASSERT_EQ(0,aegis_package_stage_cleanup(&stage,16,16));
}
TEST_F(RuntimePackageStage, RejectsLinksAndOversizedFilesWithoutTouchingTheirTargets) {
    Write(parent.get(),"keep","keep");ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis_package_stage_create(parent.get(),"job-1-abc",&stage));
    Write(stage.directory,"request","request",0400);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,linkat(parent.get(),"keep",stage.directory,"candidate.ext4",0));
    EXPECT_EQ(-1,aegis_package_stage_cleanup(&stage,16,16));EXPECT_EQ(EPERM,errno);
    EXPECT_TRUE(Exists(stage.directory,"request"));EXPECT_TRUE(Exists(parent.get(),"keep"));
    ASSERT_EQ(0,unlinkat(stage.directory,"candidate.ext4",0));
    ASSERT_EQ(0,symlinkat("../keep",stage.directory,"candidate.ext4"));
    EXPECT_EQ(-1,aegis_package_stage_cleanup(&stage,16,16));EXPECT_EQ(ELOOP,errno);
    EXPECT_TRUE(Exists(stage.directory,"request"));EXPECT_TRUE(Exists(parent.get(),"keep"));
    ASSERT_EQ(0,unlinkat(stage.directory,"candidate.ext4",0));
    Write(stage.directory,"candidate.ext4","too-large");ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,aegis_package_stage_cleanup(&stage,1,16));EXPECT_EQ(EPERM,errno);
    EXPECT_TRUE(Exists(stage.directory,"request"));ASSERT_EQ(0,aegis_package_stage_cleanup(&stage,16,16));
    ASSERT_EQ(0,unlinkat(parent.get(),"keep",0));
}
TEST_F(RuntimePackageStage, DirectoryReplacementCannotRedirectCleanup) {
    ASSERT_EQ(0,aegis_package_stage_create(parent.get(),"job-1-abc",&stage));
    Write(stage.directory,"request","request");ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,renameat(parent.get(),"job-1-abc",parent.get(),"saved"));
    ASSERT_EQ(0,mkdirat(parent.get(),"job-1-abc",0700));
    unique_fd replacement(openat(parent.get(),"job-1-abc",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_GE(replacement.get(),0);
    Write(replacement.get(),"keep","keep");ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,aegis_package_stage_cleanup(&stage,16,16));EXPECT_EQ(ESTALE,errno);
    EXPECT_TRUE(Exists(stage.directory,"request"));EXPECT_TRUE(Exists(replacement.get(),"keep"));
    ASSERT_EQ(0,unlinkat(replacement.get(),"keep",0));replacement.reset();
    ASSERT_EQ(0,unlinkat(parent.get(),"job-1-abc",AT_REMOVEDIR));
    ASSERT_EQ(0,renameat(parent.get(),"saved",parent.get(),"job-1-abc"));
    ASSERT_EQ(0,aegis_package_stage_cleanup(&stage,16,16));
}
TEST_F(RuntimePackageStage, ReclaimsInterruptedEmptyFilesAndRejectsInvalidBounds) {
    ASSERT_EQ(0,aegis_package_stage_create(parent.get(),"job-1-abc",&stage));
    Write(stage.directory,"request","");Write(stage.directory,"candidate.ext4","");ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,aegis_package_stage_cleanup(&stage,0,16));EXPECT_EQ(EINVAL,errno);EXPECT_GE(stage.directory,0);
    ASSERT_EQ(0,aegis_package_stage_cleanup(&stage,16,16));EXPECT_EQ(0,aegis_package_stage_cleanup(&stage,16,16));
}
} // namespace
