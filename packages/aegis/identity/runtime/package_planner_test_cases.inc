// Included after RuntimePackageResolver; reuse only its inert image/cgroup fixture.
class RuntimePackagePlanning : public RuntimePackageResolver {
 protected:
    aegis::PackagePlanner* planner=nullptr;
    aegis_broker_owner* owner=nullptr;
    Fd sources,key,helper,network,ca;
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
        if(image)for(const char* n:{"sources.list","key.asc","ca.pem"})if(unlink((image->directory+"/"+n).c_str())<0)EXPECT_EQ(ENOENT,errno);
        sources.reset();key.reset();helper.reset();network.reset();ca.reset();RuntimePackageResolver::TearDown();
    }
    int Start() { return aegis::PackagePlannerStart(parent.get(),source,selected.get(),sources.get(),key.get(),helper.get(),request,Deadline(),&planner,network.get(),ca.get()); }
    void CreateOwner() {
        ASSERT_EQ(0,aegis_broker_owner_create(parent.get(),source,setup,setup,&owner));request.job=0;
    }
    int StartOwner(uint64_t* job,int program=-2) {
        return aegis::BrokerStartPlanning(owner,request,parent.get(),source,selected.get(),sources.get(),key.get(),program==-2?helper.get():program,Deadline(),job,network.get(),ca.get());
    }
    void SelectOwner(uint64_t* job) {
        Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(factory.ok());
        Fd prepare(Program("aegis-package-prepare"));ASSERT_TRUE(prepare.ok());
        aegis::PackageRuntimeSelection selection;selection.requester=request.requester;selection.serial=request.serial;
        selection.factory={image->original.bytes,image->original.sha256};uint64_t selected_job=0;
        ASSERT_EQ(0,aegis::BrokerPreparePackageSelection(owner,selection,request.personal,parent.get(),-1,-1,factory.get(),prepare.get(),Deadline(),&selected_job));
        for(int i=0;i<200;++i) {
            if(aegis::BrokerStartPlanningFromSelection(owner,request,selected_job,parent.get(),source,sources.get(),key.get(),helper.get(),Deadline(),job)==0)break;
            ASSERT_EQ(EAGAIN,errno);ASSERT_EQ(0u,*job);usleep(50000);
        }
        ASSERT_EQ(selected_job,*job);ASSERT_NE(0u,*job);
    }
    int Program(const char* name) {
        char path[PATH_MAX];ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);if(n<=0)return -1;path[n]=0;
        std::string program(path);program.resize(program.rfind('/')+1);program+=name;
        return open(program.c_str(),O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    }
    void Online(const char* uri) {
        network.reset(Program("aegis-package-network"));ASSERT_TRUE(network.ok());request.request.internet=true;
        request.request.package="hello";request.request.version.clear();
        Fd directory(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(directory.ok());
        Fd certs(open("/apex/com.android.conscrypt/cacerts",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(certs.ok());
        struct statvfs fs;ASSERT_EQ(0,fstatvfs(certs.get(),&fs));ASSERT_TRUE(fs.f_flag&ST_RDONLY);
        Fd scan(openat(certs.get(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));DIR* entries=fdopendir(scan.release());ASSERT_NE(nullptr,entries);
        std::vector<std::string> names;
        while(auto* entry=readdir(entries))if(strcmp(entry->d_name,".")&&strcmp(entry->d_name,".."))names.push_back(entry->d_name);
        closedir(entries);std::sort(names.begin(),names.end());ASSERT_GE(names.size(),100u);ASSERT_LE(names.size(),300u);
        std::string pem;
        for(const auto& name:names) {
            struct stat st;ASSERT_EQ(0,fstatat(certs.get(),name.c_str(),&st,AT_SYMLINK_NOFOLLOW));
            // Verified readonly Conscrypt APEX uses the Android system owner.
            ASSERT_TRUE(S_ISREG(st.st_mode));ASSERT_EQ(1000u,st.st_uid);ASSERT_EQ(1000u,st.st_gid);ASSERT_FALSE(st.st_mode&0022);
            auto contents=Read(certs.get(),name.c_str());ASSERT_NE(std::string::npos,contents.find("-----BEGIN CERTIFICATE-----"));pem+=contents; pem+="\n";
        }
        ASSERT_LT(pem.size(),1048576u);ASSERT_EQ(0,Write(directory.get(),"ca.pem",pem));
        ca.reset(openat(directory.get(),"ca.pem",O_RDONLY|O_CLOEXEC|O_NOFOLLOW));ASSERT_TRUE(ca.ok());
        std::string official=Read(source,"etc/apt/trusted.gpg.d/debian-archive-trixie-automatic.asc");
        ASSERT_EQ(0u,official.find("-----BEGIN PGP PUBLIC KEY BLOCK-----"));
        ASSERT_EQ(0,Write(directory.get(),"key.asc",official));
        ASSERT_EQ(0,Write(directory.get(),"sources.list",std::string("deb [signed-by=/run/aegis-plan-policy/key.asc] ")+uri+" trixie main\n"));
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
    // Each admitted job consumes its detached selected view. Linux refuses
    // move_mount on the same already-detached mount after MNT_DETACH; a new
    // job reselects a freshly verified view, as the owned selection path does.
    selected.reset(aegis_base_open());ASSERT_TRUE(selected.ok());
    ASSERT_EQ(0,StartOwner(&another))<<strerror(errno);EXPECT_GT(another,job);
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

TEST(PackagePlanningProtocol, CarriesAllSixPolicyInputsAndRejectsAnExtraDescriptor) {
    using Fd=android::base::unique_fd;
    int pair[2];ASSERT_EQ(0,socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,pair));Fd first(pair[0]),second(pair[1]);
    Fd input(open("/dev/null",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(input.ok());
    int inputs[7];for(auto& fd:inputs)fd=input.get();
    aegis_planning_request sent={},received={};sent.magic=AEGIS_PLANNING_MAGIC;sent.version=AEGIS_PLANNING_VERSION;
    int before=descriptors();int outputs[AEGIS_PLANNING_INPUT_FDS];for(auto& fd:outputs)fd=-1;
    ASSERT_EQ(0,aegis_planning_send(first.get(),&sent,sizeof(sent),inputs,AEGIS_PLANNING_INPUT_FDS));
    ASSERT_EQ(0,aegis_planning_receive(second.get(),&received,sizeof(received),outputs,AEGIS_PLANNING_INPUT_FDS));
    EXPECT_EQ(0,memcmp(&sent,&received,sizeof(sent)));
    struct stat original;ASSERT_EQ(0,fstat(input.get(),&original));
    for(int fd:outputs) {
        struct stat st;EXPECT_EQ(0,fstat(fd,&st));EXPECT_EQ(original.st_dev,st.st_dev);EXPECT_EQ(original.st_ino,st.st_ino);
        EXPECT_TRUE(fcntl(fd,F_GETFD)&FD_CLOEXEC);close(fd);
    }
    EXPECT_EQ(before,descriptors());
    EXPECT_EQ(-1,aegis_planning_send(first.get(),&sent,sizeof(sent),inputs,7));EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ(-1,aegis_planning_receive(second.get(),&received,sizeof(received),outputs,AEGIS_PLANNING_INPUT_FDS));EXPECT_EQ(EAGAIN,errno);
    EXPECT_EQ(before,descriptors());
}

TEST_F(RuntimePackagePlanning, VerifiedSelectionTransfersDirectlyIntoSamePlanningJob) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());
    Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(factory.ok());
    char path[PATH_MAX];ssize_t n=readlink("/proc/self/exe",path,sizeof(path)-1);ASSERT_GT(n,0);path[n]=0;
    std::string program(path);program.resize(program.rfind('/')+1);program+="aegis-package-prepare";
    Fd prepare(open(program.c_str(),O_RDONLY|O_CLOEXEC));ASSERT_TRUE(prepare.ok());
    aegis::PackageRuntimeSelection selection;selection.requester=10;selection.serial=1234;
    selection.factory={image->original.bytes,image->original.sha256};uint64_t selected_job=0,job=0;
    ASSERT_EQ(0,aegis::BrokerPreparePackageSelection(owner,selection,request.personal,parent.get(),-1,-1,factory.get(),prepare.get(),Deadline(),&selected_job));
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

TEST_F(RuntimePackagePlanning, ReviewRequiresVerifiedSelectionAndDiesWithItsOwnedJob) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());uint64_t job=0;
    ASSERT_EQ(0,StartOwner(&job));Collected(job);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageBoundPlan review;
    EXPECT_EQ(-1,aegis::BrokerReviewPlanning(owner,10,1234,job,Deadline(),&review));EXPECT_EQ(EPERM,errno);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));
    request.create_store=true;job=0;SelectOwner(&job);ASSERT_FALSE(HasFatalFailure());Collected(job);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis::BrokerReviewPlanning(owner,10,1234,job,Deadline(),&review))<<strerror(errno);
    EXPECT_EQ(request.requester,review.reviewed.requester);EXPECT_EQ(request.serial,review.reviewed.serial);
    EXPECT_EQ(image->original.sha256,review.reviewed.source.sha256);
    EXPECT_EQ(image->original.bytes,review.reviewed.shared.bytes);
    ASSERT_EQ(2u,review.reviewed.changes.size());EXPECT_EQ(aegis::PackageInstallReason::Automatic,review.reviewed.changes[1].reason);
    ASSERT_EQ(0,aegis_broker_owner_stop_all(owner,Deadline()));
    EXPECT_EQ(-1,aegis::BrokerReviewPlanning(owner,10,1234,job,Deadline(),&review));EXPECT_EQ(ENOENT,errno);
}

