// Included after RuntimePackageResolver; reuse only its inert image/cgroup fixture.
class RuntimePackagePlanning : public RuntimePackageResolver {
 protected:
    aegis::PackagePlanner* planner=nullptr;
    aegis_broker_owner* owner=nullptr;
    Fd sources,key,helper;
    aegis::PackagePlanning request;
    static uint64_t Deadline() {
        timespec t={};EXPECT_EQ(0,clock_gettime(CLOCK_MONOTONIC,&t));
        return uint64_t(t.tv_sec)*1000000000+t.tv_nsec+UINT64_C(5000000000);
    }
    void SetUp() override {
        RuntimePackageResolver::SetUp();ASSERT_FALSE(HasFatalFailure());
        ASSERT_EQ(0,Write(selected.get(),"usr/bin/apt-get","#!/bin/sh\nexit 88\n"));
        ASSERT_EQ(0,Write(selected.get(),"etc/apt/apt.conf.d/99aegis-hostile","APT::Update::Pre-Invoke { \"exit 89\"; };\n"));
        initial=Read(selected.get(),"var/lib/dpkg/status");ASSERT_NE("<unavailable>",initial);
        automatic="Package: aegis-probe-lib\nArchitecture: arm64\nAuto-Installed: 1\n\n";
        ASSERT_EQ(0,Write(selected.get(),"var/lib/apt/extended_states",automatic));
        Fd directory(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(directory.ok());
        ASSERT_EQ(0,Write(directory.get(),"sources.list","deb [signed-by=/run/aegis-plan-policy/key.asc] copy:/tmp/aegis-repo ./\n"));
        for(const auto& file:aegis_apt_metadata)if(!strcmp(file.name,"aegis-test-key.asc"))
            ASSERT_EQ(0,Write(directory.get(),"key.asc",std::string(reinterpret_cast<const char*>(file.data),file.size)));
        sources.reset(openat(directory.get(),"sources.list",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));
        key.reset(openat(directory.get(),"key.asc",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(sources.ok());ASSERT_TRUE(key.ok());
        mount_attr attr={};attr.attr_set=MOUNT_ATTR_RDONLY;
        ASSERT_EQ(0,syscall(SYS_mount_setattr,selected.get(),"",AT_EMPTY_PATH,&attr,sizeof(attr)));
        char path[PATH_MAX];ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);ASSERT_GT(n,0);path[n]=0;
        std::string program(path);program.resize(program.rfind('/')+1);program+="aegis-package-plan-probe";
        helper.reset(open(program.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(helper.ok());
        request.requester=10;request.serial=1234;request.job=1;
        request.request.package="aegis-probe-app";request.request.version="2";
    }
    void TearDown() override {
        if(owner) { EXPECT_EQ(0,aegis_broker_owner_stop_all(owner,Deadline()));EXPECT_EQ(0,aegis_broker_owner_release(&owner)); }
        if(planner) { int fd=-1;aegis::PackagePlanningResult r;EXPECT_EQ(0,aegis::PackagePlannerFinish(&planner,true,5000,&r,&fd));if(fd>=0)close(fd); }
        if(image)for(const char* n:{"sources.list","key.asc"})EXPECT_EQ(0,unlink((image->directory+"/"+n).c_str()));
        sources.reset();key.reset();helper.reset();RuntimePackageResolver::TearDown();
    }
    int Start() { return aegis::PackagePlannerStart(parent.get(),source,selected.get(),sources.get(),key.get(),helper.get(),request,Deadline(),&planner); }
    void CreateOwner() {
        ASSERT_EQ(0,aegis_broker_owner_create(parent.get(),source,setup,setup,&owner));request.job=0;
    }
    int StartOwner(uint64_t* job,int program=-2) {
        return aegis::BrokerStartPlanning(owner,request,parent.get(),source,selected.get(),sources.get(),key.get(),program==-2?helper.get():program,Deadline(),job);
    }
    void Collected(uint64_t job) {
        aegis::PlanningState state;aegis::PackagePlanningResult result;
        for(int i=0;i<200;++i) {
            ASSERT_EQ(0,aegis::BrokerPollPlanning(owner,10,1234,job,&state,&result));
            if(state!=aegis::PlanningState::Running)break;usleep(50000);
        }
        ASSERT_EQ(aegis::PlanningState::Collected,state)<<result.error<<" worker status="<<result.status;
        EXPECT_EQ(aegis::PackagePlanningResult::Outcome::Collected,result.outcome);EXPECT_EQ(2u,result.effects);
    }
};
TEST_F(RuntimePackagePlanning, OwnedChildCopiesOnlyMetadataAndReturnsEvidenceAfterReaping) {
    int before=descriptors();mode_t previous=umask(077);int started=Start();int error=errno;umask(previous);
    ASSERT_EQ(0,started)<<strerror(error);
    aegis::PackagePlanningResult result;int fd=-1;
    ASSERT_EQ(0,aegis::PackagePlannerFinish(&planner,false,10000,&result,&fd))<<strerror(errno);
    ASSERT_EQ(aegis::PackagePlanningResult::Outcome::Collected,result.outcome)<<result.error<<" worker status="<<result.status;
    ASSERT_GE(fd,0);Fd evidence(fd);EXPECT_EQ(2u,result.effects);
    auto receipt=Read(fd,"receipt.json");EXPECT_NE(std::string::npos,receipt.find(Hash(initial)));
    EXPECT_NE(std::string::npos,Read(fd,"simulation.json").find("aegis-probe-app"));
    EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));EXPECT_EQ(automatic,Read(selected.get(),"var/lib/apt/extended_states"));
    evidence.reset();EXPECT_EQ(before,descriptors());
}
TEST_F(RuntimePackagePlanning, CancellationReapsWorkerAndDiscardsQueuedDirectory) {
    int before=descriptors();ASSERT_EQ(0,Start());
    ASSERT_EQ(0,aegis::PackagePlannerCancel(planner));
    aegis::PackagePlanningResult result;int fd=-1;
    ASSERT_EQ(0,aegis::PackagePlannerFinish(&planner,false,5000,&result,&fd));
    EXPECT_EQ(aegis::PackagePlanningResult::Outcome::Failed,result.outcome);EXPECT_EQ(ECANCELED,result.error);
    EXPECT_EQ(-1,fd);EXPECT_EQ(before,descriptors());
}
TEST_F(RuntimePackagePlanning, CollectedBrokerEvidenceStaysOwnedUntilRequesterStop) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());int before=descriptors();uint64_t job=0;
    ASSERT_EQ(0,StartOwner(&job));ASSERT_NE(0u,job);Collected(job);ASSERT_FALSE(HasFatalFailure());
    uint64_t another=0;EXPECT_EQ(-1,StartOwner(&another));EXPECT_EQ(EBUSY,errno);EXPECT_EQ(0u,another);
    EXPECT_EQ(-1,aegis_broker_owner_release(&owner));EXPECT_EQ(EBUSY,errno);
    aegis_broker_request stop={};stop.magic=AEGIS_BROKER_MAGIC;stop.version=AEGIS_BROKER_VERSION;stop.operation=AEGIS_BROKER_STOP_USER;stop.user=10;stop.serial=0;stop.sequence=2;stop.deadline_ns=Deadline();
    aegis_broker_state state;ASSERT_EQ(0,aegis_broker_owner_apply(owner,&stop,&state))<<strerror(errno);
    EXPECT_EQ(AEGIS_BROKER_ABSENT,state);
    EXPECT_EQ(before,descriptors());
    aegis::PlanningState ps;aegis::PackagePlanningResult result;
    EXPECT_EQ(-1,aegis::BrokerPollPlanning(owner,10,1234,job,&ps,&result));EXPECT_EQ(ENOENT,errno);
    ASSERT_EQ(0,StartOwner(&another));EXPECT_GT(another,job);
}
TEST_F(RuntimePackagePlanning, IdentityMismatchCannotPollOrCancelAnotherJob) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());uint64_t job=0;ASSERT_EQ(0,StartOwner(&job));
    aegis::PlanningState state;aegis::PackagePlanningResult result;
    EXPECT_EQ(-1,aegis::BrokerPollPlanning(owner,11,1234,job,&state,&result));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,aegis::BrokerCancelPlanning(owner,10,1235,job,Deadline()));EXPECT_EQ(ESTALE,errno);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));
    EXPECT_EQ(-1,aegis::BrokerPollPlanning(owner,10,1234,job,&state,&result));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackagePlanning, PartialStartKeepsConsumedJobAndReleasesAllResources) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());int before=descriptors();uint64_t job=0;
    EXPECT_EQ(-1,StartOwner(&job,-1));ASSERT_NE(0u,job);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));EXPECT_EQ(before,descriptors());
    uint64_t next=0;ASSERT_EQ(0,StartOwner(&next));EXPECT_GT(next,job);
}
TEST_F(RuntimePackagePlanning, ExpiredStopStillCancelsBeforeBoundedRetry) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());uint64_t job=0;ASSERT_EQ(0,StartOwner(&job));
    int result=aegis_broker_owner_stop_all(owner,0);
    if(result<0)EXPECT_EQ(ETIMEDOUT,errno);
    ASSERT_EQ(0,aegis_broker_owner_stop_all(owner,Deadline()));
    aegis::PlanningState state;aegis::PackagePlanningResult r;
    EXPECT_EQ(-1,aegis::BrokerPollPlanning(owner,10,1234,job,&state,&r));EXPECT_EQ(ENOENT,errno);
}