TEST_F(RuntimePackagePlanning, SignedPlanPreparesExecutesAndPublishesUnderTheSameJob) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());request.create_store=true;uint64_t job=0;
    SelectOwner(&job);ASSERT_FALSE(HasFatalFailure());Collected(job);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageBoundPlan bound;ASSERT_EQ(0,aegis::BrokerReviewPlanning(owner,10,1234,job,Deadline(),&bound))<<strerror(errno);
    const auto digest=bound.preparation.execution.plan_sha256;
    Fd root(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(root.ok());
    ASSERT_EQ(0,mkdirat(root.get(),"planned-stage",0700));ASSERT_EQ(0,mkdirat(root.get(),"planned-store",0700));
    Fd stage(openat(root.get(),"planned-stage",O_RDONLY|O_DIRECTORY|O_CLOEXEC)),store(openat(root.get(),"planned-store",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    ASSERT_TRUE(stage.ok());ASSERT_TRUE(store.ok());
    Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(factory.ok());
    Fd prepare(Program("aegis-package-prepare")),execute(Program("aegis-package-execute-probe")),publish(Program("aegis-package-publish"));
    ASSERT_TRUE(prepare.ok());ASSERT_TRUE(execute.ok());ASSERT_TRUE(publish.ok());
    auto handoff=[&](const std::string& hash) {
        return aegis::BrokerPreparePlannedTransaction(owner,10,1234,job,hash,parent.get(),stage.get(),store.get(),factory.get(),
            prepare.get(),execute.get(),publish.get(),Deadline());
    };
    EXPECT_EQ(-1,handoff(std::string(64,'f')));EXPECT_EQ(ESTALE,errno);
    struct stat absent;EXPECT_EQ(-1,fstatat(stage.get(),"candidate.ext4",&absent,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    ASSERT_EQ(0,handoff(digest))<<strerror(errno);
    aegis::PackageBoundPlan unavailable;
    EXPECT_EQ(-1,aegis::BrokerReviewPlanning(owner,10,1234,job,Deadline(),&unavailable));EXPECT_EQ(ENOENT,errno);
    aegis::PublicationState state; aegis::PackageExecutionResult result;
    for(int i=0;i<400;++i) {
        ASSERT_EQ(0,aegis::BrokerPollExecution(owner,10,1234,job,digest,&state,&result));
        if(state!=aegis::PublicationState::Preparing)break;usleep(50000);
    }
    ASSERT_EQ(aegis::PublicationState::Prepared,state)<<result.error;
    EXPECT_EQ(-1,fstatat(store.get(),"current",&absent,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,aegis::BrokerStartExecution(owner,10,1234,job,std::string(64,'f'),Deadline()));EXPECT_EQ(ESTALE,errno);
    // This internal call stands in for a NEW service-side AOSP approval. It
    // does not claim the public Binder/password endpoint has been enabled.
    ASSERT_EQ(0,aegis::BrokerStartExecution(owner,10,1234,job,digest,Deadline()))<<strerror(errno);
    for(int i=0;i<600;++i) {
        ASSERT_EQ(0,aegis::BrokerPollExecution(owner,10,1234,job,digest,&state,&result));
        if(state==aegis::PublicationState::Complete)break;usleep(50000);
    }
    ASSERT_EQ(aegis::PublicationState::Complete,state);
    ASSERT_EQ(aegis::PackageExecutionOutcome::Published,result.outcome)<<result.status<<" "<<result.error;
    // Reopen via the real selector, rehash the published generation and inspect
    // its complete dpkg state. Only this disposable store is changed.
    aegis::PackageRuntimeSelection select;select.requester=10;select.serial=1234;select.job=job+100;
    select.factory={image->original.bytes,image->original.sha256};aegis::PackagePreparer* worker=nullptr;
    ASSERT_EQ(0,aegis::PackageRuntimeSelectionStart(parent.get(),store.get(),-1,factory.get(),prepare.get(),select,&worker));
    aegis::PackagePreparationResult checked;int mount=-1;
    ASSERT_EQ(0,aegis::PackagePreparerFinish(&worker,false,9000,&checked,&mount));Fd installed(mount);
    ASSERT_EQ(aegis::PackagePreparationOutcome::Prepared,checked.outcome)<<checked.error;
    EXPECT_EQ(result.generation.image_sha256,checked.generation.image_sha256);
    auto status=Read(installed.get(),"var/lib/dpkg/status");
    for(const char* name:{"aegis-probe-app","aegis-probe-lib"}) {
        auto at=status.find(std::string("Package: ")+name+"\n");ASSERT_NE(std::string::npos,at);
        auto stanza=status.substr(at,status.find("\n\n",at)-at);
        EXPECT_NE(std::string::npos,stanza.find("Status: install ok installed"));EXPECT_NE(std::string::npos,stanza.find("Version: 2\n"));
    }
    auto marks=Read(installed.get(),"var/lib/apt/extended_states");
    EXPECT_NE(std::string::npos,marks.find("Package: aegis-probe-lib\n"));
    EXPECT_EQ(std::string::npos,marks.find("Package: aegis-probe-app\n"));
    installed.reset();ASSERT_EQ(0,aegis_broker_owner_stop_all(owner,Deadline()));
    if(!HasFailure())for(const char* name:{"planned-stage","planned-store"}) {
        Fd directory(openat(root.get(),name,O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(directory.ok());
        Fd scan(openat(directory.get(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));DIR* entries=fdopendir(scan.release());ASSERT_NE(nullptr,entries);
        while(auto* entry=readdir(entries)) {
            if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
            struct stat st;ASSERT_EQ(0,fstatat(directory.get(),entry->d_name,&st,AT_SYMLINK_NOFOLLOW));
            ASSERT_TRUE(S_ISREG(st.st_mode));ASSERT_EQ(0u,st.st_uid);ASSERT_EQ(0,unlinkat(directory.get(),entry->d_name,0));
        }
        closedir(entries);directory.reset();ASSERT_EQ(0,unlinkat(root.get(),name,AT_REMOVEDIR));
    }
}
TEST(PackagePlanningProtocol, EvidenceRejectsNoncanonicalStringsAndUnusedEntries) {
    aegis::PackageResolvedPlan proof;proof.policy_sha256=std::string(64,'a');proof.initial_status_sha256=std::string(64,'b');
    proof.initial_apt_state_presence=aegis::PackageStatePresence::Absent;
    proof.repositories={{"source",std::string(64,'c'),std::string(64,'d'),2000}};
    proof.changes={{"pkg","all","","1","source",{4,std::string(64,'e')},aegis::PackageInstallReason::Manual}};
    aegis_planning_evidence wire={};ASSERT_EQ(0,aegis::planning_wire::Encode(proof,&wire));
    aegis::PackageResolvedPlan decoded;ASSERT_EQ(0,aegis::planning_wire::Decode(wire,&decoded));ASSERT_EQ(1u,decoded.changes.size());
    for(unsigned mode=0;mode<4;++mode) {
        auto bad=wire;
        if(mode==0)bad.sources[0].id[10]='x';
        if(mode==1)bad.effects[63].bytes=1;
        if(mode==2)bad.reserved=1;
        if(mode==3)memset(bad.effects[0].effect.before,'a',sizeof(bad.effects[0].effect.before));
        EXPECT_EQ(-1,aegis::planning_wire::Decode(bad,&decoded));EXPECT_EQ(EPROTO,errno);
        EXPECT_EQ(1u,decoded.changes.size());EXPECT_EQ("pkg",decoded.changes[0].name);
    }
}

TEST(PackageNetworkPolicy, RestrictsExactHostsPortsAndPublicAddresses) {
    using namespace aegis::network;
    EXPECT_TRUE(Destination("deb.debian.org",80));EXPECT_TRUE(Destination("security.debian.org",443));
    for(const char* host:{"localhost","127.0.0.1","deb.debian.org.","deb.debian.org.evil.test","DEB.DEBIAN.ORG","security.debian.org@localhost"})
        EXPECT_FALSE(Destination(host,443));
    EXPECT_FALSE(Destination("deb.debian.org",22));EXPECT_FALSE(Destination(nullptr,80));
    for(uint32_t ip:{0u,0x0a000001u,0x7f000001u,0xa9fe0101u,0xac100001u,0xac1f0001u,0xc0a80001u,0x64400001u,0x647fffffu,
                    0xc0000201u,0xc6336401u,0xcb007101u,0xe0000001u,0xffffffffu})EXPECT_FALSE(PublicAddress(ip))<<ip;
    EXPECT_TRUE(PublicAddress(0x97650484u));EXPECT_TRUE(PublicAddress(0x01010101u));
    for(const auto& row:aegis_uid_extents)EXPECT_FALSE(kAppId>=row.app_id&&kAppId<row.app_id+row.count);
}
TEST_F(RuntimePackagePlanning, OnlineRequiresPinnedHelperBeforeAnyStart) {
    request.request.internet=true;int before=descriptors();EXPECT_EQ(-1,Start());EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ(nullptr,planner);EXPECT_EQ(before,descriptors());
}
TEST_F(RuntimePackagePlanning, OnlineCancellationReapsRelayAndPlannerTogether) {
    Online("https://deb.debian.org/debian");ASSERT_FALSE(HasFatalFailure());CreateOwner();ASSERT_FALSE(HasFatalFailure());
    int before=descriptors();uint64_t job=0;ASSERT_EQ(0,StartOwner(&job))<<strerror(errno);
    ASSERT_EQ(0,aegis_broker_owner_stop_all(owner,Deadline()))<<strerror(errno);EXPECT_EQ(before,descriptors());
    aegis::PlanningState state;aegis::PackagePlanningResult result;
    EXPECT_EQ(-1,aegis::BrokerPollPlanning(owner,10,1234,job,&state,&result));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackagePlanning, OnlineRefusesAnUnapprovedDestination) {
    Online("http://127.0.0.1/debian");ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(0,Start())<<strerror(errno);
    aegis::PackagePlanningResult result;int fd=-1;
    ASSERT_EQ(0,aegis::PackagePlannerFinish(&planner,false,10000,&result,&fd))<<strerror(errno);
    EXPECT_EQ(aegis::PackagePlanningResult::Outcome::Failed,result.outcome);EXPECT_EQ(-1,fd);
    EXPECT_EQ(aegis::PackageResolverResult::Phase::Update,result.phase);EXPECT_EQ(100,result.status);
    EXPECT_FALSE(result.diagnostic.empty());EXPECT_LE(result.diagnostic.size(),8192u);
}
TEST_F(RuntimePackagePlanning, OnlineFetchesAuthenticatesAndBindsRealDebianArchive) {
    Online("https://deb.debian.org/debian");ASSERT_FALSE(HasFatalFailure());int before=descriptors();ASSERT_EQ(0,Start())<<strerror(errno);
    aegis::PackagePlanningResult result;int fd=-1;int finished=-1;
    for(unsigned i=0;i<36;++i) {
        finished=aegis::PackagePlannerFinish(&planner,false,5000,&result,&fd);
        if(finished==0)break;ASSERT_EQ(ETIMEDOUT,errno);
    }
    ASSERT_EQ(0,finished);ASSERT_EQ(aegis::PackagePlanningResult::Outcome::Collected,result.outcome)
        <<"phase="<<int(result.phase)<<" status="<<result.status<<" error="<<result.error<<"\n"<<result.diagnostic;
    ASSERT_GE(fd,0);Fd evidence(fd);EXPECT_GT(result.effects,0u);
    const auto& proof=result.evidence;ASSERT_FALSE(proof.repositories.empty());ASSERT_FALSE(proof.changes.empty());
    auto hello=std::find_if(proof.changes.begin(),proof.changes.end(),[](const auto& p){return p.name=="hello";});
    ASSERT_NE(proof.changes.end(),hello);EXPECT_FALSE(hello->after_version.empty());EXPECT_GT(hello->archive.bytes,1000u);
    EXPECT_EQ(64u,hello->archive.sha256.size());EXPECT_EQ(64u,proof.repositories[0].release_sha256.size());
    EXPECT_NE(std::string::npos,Read(fd,"update.log").find("deb.debian.org"));
    fprintf(stderr,"ONLINE_DEBIAN_RECEIPT: %s\n",Read(fd,"receipt.json").c_str());
    EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));EXPECT_EQ(automatic,Read(selected.get(),"var/lib/apt/extended_states"));
    evidence.reset();EXPECT_EQ(before,descriptors());
}

TEST_F(RuntimePackagePlanning, OnlineRejectsUntrustedTlsAuthority) {
    Online("https://deb.debian.org/debian");ASSERT_FALSE(HasFatalFailure());
    Fd directory(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(directory.ok());
    ASSERT_EQ(0,Write(directory.get(),"ca.pem","No trusted TLS certificate\n"));
    ASSERT_EQ(0,Start())<<strerror(errno);aegis::PackagePlanningResult result;int fd=-1;int finished=-1;
    for(unsigned i=0;i<12;++i) { finished=aegis::PackagePlannerFinish(&planner,false,5000,&result,&fd);if(!finished)break;ASSERT_EQ(ETIMEDOUT,errno); }
    ASSERT_EQ(0,finished);EXPECT_EQ(-1,fd);EXPECT_EQ(aegis::PackagePlanningResult::Outcome::Failed,result.outcome);
    EXPECT_EQ(aegis::PackageResolverResult::Phase::Update,result.phase);EXPECT_EQ(100,result.status);
    EXPECT_NE(std::string::npos,result.diagnostic.find("certificate"))<<result.diagnostic;
}

TEST_F(RuntimePackagePlanning, RuntimeSelectionAndPackageScopeCannotBeRepurposedByPlanning) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());
    Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(factory.ok());
    Fd prepare(Program("aegis-package-prepare"));ASSERT_TRUE(prepare.ok());
    aegis::PackageRuntimeSelection selection;selection.requester=10;selection.serial=1234;
    selection.factory={image->original.bytes,image->original.sha256};uint64_t selected_job=0,job=0;
    ASSERT_EQ(0,aegis::BrokerPrepareRuntimeSelection(owner,selection,parent.get(),-1,-1,factory.get(),prepare.get(),Deadline(),&selected_job));
    EXPECT_EQ(-1,aegis::BrokerStartPlanningFromSelection(owner,request,selected_job,parent.get(),source,sources.get(),key.get(),helper.get(),Deadline(),&job));
    EXPECT_EQ(EPERM,errno);EXPECT_EQ(0u,job);
    ASSERT_EQ(0,aegis_broker_owner_stop_all(owner,Deadline()));selected_job=0;
    EXPECT_EQ(-1,aegis::BrokerPreparePackageSelection(owner,selection,false,parent.get(),-1,selected.get(),factory.get(),prepare.get(),Deadline(),&selected_job));
    EXPECT_EQ(EINVAL,errno);EXPECT_EQ(0u,selected_job);
    ASSERT_EQ(0,aegis::BrokerPreparePackageSelection(owner,selection,true,parent.get(),-1,-1,factory.get(),prepare.get(),Deadline(),&selected_job));
    // Scope is bound even when the private store is absent and the selected
    // content would otherwise be identical to the shared factory generation.
    request.personal=false;
    EXPECT_EQ(-1,aegis::BrokerStartPlanningFromSelection(owner,request,selected_job,parent.get(),source,sources.get(),key.get(),helper.get(),Deadline(),&job));
    EXPECT_EQ(ESTALE,errno);EXPECT_EQ(0u,job);
    ASSERT_EQ(0,aegis::BrokerCancelPackageSelection(owner,10,1234,selected_job,Deadline()));
}
TEST_F(RuntimePackagePlanning, SelectionAndPlannerPreserveAnExistingRuntimeNamespaceForTheSameUser) {
    // An actual inert namespace in the runtime leaf, no AOSP CE/users changed.
    ASSERT_EQ(0,aegis_memory_group_create(parent.get(),10,1234,&group));
    int pair[2];ASSERT_EQ(0,socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,pair));
    peers[0]=pair[0];Fd peer(pair[1]);
    ASSERT_EQ(0,aegis_namespace_create_limited(10,1234,setup,peer.get(),group,&contexts[0]));peer.reset();
    CreateOwner();ASSERT_FALSE(HasFatalFailure());uint64_t job=0;
    SelectOwner(&job);ASSERT_FALSE(HasFatalFailure());Collected(job);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));
    EXPECT_NE(std::string::npos,Read(parent.get(),"u10-s1234/cgroup.events").find("populated 1\n"));
    EXPECT_EQ("1073741824\n",Read(parent.get(),"u10-s1234/memory.max"));
    struct stat absent;EXPECT_EQ(-1,fstatat(parent.get(),"p10-s1234",&absent,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    ASSERT_EQ(0,aegis_namespace_stop(contexts[0]));aegis_child_exit exited={};
    ASSERT_EQ(0,aegis_namespace_wait(contexts[0],5000,&exited));
    EXPECT_EQ(CLD_KILLED,exited.code);EXPECT_EQ(SIGKILL,exited.status);
}

TEST_F(RuntimePackagePlanning, ProductPolicySealsPinnedTrustAndAuthenticatesDebianUpdateSources) {
    int inputs[3]={-1,-1,-1};ASSERT_EQ(0,aegis_package_policy_open(source,inputs))<<strerror(errno);
    sources.reset(inputs[0]);key.reset(inputs[1]);ca.reset(inputs[2]);
    for(int fd:inputs) {
        EXPECT_EQ(0,aegis_package_policy_file(fd,1048576));
        EXPECT_EQ(O_RDONLY,fcntl(fd,F_GETFL)&O_ACCMODE);
        EXPECT_EQ(F_SEAL_WRITE|F_SEAL_GROW|F_SEAL_SHRINK|F_SEAL_SEAL,fcntl(fd,F_GET_SEALS));
        char path[64];snprintf(path,sizeof(path),"/proc/self/fd/%d",fd);Fd write(open(path,O_RDWR|O_CLOEXEC));
        ASSERT_TRUE(write.ok());EXPECT_EQ(-1,pwrite(write.get(),"x",1,0));EXPECT_EQ(EPERM,errno);
        EXPECT_EQ(-1,ftruncate(write.get(),0));EXPECT_EQ(EPERM,errno);
    }
    char configuration[1024]={};ssize_t size=pread(sources.get(),configuration,sizeof(configuration)-1,0);ASSERT_GT(size,0);
    std::string configured(configuration,size);
    EXPECT_NE(std::string::npos,configured.find("https://deb.debian.org/debian trixie main\n"));
    EXPECT_NE(std::string::npos,configured.find("https://deb.debian.org/debian trixie-updates main\n"));
    EXPECT_NE(std::string::npos,configured.find("https://security.debian.org/debian-security trixie-security main\n"));
    EXPECT_EQ(std::string::npos,configured.find("http://"));
    network.reset(Program("aegis-package-network"));ASSERT_TRUE(network.ok());
    request.request.internet=true;request.request.package="hello";request.request.version.clear();
    ASSERT_EQ(0,Start())<<strerror(errno);int fd=-1;aegis::PackagePlanningResult result;
    int finished=-1;
    for(unsigned i=0;i<36;++i) {
        finished=aegis::PackagePlannerFinish(&planner,false,5000,&result,&fd);
        if(!finished)break;ASSERT_EQ(ETIMEDOUT,errno);
    }
    ASSERT_EQ(0,finished);Fd evidence(fd);ASSERT_EQ(aegis::PackagePlanningResult::Outcome::Collected,result.outcome)
        <<result.error<<" "<<result.status<<" "<<result.diagnostic;
    EXPECT_TRUE(evidence.ok());EXPECT_GE(result.evidence.repositories.size(),3u);
    EXPECT_GE(result.effects,1u);
    const auto updates=Read(fd,"update.log");
    EXPECT_NE(std::string::npos,updates.find("trixie-updates"));
    EXPECT_NE(std::string::npos,updates.find("trixie-security"));
    fprintf(stderr,"PRODUCT_POLICY_DEBIAN_RECEIPT: %s\n",Read(fd,"receipt.json").c_str());
    EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));
    EXPECT_EQ(automatic,Read(selected.get(),"var/lib/apt/extended_states"));
}
TEST_F(RuntimePackagePlanning, ProductPolicyRejectsUnsealedInputsAndInvalidFactoryWithoutLeaking) {
    int before=descriptors();int outputs[3]={-1,-1,-1};
    EXPECT_EQ(-1,aegis_package_policy_open(-1,outputs));EXPECT_EQ(before,descriptors());
    for(int fd:outputs)EXPECT_EQ(-1,fd);
    Fd writable(syscall(SYS_memfd_create,"unsealed-package-policy",MFD_CLOEXEC|MFD_ALLOW_SEALING));ASSERT_TRUE(writable.ok());
    ASSERT_EQ(1,write(writable.get(),"x",1));ASSERT_EQ(0,fchmod(writable.get(),0400));
    char path[64];snprintf(path,sizeof(path),"/proc/self/fd/%d",writable.get());Fd readonly(open(path,O_RDONLY|O_CLOEXEC));ASSERT_TRUE(readonly.ok());
    EXPECT_EQ(-1,aegis_package_policy_file(readonly.get(),16384));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ(-1,aegis_package_policy_file(writable.get(),16384));EXPECT_EQ(EPERM,errno);
}

TEST_F(RuntimePackagePlanning, ProductPolicyBootstrapPinsTogetherOnceBeforeAnyJob) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());
    int inputs[3]={-1,-1,-1};ASSERT_EQ(0,aegis_package_policy_open(source,inputs));
    Fd first(inputs[0]),second(inputs[1]),third(inputs[2]);
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_policy(owner,inputs));EXPECT_EQ(EALREADY,errno);
    Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(factory.ok());
    Fd prepare(Program("aegis-package-prepare"));ASSERT_TRUE(prepare.ok());
    Fd state(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(state.ok());
    aegis_base_receipt receipt={};receipt.bytes=image->original.bytes;memcpy(receipt.sha256,image->original.sha256,65);
    ASSERT_EQ(0,aegis_broker_owner_enable_selection(owner,factory.get(),&receipt,prepare.get(),state.get()));
    int before=descriptors();int invalid[3]={inputs[0],inputs[1],-1};
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_policy(owner,invalid));EXPECT_EQ(before,descriptors());
    ASSERT_EQ(0,aegis_broker_owner_enable_package_policy(owner,inputs));EXPECT_EQ(before+3,descriptors());
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_policy(owner,inputs));EXPECT_EQ(EALREADY,errno);
    EXPECT_EQ(before+3,descriptors());
}