TEST_F(RuntimePackagePlanning, ProductionEntryRejectsTestDomain) {
    char path[PATH_MAX];ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);ASSERT_GT(n,0);path[n]=0;
    std::string program(path);program.resize(program.rfind('/')+1);program+="aegis-package-plan";
    helper.reset(open(program.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(helper.ok());
    ASSERT_EQ(0,Start());int fd=-1;aegis::PackagePlanningResult result;
    ASSERT_EQ(0,aegis::PackagePlannerFinish(&planner,false,5000,&result,&fd));
    EXPECT_NE(aegis::PackagePlanningResult::Outcome::Collected,result.outcome);EXPECT_EQ(-1,fd);
}
TEST(PackagePlanningProtocol, RejectsExcessDescriptorsWithoutLeaking) {
    using Fd=android::base::unique_fd;
    int pair[2];ASSERT_EQ(0,socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,pair));Fd first(pair[0]),second(pair[1]);
    Fd input(open("/dev/null",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(input.ok());int fds[]={input.get(),input.get()};
    aegis_planning_reply sent={},received={};int before=descriptors();
    ASSERT_EQ(0,aegis_planning_send(first.get(),&sent,sizeof(sent),fds,2));
    int output=-1;EXPECT_EQ(-1,aegis_planning_receive(second.get(),&received,sizeof(received),&output,1));
    EXPECT_EQ(EPROTO,errno);EXPECT_EQ(-1,output);EXPECT_EQ(before,descriptors());
}

TEST_F(RuntimePackagePlanning, VerifiedSelectionTransfersDirectlyIntoSamePlanningJob) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());
    Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(factory.ok());
    char path[PATH_MAX];ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);ASSERT_GT(n,0);path[n]=0;
    std::string program(path);program.resize(program.rfind('/')+1);program+="aegis-package-prepare";
    Fd prepare(open(program.c_str(),O_RDONLY|O_CLOEXEC));ASSERT_TRUE(prepare.ok());
    aegis::PackageRuntimeSelection selection;selection.requester=10;selection.serial=1234;
    selection.factory={image->original.bytes,image->original.sha256};uint64_t selected_job=0,job=0;
    ASSERT_EQ(0,aegis::BrokerPrepareRuntimeSelection(owner,selection,parent.get(),-1,-1,factory.get(),prepare.get(),Deadline(),&selected_job));
    int started=-1;
    for(int i=0;i<200;++i) {
        started=aegis::BrokerStartPlanningFromSelection(owner,request,selected_job,parent.get(),source,sources.get(),key.get(),helper.get(),Deadline(),&job);
        if(started==0)break;ASSERT_EQ(EAGAIN,errno);ASSERT_EQ(0u,job);usleep(50000);
    }
    ASSERT_EQ(0,started);EXPECT_EQ(selected_job,job);
    aegis::RuntimeSelectionState state;aegis::PackagePreparationResult result;
    EXPECT_EQ(-1,aegis::BrokerPollRuntimeSelection(owner,10,1234,selected_job,&state,&result));EXPECT_EQ(ENOENT,errno);
    Collected(job);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));
}
