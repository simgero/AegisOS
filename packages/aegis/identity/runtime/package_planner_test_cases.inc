// Included after RuntimePackageResolver; reuse only its inert image/cgroup fixture.
class RuntimePackagePlanning : public RuntimePackageResolver {
 protected:
    aegis::PackagePlanner* planner=nullptr;
    aegis_broker_owner* owner=nullptr;
    Fd sources,key,helper,network,ca;
    aegis::PackagePlanning request;
    bool configured_install_fixture=false;
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
        if(configured_install_fixture && image && !HasFailure()) {
            Fd state(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
            ASSERT_TRUE(state.ok());
            for(const char* name:{"shared-staging","shared-packages"})RemoveFixtureTree(state.get(),name);
        }
        if(image)for(const char* n:{"sources.list","key.asc","ca.pem"})if(unlink((image->directory+"/"+n).c_str())<0)EXPECT_EQ(ENOENT,errno);
        sources.reset();key.reset();helper.reset();network.reset();ca.reset();RuntimePackageResolver::TearDown();
    }
    int Start() { return aegis::PackagePlannerStart(parent.get(),source,selected.get(),sources.get(),key.get(),helper.get(),request,Deadline(),&planner,network.get(),ca.get()); }
    int FinishPlanBounded(aegis::PackagePlanningResult* result,int* fd) {
        // The production API allows at most 10 seconds per observation. Its
        // timeout retains worker ownership, so TCG tests may poll the SAME
        // worker within a bounded overall window instead of restarting it.
        timespec started={};if(clock_gettime(CLOCK_MONOTONIC,&started)<0)return -1;
        const uint64_t end=uint64_t(started.tv_sec)*1000000000+started.tv_nsec+UINT64_C(180000000000);
        for(;;) {
            int rc=aegis::PackagePlannerFinish(&planner,false,1000,result,fd);
            if(rc==0||errno!=ETIMEDOUT)return rc;
            timespec now={};if(clock_gettime(CLOCK_MONOTONIC,&now)<0)return -1;
            if(uint64_t(now.tv_sec)*1000000000+now.tv_nsec>=end) { errno=ETIMEDOUT;return -1; }
        }
    }
    template<class Poll> int ObserveOwned(Poll poll,int pending=EAGAIN) {
        // Only observation of an already owned job is repeated. Production
        // admission deadlines and per-call limits remain unchanged.
        timespec started={};if(clock_gettime(CLOCK_MONOTONIC,&started)<0)return -1;
        const uint64_t end=uint64_t(started.tv_sec)*1000000000+started.tv_nsec+UINT64_C(180000000000);
        for(;;) {
            const int rc=poll();if(rc==0||errno!=pending)return rc;
            timespec now={};if(clock_gettime(CLOCK_MONOTONIC,&now)<0)return -1;
            if(uint64_t(now.tv_sec)*1000000000+now.tv_nsec>=end) { errno=ETIMEDOUT;return -1; }
            usleep(50000);
        }
    }
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
    void ConfigureOwner(bool product,bool enable_helpers=true) {
        ASSERT_NE(nullptr,owner);
        Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(factory.ok());
        Fd prepare(Program("aegis-package-prepare"));ASSERT_TRUE(prepare.ok());
        Fd state(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(state.ok());
        ASSERT_EQ(0,aegis_broker_owner_enable_selection(owner,factory.get(),&image->original,prepare.get(),state.get()));
        int fixed[3]={-1,-1,-1};ASSERT_EQ(0,aegis_package_policy_open(source,fixed));
        Fd official_sources(fixed[0]),official_key(fixed[1]),official_ca(fixed[2]);
        const int policy[]={product?fixed[0]:sources.get(),product?fixed[1]:key.get(),fixed[2]};
        ASSERT_EQ(0,aegis_broker_owner_enable_package_policy(owner,policy));
        network.reset(Program("aegis-package-network"));ASSERT_TRUE(network.ok());
        if(enable_helpers)ASSERT_EQ(0,aegis_broker_owner_enable_package_planner(owner,helper.get(),network.get()));
        // Every bootstrap original is closed on return, except the explicitly
        // retained fixture helper handles. Only owner duplicates feed jobs.
    }
    // These are newly exclusive ordinary fixtures. Never create or erase a
    // live user's store. Product SELinux transitions need a full-image proof.
    void ConfigureInstallation() {
        Fd execute(Program("aegis-package-execute-probe")),publish(Program("aegis-package-publish"));
        ASSERT_TRUE(execute.ok());ASSERT_TRUE(publish.ok());
        ASSERT_EQ(0,aegis_broker_owner_enable_package_installation(owner,execute.get(),publish.get()));
        Fd state(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(state.ok());
        constexpr char label[]="u:object_r:aegis_package_shared_file:s0";
        ASSERT_EQ(0,fsetxattr(state.get(),"security.selinux",label,sizeof(label),0));
        ASSERT_EQ(0,mkdirat(state.get(),"shared-staging",0700));
        Fd staging(openat(state.get(),"shared-staging",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(staging.ok());
        constexpr char staging_label[]="u:object_r:aegis_package_staging_file:s0";
        ASSERT_EQ(0,fsetxattr(staging.get(),"security.selinux",staging_label,sizeof(staging_label),0));
        configured_install_fixture=true;
    }
    void RemoveFixtureTree(int parent_fd,const char* name) {
        Fd directory(openat(parent_fd,name,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW));
        if(!directory.ok()) { EXPECT_EQ(ENOENT,errno);return; }
        struct stat st;ASSERT_EQ(0,fstat(directory.get(),&st));ASSERT_EQ(0u,st.st_uid);
        const auto device=st.st_dev;
        Fd scan(openat(directory.get(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(scan.ok());
        DIR* entries=fdopendir(scan.release());ASSERT_NE(nullptr,entries);
        std::vector<std::string> names;
        while(auto* entry=readdir(entries))if(strcmp(entry->d_name,".")&&strcmp(entry->d_name,".."))names.push_back(entry->d_name);
        closedir(entries);
        for(const auto& child:names) {
            ASSERT_EQ(0,fstatat(directory.get(),child.c_str(),&st,AT_SYMLINK_NOFOLLOW));
            ASSERT_EQ(device,st.st_dev);ASSERT_EQ(0u,st.st_uid);
            if(S_ISDIR(st.st_mode))RemoveFixtureTree(directory.get(),child.c_str());
            else { ASSERT_TRUE(S_ISREG(st.st_mode));ASSERT_EQ(1u,st.st_nlink);ASSERT_EQ(0,unlinkat(directory.get(),child.c_str(),0)); }
            ASSERT_FALSE(HasFatalFailure());
        }
        directory.reset();ASSERT_EQ(0,unlinkat(parent_fd,name,AT_REMOVEDIR));
    }
    std::vector<std::string> StageNames() {
        Fd staging(open((image->directory+"/shared-staging").c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));
        if(!staging.ok()) { ADD_FAILURE()<<strerror(errno);return {}; }
        DIR* entries=fdopendir(staging.release());
        if(!entries) { ADD_FAILURE()<<strerror(errno);return {}; }
        std::vector<std::string> names;
        while(auto* entry=readdir(entries))if(strcmp(entry->d_name,".")&&strcmp(entry->d_name,".."))names.push_back(entry->d_name);
        closedir(entries);return names;
    }
    void ConfiguredReview(const aegis::PackageIntent& intent,uint64_t* job,aegis::PackageBoundPlan* bound) {
        ASSERT_EQ(0,aegis::BrokerBeginConfiguredPackage(owner,10,1234,intent,Deadline(),job));
        ContinueConfigured(10,1234,*job);ASSERT_FALSE(HasFatalFailure());
        CollectConfigured(10,1234,*job);ASSERT_FALSE(HasFatalFailure());
        ASSERT_EQ(0,aegis::BrokerReviewPlanning(owner,10,1234,*job,Deadline(),bound));
    }
    void Prepared(uint64_t job,const std::string& digest) {
        aegis::PublicationState state=aegis::PublicationState::Preparing;aegis::PackageExecutionResult result;
        ASSERT_EQ(0,ObserveOwned([&] {
            if(aegis::BrokerPollExecution(owner,10,1234,job,digest,&state,&result)<0)return -1;
            if(state==aegis::PublicationState::Preparing) { errno=EAGAIN;return -1; }return 0;
        }))<<strerror(errno);
        ASSERT_EQ(aegis::PublicationState::Prepared,state)<<result.error;
    }
    void Completed(uint64_t job,const std::string& digest,aegis::PackageExecutionResult* result) {
        aegis::PublicationState state=aegis::PublicationState::Preparing;
        ASSERT_EQ(0,ObserveOwned([&] {
            if(aegis::BrokerPollExecution(owner,10,1234,job,digest,&state,result)<0)return -1;
            if(state!=aegis::PublicationState::Complete) { errno=EAGAIN;return -1; }return 0;
        }))<<strerror(errno);
        ASSERT_EQ(aegis::PublicationState::Complete,state);
    }
    void ContinueConfigured(uint32_t user,uint32_t serial,uint64_t job) {
        int started=-1;
        for(unsigned i=0;i<200;++i) {
            started=aegis::BrokerContinueConfiguredPackagePlanning(owner,user,serial,job,Deadline());
            if(started==0)break;ASSERT_EQ(EAGAIN,errno)<<strerror(errno);usleep(50000);
        }
        ASSERT_EQ(0,started);
    }
    void CollectConfigured(uint32_t user,uint32_t serial,uint64_t job) {
        aegis::PlanningState state=aegis::PlanningState::Running;aegis::PackagePlanningResult result;
        for(unsigned i=0;i<2000;++i) {
            ASSERT_EQ(0,aegis::BrokerPollPlanning(owner,user,serial,job,&state,&result));
            if(state!=aegis::PlanningState::Running)break;usleep(50000);
        }
        ASSERT_EQ(aegis::PlanningState::Collected,state)<<result.error<<" "<<result.status<<" "<<result.diagnostic;
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
    ASSERT_EQ(0,FinishPlanBounded(&result,&fd))<<strerror(errno);
    ASSERT_EQ(aegis::PackagePlanningResult::Outcome::Collected,result.outcome)<<result.error<<" worker status="<<result.status;
    ASSERT_GE(fd,0);Fd evidence(fd);EXPECT_EQ(2u,result.effects);
    auto receipt=Read(fd,"receipt.json");EXPECT_NE(std::string::npos,receipt.find(Hash(initial)));
    EXPECT_NE(std::string::npos,Read(fd,"simulation.json").find("aegis-probe-app"));
    EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));EXPECT_EQ(automatic,Read(selected.get(),"var/lib/apt/extended_states"));
    evidence.reset();EXPECT_EQ(before,descriptors());
}
TEST_F(RuntimePackagePlanning, ExplicitOlderVersionSelectsMatchingDependencyDespiteNewerCandidate) {
    // The signed fixture publishes versions 1 and 2 of both packages. The
    // requested app=1 has an exact lib=1 dependency, even though 2 is the
    // repository candidate. This is an ordinary private install, not rebase.
    request.personal=true;request.request.version="1";
    const int before=descriptors();ASSERT_EQ(0,Start())<<strerror(errno);
    aegis::PackagePlanningResult result;int fd=-1;
    ASSERT_EQ(0,FinishPlanBounded(&result,&fd));
    Fd evidence(fd);
    ASSERT_EQ(aegis::PackagePlanningResult::Outcome::Collected,result.outcome)
        <<result.error<<" worker status="<<result.status<<" diagnostic="<<result.diagnostic;
    ASSERT_TRUE(evidence.ok());ASSERT_EQ(2u,result.evidence.changes.size());
    EXPECT_EQ(2u,result.effects);
    const auto& app=result.evidence.changes[0];
    const auto& lib=result.evidence.changes[1];
    EXPECT_EQ("aegis-probe-app",app.name);EXPECT_EQ("aegis-probe-lib",lib.name);
    for(const auto& change:result.evidence.changes) {
        EXPECT_TRUE(change.before_version.empty());EXPECT_EQ("1",change.after_version);
        EXPECT_EQ("all",change.architecture);EXPECT_GT(change.archive.bytes,0u);
        EXPECT_EQ(64u,change.archive.sha256.size());
    }
    EXPECT_EQ(aegis::PackageInstallReason::Manual,app.reason);
    EXPECT_EQ(aegis::PackageInstallReason::Automatic,lib.reason);
    EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));
    EXPECT_EQ(automatic,Read(selected.get(),"var/lib/apt/extended_states"));
    evidence.reset();EXPECT_EQ(before,descriptors());
}
TEST_F(RuntimePackagePlanning, ExplicitDowngradeDownloadsExactOlderAppAndDependencyWithoutChangingInstalledState) {
    mount_attr writable={};writable.attr_clr=MOUNT_ATTR_RDONLY;
    ASSERT_EQ(0,syscall(SYS_mount_setattr,selected.get(),"",AT_EMPTY_PATH,&writable,sizeof(writable)));
    initial+="\nPackage: aegis-probe-app\nStatus: install ok installed\nArchitecture: all\nVersion: 2\nDepends: aegis-probe-lib (= 2)\nDescription: installed newer application\n\n"
             "Package: aegis-probe-lib\nStatus: install ok installed\nArchitecture: all\nVersion: 2\nDescription: installed newer dependency\n\n";
    automatic="Package: aegis-probe-lib\nArchitecture: all\nAuto-Installed: 1\n\n";
    ASSERT_EQ(0,Write(selected.get(),"var/lib/dpkg/status",initial));
    ASSERT_EQ(0,Write(selected.get(),"var/lib/apt/extended_states",automatic));
    mount_attr readonly={};readonly.attr_set=MOUNT_ATTR_RDONLY;
    ASSERT_EQ(0,syscall(SYS_mount_setattr,selected.get(),"",AT_EMPTY_PATH,&readonly,sizeof(readonly)));
    request.personal=true;request.request.version="1";
    ASSERT_EQ(0,Start())<<strerror(errno);
    aegis::PackagePlanningResult result;int fd=-1;
    ASSERT_EQ(0,FinishPlanBounded(&result,&fd))<<strerror(errno);Fd evidence(fd);
    ASSERT_EQ(aegis::PackagePlanningResult::Outcome::Collected,result.outcome)
        <<result.error<<" status="<<result.status<<" "<<result.diagnostic;
    ASSERT_TRUE(evidence.ok());ASSERT_EQ(2u,result.evidence.changes.size());
    EXPECT_EQ("aegis-probe-app",result.evidence.changes[0].name);
    EXPECT_EQ("aegis-probe-lib",result.evidence.changes[1].name);
    for(const auto& change:result.evidence.changes) {
        EXPECT_EQ("2",change.before_version);EXPECT_EQ("1",change.after_version);
        EXPECT_GT(change.archive.bytes,0u);EXPECT_EQ(64u,change.archive.sha256.size());
    }
    EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));
    EXPECT_EQ(automatic,Read(selected.get(),"var/lib/apt/extended_states"));
}
TEST_F(RuntimePackagePlanning, UnavailableExactVersionNeverSubstitutesNewerCandidate) {
    request.personal=true;request.request.version="99";
    ASSERT_EQ(0,Start())<<strerror(errno);
    aegis::PackagePlanningResult result;int fd=-1;
    ASSERT_EQ(0,FinishPlanBounded(&result,&fd))<<strerror(errno);Fd evidence(fd);
    EXPECT_EQ(aegis::PackagePlanningResult::Outcome::Failed,result.outcome);
    EXPECT_EQ(aegis::PackageResolverResult::Phase::Simulate,result.phase);
    EXPECT_EQ(100,result.status);EXPECT_FALSE(evidence.ok());EXPECT_TRUE(result.evidence.changes.empty());
    EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));
    EXPECT_EQ(automatic,Read(selected.get(),"var/lib/apt/extended_states"));
}
TEST_F(RuntimePackagePlanning, ReadonlyPrivateChoicesReachEvidenceWithoutChangingSelectedMetadata) {
    // This is exclusively owned synthetic storage. No real user's CE is opened.
    mount_attr writable={};writable.attr_clr=MOUNT_ATTR_RDONLY;
    ASSERT_EQ(0,syscall(SYS_mount_setattr,selected.get(),"",AT_EMPTY_PATH,&writable,sizeof(writable)));
    ASSERT_EQ(std::string::npos,initial.find("Package: aegis-probe-lib\n"));
    initial+="Package: aegis-probe-lib\nStatus: install ok installed\nArchitecture: arm64\nVersion: 1\n\n";
    const std::string choices="AEGIS-PRIVATE-CHOICES1\naegis-probe-lib\tarm64\t1\n";
    ASSERT_EQ(0,Write(selected.get(),"var/lib/dpkg/status",initial));
    ASSERT_EQ(0,mkdirat(selected.get(),"var/lib/aegis",0700));
    ASSERT_EQ(0,Write(selected.get(),"var/lib/aegis/private-choices",choices));
    mount_attr readonly={};readonly.attr_set=MOUNT_ATTR_RDONLY;
    ASSERT_EQ(0,syscall(SYS_mount_setattr,selected.get(),"",AT_EMPTY_PATH,&readonly,sizeof(readonly)));
    int before=descriptors();ASSERT_EQ(0,Start())<<strerror(errno);
    aegis::PackagePlanningResult result;int fd=-1;
    ASSERT_EQ(0,aegis::PackagePlannerFinish(&planner,false,10000,&result,&fd));Fd evidence(fd);
    ASSERT_EQ(aegis::PackagePlanningResult::Outcome::Collected,result.outcome)<<result.error<<" status="<<result.status;
    ASSERT_TRUE(evidence.ok());EXPECT_EQ(choices,result.evidence.initial_private_choices);
    EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));
    EXPECT_EQ(choices,Read(selected.get(),"var/lib/aegis/private-choices"));
    EXPECT_EQ(automatic,Read(selected.get(),"var/lib/apt/extended_states"));
    evidence.reset();EXPECT_EQ(before,descriptors());
}
TEST_F(RuntimePackagePlanning, AlreadyInstalledVersionReturnsPrivateSelectionEvidenceWithoutArchiveOrMutation) {
    mount_attr writable={};writable.attr_clr=MOUNT_ATTR_RDONLY;
    ASSERT_EQ(0,syscall(SYS_mount_setattr,selected.get(),"",AT_EMPTY_PATH,&writable,sizeof(writable)));
    ASSERT_EQ(std::string::npos,initial.find("Package: aegis-probe-lib\n"));
    // The signed fixture archive is Architecture: all. A native-only status
    // record would request an architecture replacement, not an unchanged choice.
    initial+="Package: aegis-probe-lib\nStatus: install ok installed\nArchitecture: all\nVersion: 2\n\n";
    ASSERT_EQ(0,Write(selected.get(),"var/lib/dpkg/status",initial));
    mount_attr readonly={};readonly.attr_set=MOUNT_ATTR_RDONLY;
    ASSERT_EQ(0,syscall(SYS_mount_setattr,selected.get(),"",AT_EMPTY_PATH,&readonly,sizeof(readonly)));
    request.personal=true;request.request.package="aegis-probe-lib";request.request.version="2";
    int before=descriptors();ASSERT_EQ(0,Start())<<strerror(errno);
    aegis::PackagePlanningResult result;int fd=-1;
    ASSERT_EQ(0,aegis::PackagePlannerFinish(&planner,false,10000,&result,&fd));Fd evidence(fd);
    ASSERT_EQ(aegis::PackagePlanningResult::Outcome::Collected,result.outcome)<<result.error<<" status="<<result.status<<" phase="<<int(result.phase)<<" "<<result.diagnostic;
    ASSERT_TRUE(evidence.ok());fprintf(stderr,"Same-version APT hook: %s\n",Read(fd,"simulation.json").c_str());EXPECT_EQ(1u,result.effects);ASSERT_EQ(1u,result.evidence.changes.size());
    const auto& c=result.evidence.changes[0];EXPECT_EQ("aegis-probe-lib",c.name);EXPECT_EQ("all",c.architecture);
    EXPECT_EQ("2",c.before_version);EXPECT_EQ("2",c.after_version);EXPECT_EQ(aegis::PackageInstallReason::Manual,c.reason);
    EXPECT_EQ(0u,c.archive.bytes);EXPECT_TRUE(c.repository.empty());
    EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));EXPECT_EQ(automatic,Read(selected.get(),"var/lib/apt/extended_states"));
    auto plan=result.evidence;plan.requester=10;plan.serial=1234;plan.personal=true;plan.create_store=true;
    plan.source=plan.shared={image->original.bytes,image->original.sha256};plan.planner_image_sha256=image->original.sha256;
    plan.action=aegis::PackageAction::Install;plan.requested_package=request.request.package;plan.requested_version="2";
    aegis::PackageBoundPlan bound;ASSERT_EQ(0,aegis::PackageBindResolvedPlan(plan,time(nullptr),&bound))<<strerror(errno);
    EXPECT_EQ(AEGIS_PACKAGE_SELECTION,bound.preparation.execution.kind);EXPECT_TRUE(bound.preparation.archives.empty());
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

TEST(PackagePlanningProtocol, CarriesAllEightReconciliationInputsAndRejectsAnExtraDescriptor) {
    using Fd=android::base::unique_fd;
    int pair[2];ASSERT_EQ(0,socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,pair));Fd first(pair[0]),second(pair[1]);
    Fd input(open("/dev/null",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(input.ok());
    int inputs[9];for(auto& fd:inputs)fd=input.get();
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
    EXPECT_EQ(-1,aegis_planning_send(first.get(),&sent,sizeof(sent),inputs,9));EXPECT_EQ(EINVAL,errno);
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
        if(mode==2)bad.reconciliation=2;
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

TEST_F(RuntimePackagePlanning, ConfiguredIntentAndTrustRemainBoundAcrossSelectionAndOnlinePlanning) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(true);ASSERT_FALSE(HasFatalFailure());
    sources.reset();key.reset();helper.reset();network.reset();int before=descriptors();
    aegis::PackageIntent intent;intent.package="hello";uint64_t job=0;
    ASSERT_EQ(0,aegis::BrokerBeginConfiguredPackage(owner,10,1234,intent,Deadline(),&job));ASSERT_NE(0u,job);
    // Changing the caller object or trying the generic FD-based handoff must
    // never replace the registered intent, scope or startup policy.
    intent.action=aegis::PackageAction::Remove;intent.package="bash";intent.personal=true;
    uint64_t second=0;EXPECT_EQ(-1,aegis::BrokerBeginConfiguredPackage(owner,10,1234,intent,Deadline(),&second));
    EXPECT_EQ(EBUSY,errno);EXPECT_EQ(0u,second);
    EXPECT_EQ(-1,aegis::BrokerStartPlanningFromSelection(owner,request,job,-1,-1,-1,-1,-1,Deadline(),&second));
    EXPECT_EQ(EPERM,errno);EXPECT_EQ(0u,second);
    EXPECT_EQ(-1,aegis::BrokerContinueConfiguredPackagePlanning(owner,11,1234,job,Deadline()));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,aegis::BrokerContinueConfiguredPackagePlanning(owner,10,1235,job,Deadline()));EXPECT_EQ(ESTALE,errno);
    ContinueConfigured(10,1234,job);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,aegis::BrokerContinueConfiguredPackagePlanning(owner,10,1234,job,Deadline()));EXPECT_EQ(EALREADY,errno);
    CollectConfigured(10,1234,job);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageBoundPlan bound;ASSERT_EQ(0,aegis::BrokerReviewPlanning(owner,10,1234,job,Deadline(),&bound));
    EXPECT_EQ(aegis::PackageAction::Install,bound.reviewed.action);EXPECT_EQ("hello",bound.reviewed.requested_package);
    EXPECT_FALSE(bound.reviewed.personal);EXPECT_TRUE(bound.reviewed.create_store);EXPECT_FALSE(bound.reviewed.has_previous);
    EXPECT_EQ(image->original.sha256,bound.reviewed.source.sha256);EXPECT_EQ(image->original.sha256,bound.reviewed.planner_image_sha256);
    EXPECT_EQ(3u,bound.reviewed.repositories.size());ASSERT_EQ(1u,bound.reviewed.changes.size());
    EXPECT_EQ("hello",bound.reviewed.changes[0].name);
    EXPECT_EQ("7a917c7f44fbd3373dff0f35a0b6bdf8ef564ff90579d8b130ff52fbf33fce1f",bound.reviewed.changes[0].archive.sha256);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));EXPECT_EQ(before,descriptors());
    EXPECT_EQ(-1,aegis::BrokerContinueConfiguredPackagePlanning(owner,10,1234,job,Deadline()));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackagePlanning, ConfiguredStoreCreationDistinguishesAbsenceFromAnInitializedEmptyStore) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";uint64_t job=0;
    ASSERT_EQ(0,aegis::BrokerBeginConfiguredPackage(owner,10,1234,intent,Deadline(),&job));
    ContinueConfigured(10,1234,job);ASSERT_FALSE(HasFatalFailure());CollectConfigured(10,1234,job);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageBoundPlan absent;ASSERT_EQ(0,aegis::BrokerReviewPlanning(owner,10,1234,job,Deadline(),&absent));
    ASSERT_TRUE(absent.reviewed.create_store);ASSERT_FALSE(absent.reviewed.has_previous);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));uint64_t previous=job;job=0;
    Fd state(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(state.ok());
    ASSERT_EQ(0,mkdirat(state.get(),"shared-packages",0700));
    Fd target(openat(state.get(),"shared-packages",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));ASSERT_TRUE(target.ok());
    constexpr char label[]="u:object_r:aegis_package_shared_file:s0";
    ASSERT_EQ(0,fsetxattr(target.get(),"security.selinux",label,sizeof(label),0));
    std::unique_ptr<aegis::PackageStore> initialized(aegis::PackageStore::Open(target.get(),{false,0,0},true));ASSERT_NE(nullptr,initialized);
    initialized.reset();
    ASSERT_EQ(0,aegis::BrokerBeginConfiguredPackage(owner,10,1234,intent,Deadline(),&job));EXPECT_GT(job,previous);
    ContinueConfigured(10,1234,job);ASSERT_FALSE(HasFatalFailure());CollectConfigured(10,1234,job);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageBoundPlan present;ASSERT_EQ(0,aegis::BrokerReviewPlanning(owner,10,1234,job,Deadline(),&present));
    EXPECT_FALSE(present.reviewed.create_store);EXPECT_FALSE(present.reviewed.has_previous);
    EXPECT_EQ(absent.reviewed.source.sha256,present.reviewed.source.sha256);
    EXPECT_NE(absent.preparation.execution.plan_sha256,present.preparation.execution.plan_sha256);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));
    // A present but damaged store is not a new target or a factory fallback.
    ASSERT_EQ(0,unlinkat(target.get(),"owner",0));job=0;
    ASSERT_EQ(0,aegis::BrokerBeginConfiguredPackage(owner,10,1234,intent,Deadline(),&job));
    int continued=0;
    for(unsigned i=0;i<200;++i) {
        continued=aegis::BrokerContinueConfiguredPackagePlanning(owner,10,1234,job,Deadline());
        if(continued<0&&errno!=EAGAIN)break;usleep(50000);
    }
    EXPECT_EQ(-1,continued);EXPECT_EQ(ENOENT,errno);
    aegis::RuntimeSelectionState failed;aegis::PackagePreparationResult result;
    ASSERT_EQ(0,aegis::BrokerPollRuntimeSelection(owner,10,1234,job,&failed,&result));EXPECT_EQ(aegis::RuntimeSelectionState::Failed,failed);
    ASSERT_EQ(0,aegis::BrokerCancelPackageSelection(owner,10,1234,job,Deadline()));
    ASSERT_EQ(0,unlinkat(target.get(),"lock",0));target.reset();ASSERT_EQ(0,unlinkat(state.get(),"shared-packages",AT_REMOVEDIR));
}
TEST_F(RuntimePackagePlanning, ConfiguredPlannerHelpersArePinnedAtomicallyAndCannotBeReplaced) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());int before=descriptors();
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_planner(owner,helper.get(),helper.get()));EXPECT_EQ(EALREADY,errno);
    EXPECT_EQ(before,descriptors());ConfigureOwner(false,false);ASSERT_FALSE(HasFatalFailure());before=descriptors();
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_planner(owner,helper.get(),-1));EXPECT_EQ(before,descriptors());
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_planner(owner,helper.get(),sources.get()));EXPECT_EQ(EPERM,errno);EXPECT_EQ(before,descriptors());
    ASSERT_EQ(0,aegis_broker_owner_enable_package_planner(owner,helper.get(),network.get()));EXPECT_EQ(before+2,descriptors());
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_planner(owner,helper.get(),network.get()));EXPECT_EQ(EALREADY,errno);
    uint64_t generic=0;ASSERT_EQ(0,StartOwner(&generic));
    EXPECT_EQ(-1,aegis::BrokerContinueConfiguredPackagePlanning(owner,10,1234,generic,Deadline()));EXPECT_EQ(EPERM,errno);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,generic,Deadline()));
    aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";uint64_t job=0;
    ASSERT_EQ(0,aegis::BrokerBeginConfiguredPackage(owner,10,1234,intent,Deadline(),&job));
    ASSERT_EQ(0,aegis::BrokerCancelPackageSelection(owner,10,1234,job,Deadline()));EXPECT_EQ(before+2,descriptors());
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_planner(owner,helper.get(),network.get()));EXPECT_EQ(EALREADY,errno);
    EXPECT_EQ(-1,aegis::BrokerContinueConfiguredPackagePlanning(owner,10,1234,job,Deadline()));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackagePlanning, ConfiguredPackageRefusesUnreadyInvalidAndMissingPrivateStorage) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";uint64_t job=0;
    EXPECT_EQ(-1,aegis::BrokerBeginConfiguredPackage(owner,10,1234,intent,Deadline(),&job));EXPECT_EQ(ENOTSUP,errno);EXPECT_EQ(0u,job);
    ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());int before=descriptors();
    auto bad=intent;bad.package="--allow-unauthenticated";
    EXPECT_EQ(-1,aegis::BrokerBeginConfiguredPackage(owner,10,1234,bad,Deadline(),&job));EXPECT_EQ(0u,job);EXPECT_EQ(before,descriptors());
    bad=intent;bad.action=static_cast<aegis::PackageAction>(0);
    EXPECT_EQ(-1,aegis::BrokerBeginConfiguredPackage(owner,10,1234,bad,Deadline(),&job));EXPECT_EQ(0u,job);
    EXPECT_EQ(-1,aegis::BrokerBeginConfiguredPackage(owner,10,1234,intent,0,&job));EXPECT_EQ(ETIMEDOUT,errno);EXPECT_EQ(0u,job);
    struct stat missing;ASSERT_EQ(-1,lstat("/data/system_ce/21472",&missing));ASSERT_EQ(ENOENT,errno);
    intent.personal=true;EXPECT_EQ(-1,aegis::BrokerBeginConfiguredPackage(owner,21472,1234,intent,Deadline(),&job));
    EXPECT_EQ(ENOENT,errno);ASSERT_NE(0u,job);EXPECT_EQ(before,descriptors());
    EXPECT_EQ(-1,aegis::BrokerContinueConfiguredPackagePlanning(owner,21472,1234,job,Deadline()));EXPECT_EQ(ENOENT,errno);
    aegis::RuntimeSelectionState state;aegis::PackagePreparationResult failed;
    ASSERT_EQ(0,aegis::BrokerPollRuntimeSelection(owner,21472,1234,job,&state,&failed));EXPECT_EQ(aegis::RuntimeSelectionState::Failed,state);
    ASSERT_EQ(0,aegis::BrokerCancelPackageSelection(owner,21472,1234,job,Deadline()));EXPECT_EQ(before,descriptors());
    ASSERT_EQ(-1,lstat("/data/system_ce/21472",&missing));EXPECT_EQ(ENOENT,errno);
}

TEST_F(RuntimePackagePlanning, ConfiguredInstallPublishesAndRemovesFromTheRetainedSharedGeneration) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    ConfigureInstallation();ASSERT_FALSE(HasFatalFailure());int before=descriptors();
    Fd root(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(root.ok());
    aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";
    aegis::PackageGeneration previous;uint64_t previous_job=0;
    for(unsigned pass=0;pass<2;++pass) {
        uint64_t job=0;aegis::PackageBoundPlan bound;ConfiguredReview(intent,&job,&bound);ASSERT_FALSE(HasFatalFailure());
        EXPECT_GT(job,previous_job);previous_job=job;const auto digest=bound.preparation.execution.plan_sha256;
        EXPECT_EQ(pass==0,bound.reviewed.create_store);EXPECT_EQ(pass==1,bound.reviewed.has_previous);
        EXPECT_EQ(pass==0?std::string(image->original.sha256):previous.image_sha256,bound.reviewed.source.sha256);
        EXPECT_EQ(-1,aegis::BrokerPreparePlannedTransaction(owner,10,1234,job,digest,-1,-1,-1,-1,-1,-1,-1,Deadline()));EXPECT_EQ(EPERM,errno);
        EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredTransaction(owner,11,1234,job,Deadline()));EXPECT_EQ(ESTALE,errno);
        EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredTransaction(owner,10,1235,job,Deadline()));EXPECT_EQ(ESTALE,errno);
        EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,0));EXPECT_EQ(ETIMEDOUT,errno);
        ASSERT_EQ(0,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()))<<strerror(errno);
        EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()));EXPECT_EQ(ENOENT,errno);
        Prepared(job,digest);ASSERT_FALSE(HasFatalFailure());
        Fd store(openat(root.get(),"shared-packages",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(store.ok());
        if(pass==0) { struct stat absent;EXPECT_EQ(-1,fstatat(store.get(),"current",&absent,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno); }
        EXPECT_EQ(-1,aegis::BrokerStartExecution(owner,10,1234,job,std::string(64,'f'),Deadline()));EXPECT_EQ(ESTALE,errno);
        // Internal stand-in for a fresh AOSP approval; no public authorization claim.
        ASSERT_EQ(0,aegis::BrokerStartExecution(owner,10,1234,job,digest,Deadline()));
        aegis::ConfiguredPackageStatus public_status;
        for(unsigned i=0;i<600;++i) {
            ASSERT_EQ(0,aegis::BrokerPollConfiguredPackage(owner,10,1234,job,&public_status));
            EXPECT_EQ(intent.action,public_status.intent.action);EXPECT_EQ(intent.package,public_status.intent.package);
            EXPECT_EQ(intent.version,public_status.intent.version);EXPECT_FALSE(public_status.intent.personal);
            EXPECT_EQ(digest,public_status.plan_sha256);
            if(public_status.phase==aegis::ConfiguredPackagePhase::Complete)break;usleep(50000);
        }
        ASSERT_EQ(aegis::ConfiguredPackagePhase::Complete,public_status.phase);
        const auto result=public_status.result;
        ASSERT_EQ(aegis::PackageExecutionOutcome::Published,result.outcome)<<result.error<<" "<<result.status;
        EXPECT_TRUE(StageNames().empty());
        aegis::PackageRuntimeSelection select;select.requester=10;select.serial=1234;select.job=job+100;
        select.factory={image->original.bytes,image->original.sha256};aegis::PackagePreparer* worker=nullptr;
        Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC)),prepare(Program("aegis-package-prepare"));
        ASSERT_EQ(0,aegis::PackageRuntimeSelectionStart(parent.get(),store.get(),-1,factory.get(),prepare.get(),select,&worker));
        aegis::PackagePreparationResult checked;int mount=-1;
        ASSERT_EQ(0,aegis::PackagePreparerFinish(&worker,false,9000,&checked,&mount));Fd installed(mount);
        ASSERT_EQ(aegis::PackagePreparationOutcome::Prepared,checked.outcome)<<checked.error;
        EXPECT_EQ(aegis::PackagePreparationResult::Scope::Shared,checked.scope);
        EXPECT_EQ(result.generation.image_sha256,checked.generation.image_sha256);
        auto status=Read(installed.get(),"var/lib/dpkg/status");
        const auto at=status.find("Package: aegis-probe-app\n");
        if(pass==0) {
            ASSERT_NE(std::string::npos,at);const auto stanza=status.substr(at,status.find("\n\n",at)-at);
            EXPECT_NE(std::string::npos,stanza.find("Status: install ok installed"));EXPECT_NE(std::string::npos,stanza.find("Version: 2\n"));
            EXPECT_NE(std::string::npos,Read(installed.get(),"usr/bin/aegis-probe-app").find("app-2"));
        } else {
            if(at!=std::string::npos)EXPECT_EQ(std::string::npos,status.substr(at,status.find("\n\n",at)-at).find("Status: install ok installed"));
            EXPECT_EQ("<unavailable>",Read(installed.get(),"usr/bin/aegis-probe-app"));
            EXPECT_NE(previous.image_sha256,result.generation.image_sha256);
        }
        EXPECT_NE(std::string::npos,status.find("Package: aegis-probe-lib\n"));
        EXPECT_NE(std::string::npos,Read(installed.get(),"var/lib/apt/extended_states").find("Package: aegis-probe-lib\n"));
        previous=result.generation;intent.action=aegis::PackageAction::Remove;intent.version.clear();
    }
    root.reset();EXPECT_EQ(before,descriptors());
}
TEST_F(RuntimePackagePlanning, ConfiguredInstallationHelpersAreAtomicStartupOnlyAndCannotClaimGenericJobs) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());int before=descriptors();
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_installation(owner,helper.get(),helper.get()));EXPECT_EQ(EALREADY,errno);
    EXPECT_EQ(before,descriptors());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());before=descriptors();
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_installation(owner,helper.get(),-1));EXPECT_EQ(before,descriptors());
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_installation(owner,helper.get(),sources.get()));EXPECT_EQ(EPERM,errno);EXPECT_EQ(before,descriptors());
    ConfigureInstallation();ASSERT_FALSE(HasFatalFailure());EXPECT_EQ(before+2,descriptors());
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_installation(owner,helper.get(),helper.get()));EXPECT_EQ(EALREADY,errno);
    uint64_t job=0;ASSERT_EQ(0,StartOwner(&job));
    EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()));EXPECT_EQ(EPERM,errno);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));EXPECT_EQ(before+2,descriptors());
    EXPECT_EQ(-1,aegis_broker_owner_enable_package_installation(owner,helper.get(),helper.get()));EXPECT_EQ(EALREADY,errno);
}
TEST_F(RuntimePackagePlanning, ConfiguredPreparationRequiresInstallationBootstrapAndReviewedEvidence) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";uint64_t job=0;
    aegis::PackageBoundPlan bound;ConfiguredReview(intent,&job,&bound);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()));EXPECT_EQ(ENOTSUP,errno);
    struct stat absent;EXPECT_EQ(-1,lstat((image->directory+"/shared-packages").c_str(),&absent));EXPECT_EQ(ENOENT,errno);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));
    ASSERT_EQ(0,aegis_broker_owner_release(&owner));CreateOwner();ASSERT_FALSE(HasFatalFailure());
    ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());ConfigureInstallation();ASSERT_FALSE(HasFatalFailure());job=0;
    ASSERT_EQ(0,aegis::BrokerBeginConfiguredPackage(owner,10,1234,intent,Deadline(),&job));
    ContinueConfigured(10,1234,job);ASSERT_FALSE(HasFatalFailure());CollectConfigured(10,1234,job);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()));EXPECT_EQ(EBUSY,errno);
    EXPECT_EQ(-1,lstat((image->directory+"/shared-packages").c_str(),&absent));EXPECT_EQ(ENOENT,errno);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));
}
TEST_F(RuntimePackagePlanning, CancelledConfiguredPreparationCanReuseOnlyAPristineStoreWithAFreshStage) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    ConfigureInstallation();ASSERT_FALSE(HasFatalFailure());int before=descriptors();
    aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";
    std::vector<std::string> created;
    for(unsigned pass=0;pass<2;++pass) {
        uint64_t job=0;aegis::PackageBoundPlan bound;ConfiguredReview(intent,&job,&bound);ASSERT_FALSE(HasFatalFailure());
        EXPECT_TRUE(bound.reviewed.create_store);EXPECT_FALSE(bound.reviewed.has_previous);
        ASSERT_EQ(0,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()));
        const auto digest=bound.preparation.execution.plan_sha256;Prepared(job,digest);ASSERT_FALSE(HasFatalFailure());
        auto names=StageNames();ASSERT_EQ(1u,names.size());created.push_back(names[0]);
        ASSERT_EQ(0,aegis::BrokerCancelExecution(owner,10,1234,job,digest,Deadline()));
        aegis::PackageExecutionResult result;Completed(job,digest,&result);ASSERT_FALSE(HasFatalFailure());
        EXPECT_EQ(aegis::PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(ECANCELED,result.error);
        EXPECT_EQ(before,descriptors());EXPECT_TRUE(StageNames().empty());
    }
    Fd root(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(root.ok());
    EXPECT_EQ("<unavailable>",Read(root.get(),"shared-packages/current"));
    ASSERT_EQ(2u,created.size());EXPECT_NE(created[0],created[1]);
    for(const auto& name:created)EXPECT_EQ(0u,name.find("job-10-1234-"));
}

TEST_F(RuntimePackagePlanning, ConfiguredPreparationRetainsFailureWhenAReviewedSourceDisappears) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    ConfigureInstallation();ASSERT_FALSE(HasFatalFailure());
    Fd root(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(root.ok());
    ASSERT_EQ(0,mkdirat(root.get(),"shared-packages",0700));
    Fd target(openat(root.get(),"shared-packages",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(target.ok());
    Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC));ASSERT_TRUE(factory.ok());
    std::unique_ptr<aegis::PackageStore> store(aegis::PackageStore::Open(target.get(),{false,0,0},true));ASSERT_NE(nullptr,store);
    aegis::PackageGeneration generation={image->original.sha256,"",image->original.bytes};std::atomic_bool cancel(false);
    ASSERT_EQ(aegis::PackagePublish::Confirmed,store->Publish(nullptr,factory.get(),generation,cancel));store.reset();
    aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";uint64_t job=0;
    aegis::PackageBoundPlan bound;ConfiguredReview(intent,&job,&bound);ASSERT_FALSE(HasFatalFailure());
    ASSERT_FALSE(bound.reviewed.create_store);ASSERT_TRUE(bound.reviewed.has_previous);
    const auto name=generation.image_sha256+".image";const auto current=Read(target.get(),"current");
    ASSERT_EQ(0,renameat(target.get(),name.c_str(),target.get(),"saved.image"));int before=descriptors();
    EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,aegis::BrokerReviewPlanning(owner,10,1234,job,Deadline(),&bound));EXPECT_EQ(ENOENT,errno);
    aegis::PackageExecutionResult result;Completed(job,bound.preparation.execution.plan_sha256,&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(aegis::PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(ENOENT,result.error);
    EXPECT_LE(descriptors(),before);EXPECT_EQ(current,Read(target.get(),"current"));
    EXPECT_TRUE(StageNames().empty());
    EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()));EXPECT_EQ(ENOENT,errno);
    ASSERT_EQ(0,renameat(target.get(),"saved.image",target.get(),name.c_str()));
}

TEST_F(RuntimePackagePlanning, CleanupFailureRetainsItsJobUntilTheOriginalStageIsSafe) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    ConfigureInstallation();ASSERT_FALSE(HasFatalFailure());
    aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";
    uint64_t job=0;aegis::PackageBoundPlan bound;ConfiguredReview(intent,&job,&bound);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()));
    const auto digest=bound.preparation.execution.plan_sha256;Prepared(job,digest);ASSERT_FALSE(HasFatalFailure());
    auto names=StageNames();ASSERT_EQ(1u,names.size());
    Fd stage(open((image->directory+"/shared-staging/"+names[0]).c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(stage.ok());
    ASSERT_EQ(0,Write(stage.get(),"unexpected","preserve"));
    EXPECT_EQ(-1,aegis::BrokerCancelExecution(owner,10,1234,job,digest,Deadline()));EXPECT_EQ(ENOTEMPTY,errno);
    EXPECT_EQ("preserve",Read(stage.get(),"unexpected"));
    aegis::PublicationState state;aegis::PackageExecutionResult result;
    EXPECT_EQ(-1,aegis::BrokerPollExecution(owner,10,1234,job,digest,&state,&result));EXPECT_EQ(ENOTEMPTY,errno);
    uint64_t denied=0;EXPECT_EQ(-1,aegis::BrokerBeginConfiguredPackage(owner,10,1234,intent,Deadline(),&denied));EXPECT_EQ(EBUSY,errno);
    EXPECT_EQ(-1,aegis_broker_owner_stop_all(owner,Deadline()));EXPECT_EQ(ENOTEMPTY,errno);
    ASSERT_EQ(0,unlinkat(stage.get(),"unexpected",0));stage.reset();
    ASSERT_EQ(0,aegis::BrokerCancelExecution(owner,10,1234,job,digest,Deadline()));
    Completed(job,digest,&result);ASSERT_FALSE(HasFatalFailure());EXPECT_EQ(ECANCELED,result.error);
    EXPECT_TRUE(StageNames().empty());
}
TEST_F(RuntimePackagePlanning, StopRemovesOwnedPreparationBeforeReleasingTheRequester) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    ConfigureInstallation();ASSERT_FALSE(HasFatalFailure());int before=descriptors();
    aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";
    uint64_t job=0;aegis::PackageBoundPlan bound;ConfiguredReview(intent,&job,&bound);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()));
    Prepared(job,bound.preparation.execution.plan_sha256);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(1u,StageNames().size());ASSERT_EQ(0,aegis_broker_owner_stop_all(owner,Deadline()));
    EXPECT_TRUE(StageNames().empty());EXPECT_EQ(before,descriptors());
}

TEST_F(RuntimePackagePlanning, BackgroundCleanupFailurePreservesPublishedOutcomeAndStopOwnership) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    ConfigureInstallation();ASSERT_FALSE(HasFatalFailure());
    aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";
    uint64_t job=0;aegis::PackageBoundPlan bound;ConfiguredReview(intent,&job,&bound);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()));
    const auto digest=bound.preparation.execution.plan_sha256;Prepared(job,digest);ASSERT_FALSE(HasFatalFailure());
    auto names=StageNames();ASSERT_EQ(1u,names.size());
    Fd stage(open((image->directory+"/shared-staging/"+names[0]).c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(stage.ok());
    ASSERT_EQ(0,Write(stage.get(),"unexpected","preserve"));
    ASSERT_EQ(0,aegis::BrokerStartExecution(owner,10,1234,job,digest,Deadline()));
    int observed=0;
    for(unsigned i=0;i<600;++i) {
        if(aegis_broker_owner_reap_publications(owner)<0) { observed=errno;break; }
        usleep(50000);
    }
    ASSERT_EQ(ENOTEMPTY,observed);
    EXPECT_EQ(-1,aegis_broker_owner_stop_all(owner,Deadline()));EXPECT_EQ(ENOTEMPTY,errno);
    aegis::PublicationState state;aegis::PackageExecutionResult result;
    EXPECT_EQ(-1,aegis::BrokerPollExecution(owner,10,1234,job,digest,&state,&result));EXPECT_EQ(ENOTEMPTY,errno);
    EXPECT_EQ("preserve",Read(stage.get(),"unexpected"));
    ASSERT_EQ(0,unlinkat(stage.get(),"unexpected",0));stage.reset();
    EXPECT_EQ(-1,aegis::BrokerCancelExecution(owner,10,1234,job,digest,Deadline()));EXPECT_EQ(EALREADY,errno);
    Completed(job,digest,&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(aegis::PackageExecutionOutcome::Published,result.outcome);EXPECT_TRUE(StageNames().empty());
}

TEST_F(RuntimePackagePlanning, ConfiguredStatusRejectsInternalJobsAndWrongOwnerWithoutConsumption) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    uint64_t job=0;ASSERT_EQ(0,StartOwner(&job));
    aegis::ConfiguredPackageStatus status;status.plan_sha256="untouched";
    EXPECT_EQ(-1,aegis::BrokerPollConfiguredPackage(owner,11,1234,job,&status));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,aegis::BrokerPollConfiguredPackage(owner,10,1234,job,&status));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ("untouched",status.plan_sha256);
    EXPECT_EQ(-1,aegis::BrokerCancelConfiguredPackage(owner,10,1234,job,Deadline()));EXPECT_EQ(EPERM,errno);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));
}
TEST_F(RuntimePackagePlanning, ConfiguredSelectionHasOnePhaseIndependentCancellationIdentity) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    int before=descriptors();aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";
    aegis_broker_call call={};call.request.operation=AEGIS_BROKER_PACKAGE_BEGIN;
    call.request.user=10;call.request.serial=1234;call.request.deadline_ns=Deadline();
    call.package_action=1;call.package_scope=2;
    strcpy(call.package_name,intent.package.c_str());strcpy(call.package_version,intent.version.c_str());
    aegis_broker_package_reply wire={};
    ASSERT_EQ(0,aegis_broker_owner_package(owner,&call,&wire));
    const uint64_t job=wire.job;ASSERT_NE(0u,job);EXPECT_EQ(0u,wire.kind);EXPECT_EQ(0u,wire.bytes);
    aegis::ConfiguredPackageStatus status;status.plan_sha256="untouched";
    EXPECT_EQ(-1,aegis::BrokerPollConfiguredPackage(owner,10,1235,job,&status));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ("untouched",status.plan_sha256);
    EXPECT_EQ(-1,aegis::BrokerCancelConfiguredPackage(owner,11,1234,job,Deadline()));EXPECT_EQ(ESTALE,errno);
    ASSERT_EQ(0,aegis::BrokerPollConfiguredPackage(owner,10,1234,job,&status));
    EXPECT_TRUE(status.phase==aegis::ConfiguredPackagePhase::Selecting || status.phase==aegis::ConfiguredPackagePhase::Selected);
    EXPECT_EQ(intent.package,status.intent.package);EXPECT_EQ(intent.version,status.intent.version);
    EXPECT_FALSE(status.intent.personal);EXPECT_TRUE(status.plan_sha256.empty());
    call.command=job;call.request.operation=AEGIS_BROKER_PACKAGE_STATUS;
    ASSERT_EQ(0,aegis_broker_owner_package(owner,&call,&wire));EXPECT_EQ(job,wire.job);EXPECT_EQ(1u,wire.kind);
    Json::CharReaderBuilder builder;std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value metadata;std::string errors;const char* body=reinterpret_cast<const char*>(wire.data);
    ASSERT_TRUE(reader->parse(body,body+wire.bytes,&metadata,&errors));
    EXPECT_EQ(intent.package,metadata["package"].asString());EXPECT_EQ(intent.version,metadata["version"].asString());
    EXPECT_EQ(2,metadata["scope"].asInt());EXPECT_TRUE(metadata["generation"]["image"].asString().empty());
    call.request.operation=AEGIS_BROKER_PACKAGE_CANCEL;call.request.deadline_ns=Deadline();
    ASSERT_EQ(0,aegis_broker_owner_package(owner,&call,&wire));
    EXPECT_EQ(job,wire.job);EXPECT_EQ(0u,wire.kind);EXPECT_EQ(0u,wire.bytes);
    EXPECT_EQ(before,descriptors());
    EXPECT_EQ(-1,aegis::BrokerPollConfiguredPackage(owner,10,1234,job,&status));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,aegis::BrokerCancelConfiguredPackage(owner,10,1234,job,Deadline()));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackagePlanning, ConfiguredStatusPreservesReviewedIntentAcrossPreparationAndCancellation) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    ConfigureInstallation();ASSERT_FALSE(HasFatalFailure());
    aegis::PackageIntent intent;intent.package="aegis-probe-app";intent.version="2";
    uint64_t job=0;aegis::PackageBoundPlan bound;ConfiguredReview(intent,&job,&bound);ASSERT_FALSE(HasFatalFailure());
    aegis_broker_call call={};call.request.operation=AEGIS_BROKER_PACKAGE_REVIEW;
    call.request.user=10;call.request.serial=1234;call.request.deadline_ns=Deadline();call.command=job;
    aegis_broker_package_reply wire={};ASSERT_EQ(0,aegis_broker_owner_package(owner,&call,&wire));
    EXPECT_EQ(job,wire.job);EXPECT_EQ(2u,wire.kind);
    Json::CharReaderBuilder builder;std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    Json::Value metadata;std::string errors;const char* body=reinterpret_cast<const char*>(wire.data);
    ASSERT_TRUE(reader->parse(body,body+wire.bytes,&metadata,&errors));
    EXPECT_EQ(bound.preparation.execution.plan_sha256,metadata["digest"].asString());
    EXPECT_EQ(bound.valid_until_unix,metadata["validUntil"].asUInt64());
    EXPECT_EQ(bound.reviewed.changes.size(),metadata["changes"].size());
    for(unsigned i=0;i<metadata["changes"].size();i++) {
        const auto& original=bound.reviewed.changes[i];const auto& change=metadata["changes"][i];
        EXPECT_EQ(original.name,change["name"].asString());EXPECT_EQ(original.architecture,change["architecture"].asString());
        EXPECT_EQ(original.before_version,change["before"].asString());EXPECT_EQ(original.after_version,change["after"].asString());
        EXPECT_EQ(static_cast<unsigned>(original.reason),change["reason"].asUInt());
    }
    aegis::ConfiguredPackageStatus status;
    ASSERT_EQ(0,aegis::BrokerPollConfiguredPackage(owner,10,1234,job,&status));
    EXPECT_EQ(aegis::ConfiguredPackagePhase::Reviewed,status.phase);
    EXPECT_EQ(bound.preparation.execution.plan_sha256,status.plan_sha256);
    ASSERT_EQ(0,aegis::BrokerPrepareConfiguredTransaction(owner,10,1234,job,Deadline()));
    Prepared(job,status.plan_sha256);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis::BrokerPollConfiguredPackage(owner,10,1234,job,&status));
    EXPECT_EQ(aegis::ConfiguredPackagePhase::Prepared,status.phase);
    EXPECT_EQ(intent.action,status.intent.action);EXPECT_EQ(intent.package,status.intent.package);
    EXPECT_EQ(intent.version,status.intent.version);EXPECT_FALSE(status.intent.personal);
    EXPECT_EQ(bound.preparation.execution.plan_sha256,status.plan_sha256);
    EXPECT_EQ(bound.valid_until_unix,status.valid_until_unix);
    ASSERT_EQ(0,aegis::BrokerCancelConfiguredPackage(owner,10,1234,job,Deadline()));
    ASSERT_EQ(0,aegis::BrokerPollConfiguredPackage(owner,10,1234,job,&status));
    EXPECT_EQ(aegis::ConfiguredPackagePhase::Complete,status.phase);
    EXPECT_EQ(aegis::PackageExecutionOutcome::Failed,status.result.outcome);EXPECT_EQ(ECANCELED,status.result.error);
    EXPECT_TRUE(StageNames().empty());
    EXPECT_EQ(-1,aegis::BrokerPollConfiguredPackage(owner,10,1234,job,&status));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimePackagePlanning, ConfiguredFailedSelectionReportsItsOwnedErrorOnceWithoutRestart) {
    struct stat missing;ASSERT_EQ(-1,lstat("/data/system_ce/21472",&missing));ASSERT_EQ(ENOENT,errno);
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    const int before=descriptors();aegis::PackageIntent intent;intent.package="hello";intent.personal=true;
    uint64_t job=0;EXPECT_EQ(-1,aegis::BrokerBeginConfiguredPackage(owner,21472,1234,intent,Deadline(),&job));
    ASSERT_EQ(ENOENT,errno);ASSERT_NE(0u,job);
    aegis::ConfiguredPackageStatus status;
    ASSERT_EQ(0,aegis::BrokerPollConfiguredPackage(owner,21472,1234,job,&status));
    EXPECT_EQ(aegis::ConfiguredPackagePhase::Complete,status.phase);
    EXPECT_EQ(aegis::PackageExecutionOutcome::Failed,status.result.outcome);EXPECT_EQ(ENOENT,status.result.error);
    EXPECT_TRUE(status.intent.personal);EXPECT_EQ("hello",status.intent.package);EXPECT_TRUE(status.plan_sha256.empty());
    EXPECT_EQ(before,descriptors());
    EXPECT_EQ(-1,aegis::BrokerPollConfiguredPackage(owner,21472,1234,job,&status));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,aegis::BrokerContinueConfiguredPackagePlanning(owner,21472,1234,job,Deadline()));EXPECT_EQ(ENOENT,errno);
}

// Three independent synthetic generation views, with real immutable APT and
// signed app/lib archives. The previous selector tests establish image hashes;
// this stage proves descriptor ownership, metadata copying and solver behavior.
class RuntimeReconciliationPlanning : public RuntimePackagePlanning {
 protected:
    std::unique_ptr<AptImageFixture> old_image,new_image;
    Fd previous,current,shared_store,private_store;
    aegis::PackageGeneration old_generation,current_generation,private_generation;
    uint64_t selection_job=0;
    Fd transaction_stage,readback_view;
    aegis::PackagePreparer* readback=nullptr;
    aegis::PackageBoundPlan owner_bound;
    std::string old_status,new_status,choices;
    void Readonly(int fd,bool enabled) {
        mount_attr attr={};if(enabled)attr.attr_set=MOUNT_ATTR_RDONLY;else attr.attr_clr=MOUNT_ATTR_RDONLY;
        ASSERT_EQ(0,syscall(SYS_mount_setattr,fd,"",AT_EMPTY_PATH,&attr,sizeof(attr)));
    }
    std::string Packages(const char* version) {
        std::string v=version;
        return "\nPackage: aegis-probe-app\nStatus: install ok installed\nArchitecture: all\nMaintainer: AEGIS fixture <test@invalid>\nVersion: "+v+
            "\nDepends: aegis-probe-lib (= "+v+")\nDescription: selected app\n\nPackage: aegis-probe-lib\nStatus: install ok installed\nArchitecture: all\nMaintainer: AEGIS fixture <test@invalid>\nVersion: "+v+"\nDescription: selected library\n\n";
    }
    void Configure(const std::string& selected_choices,bool remove=false,bool conflict=false,bool downgrade=false) {
        ASSERT_EQ(std::string::npos,initial.find("Package: aegis-probe-app\n"));
        const auto base=initial;initial+=Packages(downgrade?"2":"1");old_status=initial;
        new_status=base+(remove?"":Packages(downgrade?"1":"2"));choices=selected_choices;
        old_image=std::make_unique<AptImageFixture>();new_image=std::make_unique<AptImageFixture>();
        previous.reset(old_image->create());current.reset(new_image->create());ASSERT_TRUE(previous.ok());ASSERT_TRUE(current.ok());
        Readonly(selected.get(),false);ASSERT_FALSE(HasFatalFailure());
        ASSERT_EQ(0,Write(selected.get(),"var/lib/dpkg/status",initial));
        if(!choices.empty()) {
            ASSERT_EQ(0,mkdirat(selected.get(),"var/lib/aegis",0700));
            ASSERT_EQ(0,Write(selected.get(),"var/lib/aegis/private-choices",choices));
        }
        ASSERT_EQ(0,Write(previous.get(),"var/lib/dpkg/status",old_status));
        ASSERT_EQ(0,Write(previous.get(),"var/lib/apt/extended_states",automatic));
        ASSERT_EQ(0,Write(current.get(),"var/lib/dpkg/status",new_status));
        ASSERT_EQ(0,Write(current.get(),"var/lib/apt/extended_states",conflict||remove?"":automatic));
        Readonly(selected.get(),true);Readonly(previous.get(),true);Readonly(current.get(),true);
        request.personal=true;request.request.reconciliation=true;request.request.action=aegis::PackageAction::Update;
        request.request.package.clear();request.request.version.clear();
    }
    void ConfigureAutomaticLibrary(const char* common_version) {
        const auto base=initial;
        Configure("AEGIS-PRIVATE-CHOICES1\naegis-private-choice\tall\t1\n");ASSERT_FALSE(HasFatalFailure());
        // Status-only unchanged roots need no archive. The signed repository's
        // real library archives supply the independently updated dependency.
        const std::string common="\nPackage: aegis-common-root\nStatus: install ok installed\nArchitecture: all\nVersion: 1\nDepends: aegis-probe-lib (>= 1)\nDescription: unchanged common root\n\n";
        const std::string personal="\nPackage: aegis-private-choice\nStatus: install ok installed\nArchitecture: all\nVersion: 1\nDescription: retained personal choice\n\n";
        auto library=[](const char* version) {
            return std::string("\nPackage: aegis-probe-lib\nStatus: install ok installed\nArchitecture: all\nVersion: ")+version+"\nDescription: common automatic library\n\n";
        };
        old_status=base+common+library("1");initial=old_status+personal;
        new_status=base+common+library(common_version);
        for(int fd:{selected.get(),previous.get(),current.get()})Readonly(fd,false);
        ASSERT_FALSE(HasFatalFailure());
        ASSERT_EQ(0,Write(selected.get(),"var/lib/dpkg/status",initial));
        ASSERT_EQ(0,Write(previous.get(),"var/lib/dpkg/status",old_status));
        ASSERT_EQ(0,Write(current.get(),"var/lib/dpkg/status",new_status));
        for(int fd:{selected.get(),previous.get(),current.get()})Readonly(fd,true);
    }
    void StoreImage(AptImageFixture& fixture,Fd& view,int store,aegis::PackageOwner identity,
                    const aegis::PackageGeneration* prior,const std::string& base,aegis::PackageGeneration* output) {
        Fd tree(openat(view.get(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(tree.ok());
        ASSERT_EQ(0,syncfs(tree.get()));tree.reset();view.reset();fixture.loop.reset();
        Fd file(open((fixture.directory+"/candidate.ext4").c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));ASSERT_TRUE(file.ok());
        struct stat st;ASSERT_EQ(0,fstat(file.get(),&st));SHA256_CTX hash;ASSERT_EQ(1,SHA256_Init(&hash));
        std::array<unsigned char,128*1024> data;
        for(off_t at=0;at<st.st_size;) {
            ssize_t n=TEMP_FAILURE_RETRY(pread(file.get(),data.data(),data.size(),at));ASSERT_GT(n,0);
            ASSERT_EQ(1,SHA256_Update(&hash,data.data(),n));at+=n;
        }
        unsigned char bytes[32];char text[65];ASSERT_EQ(1,SHA256_Final(bytes,&hash));
        for(unsigned i=0;i<32;i++)snprintf(text+2*i,3,"%02x",bytes[i]);
        aegis::PackageGeneration generation{text,base,static_cast<uint64_t>(st.st_size)};
        std::unique_ptr<aegis::PackageStore> owned(aegis::PackageStore::Open(store,identity,prior==nullptr));ASSERT_TRUE(owned)<<strerror(errno);
        const std::atomic_bool proceed{false};ASSERT_EQ(aegis::PackagePublish::Confirmed,owned->Publish(prior,file.get(),generation,proceed))<<strerror(errno);
        *output=generation;
    }
    void StoredViews() {
        Fd dir(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(dir.ok());
        ASSERT_EQ(0,mkdirat(dir.get(),"reconcile-shared",0700));ASSERT_EQ(0,mkdirat(dir.get(),"reconcile-private",0700));
        shared_store.reset(openat(dir.get(),"reconcile-shared",O_RDONLY|O_DIRECTORY|O_CLOEXEC));
        private_store.reset(openat(dir.get(),"reconcile-private",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(shared_store.ok());ASSERT_TRUE(private_store.ok());
        StoreImage(*old_image,previous,shared_store.get(),{false,0,0},nullptr,"",&old_generation);ASSERT_FALSE(HasFatalFailure());
        StoreImage(*new_image,current,shared_store.get(),{false,0,0},&old_generation,"",&current_generation);ASSERT_FALSE(HasFatalFailure());
        StoreImage(*image,selected,private_store.get(),{true,10,1234},nullptr,
            request.request.private_removal?current_generation.image_sha256:old_generation.image_sha256,&private_generation);ASSERT_FALSE(HasFatalFailure());
        CreateOwner();ASSERT_FALSE(HasFatalFailure());
    }
    void TransferOwner() {
        Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC)),prepare(Program("aegis-package-prepare"));ASSERT_TRUE(factory.ok());ASSERT_TRUE(prepare.ok());
        aegis::PackageRuntimeSelection input;input.requester=10;input.serial=1234;input.factory={image->original.bytes,image->original.sha256};
        ASSERT_EQ(0,request.request.private_removal
            ? aegis::BrokerPreparePrivateRemovalSelection(owner,input,parent.get(),shared_store.get(),private_store.get(),factory.get(),prepare.get(),Deadline(),&selection_job)
            : aegis::BrokerPrepareReconciliationSelection(owner,input,parent.get(),shared_store.get(),private_store.get(),factory.get(),prepare.get(),Deadline(),&selection_job));
        uint64_t same=0;
        ASSERT_EQ(0,ObserveOwned([&] {
            const int rc=aegis::BrokerStartPlanningFromSelection(owner,request,selection_job,parent.get(),source,sources.get(),key.get(),helper.get(),Deadline(),&same);
            if(rc<0&&errno==EAGAIN&&same) { errno=EPROTO;return -1; }return rc;
        }))<<strerror(errno);EXPECT_EQ(selection_job,same);
        aegis::RuntimeSelectionState state;aegis::PackagePreparationResult result;
        EXPECT_EQ(-1,aegis::BrokerPollRuntimeSelection(owner,10,1234,selection_job,&state,&result));EXPECT_EQ(ENOENT,errno);
    }
    void OwnerReview(size_t effects) {
        aegis::PlanningState state;aegis::PackagePlanningResult result;
        ASSERT_EQ(0,ObserveOwned([&] {
            if(aegis::BrokerPollPlanning(owner,10,1234,selection_job,&state,&result)<0)return -1;
            if(state==aegis::PlanningState::Running) { errno=EAGAIN;return -1; }return 0;
        }))<<strerror(errno);
        ASSERT_EQ(aegis::PlanningState::Collected,state)<<result.error<<" "<<result.status<<" "<<result.diagnostic;
        aegis::PackageBoundPlan bound;ASSERT_EQ(0,aegis::BrokerReviewPlanning(owner,10,1234,selection_job,Deadline(),&bound))<<strerror(errno);
        EXPECT_EQ(request.request.reconciliation,bound.reviewed.reconciliation);
        EXPECT_EQ(request.request.private_removal,bound.reviewed.private_removal);EXPECT_EQ(effects,bound.reviewed.changes.size());
        EXPECT_EQ(request.request.private_removal?AEGIS_PACKAGE_PRIVATE_REMOVE:AEGIS_PACKAGE_RECONCILE,bound.preparation.execution.kind);
        EXPECT_EQ(private_generation.image_sha256,bound.reviewed.source.sha256);
        EXPECT_EQ(request.request.private_removal?current_generation.image_sha256:old_generation.image_sha256,bound.reviewed.previous_shared.sha256);
        EXPECT_EQ(current_generation.image_sha256,bound.reviewed.shared.sha256);EXPECT_EQ(choices,bound.reviewed.initial_private_choices);
        EXPECT_TRUE(bound.publication.fence_shared_current);EXPECT_EQ(current_generation.image_sha256,bound.publication.expected_shared.image_sha256);
        EXPECT_EQ(bound.preparation.execution.plan_sha256,bound.publication.plan_sha256);
        auto again=bound;ASSERT_EQ(0,aegis::BrokerReviewPlanning(owner,10,1234,selection_job,Deadline(),&again));
        EXPECT_EQ(bound.preparation.execution.plan_sha256,again.preparation.execution.plan_sha256);
        EXPECT_EQ(-1,aegis::BrokerReviewConfiguredPackage(owner,10,1234,selection_job,Deadline(),&again));EXPECT_EQ(EPERM,errno);
        owner_bound=bound;
    }
    // Full source files/control records for the pre-existing fixture packages.
    // These synthetic installed images are never substituted for live CE.
    void ExecutionSource(bool corrupt_program=false) {
        Readonly(selected.get(),false);ASSERT_FALSE(HasFatalFailure());
        // Planning-only tests poison the selected APT executable deliberately.
        // A positive execution source must pass the unchanged dpkg baseline.
        const auto apt=Read(previous.get(),"usr/bin/apt-get");ASSERT_NE("<unavailable>",apt);
        ASSERT_EQ(0,Write(selected.get(),"usr/bin/apt-get",corrupt_program?"#!/bin/sh\nexit 88\n":apt));
        ASSERT_EQ(0,unlinkat(selected.get(),"etc/apt/apt.conf.d/99aegis-hostile",0));
        ASSERT_EQ(0,Write(selected.get(),"usr/bin/aegis-probe-app","#!/bin/sh\nprintf 'app-1\\n'\n"));
        ASSERT_EQ(0,fchmodat(selected.get(),"usr/bin/aegis-probe-app",0755,0));
        ASSERT_EQ(0,mkdirat(selected.get(),"usr/share/aegis-probe",0755));
        ASSERT_EQ(0,Write(selected.get(),"usr/share/aegis-probe/library","1\n"));
        ASSERT_EQ(0,Write(selected.get(),"var/lib/dpkg/info/aegis-probe-app.list","/.\n/usr\n/usr/bin\n/usr/bin/aegis-probe-app\n"));
        ASSERT_EQ(0,Write(selected.get(),"var/lib/dpkg/info/aegis-probe-lib.list","/.\n/usr\n/usr/share\n/usr/share/aegis-probe\n/usr/share/aegis-probe/library\n"));
        ASSERT_EQ(0,Write(selected.get(),"var/lib/dpkg/info/aegis-probe-app.md5sums","7864e48c45aa24e5f16d4bbfba7a9202  usr/bin/aegis-probe-app\n"));
        ASSERT_EQ(0,Write(selected.get(),"var/lib/dpkg/info/aegis-probe-lib.md5sums","b026324c6904b2a9cb4b88d6d61c81d1  usr/share/aegis-probe/library\n"));
        ASSERT_EQ(0,Write(selected.get(),"etc/aegis-reconcile-test.conf","personal=keep\n"));
        ASSERT_EQ(0,Write(selected.get(),"var/lib/aegis-reconcile-owned","technical-owner\n"));
        ASSERT_EQ(0,fchownat(selected.get(),"var/lib/aegis-reconcile-owned",42,42,AT_SYMLINK_NOFOLLOW));
        ASSERT_EQ(0,fchmodat(selected.get(),"var/lib/aegis/private-choices",0600,0));
        Readonly(selected.get(),true);ASSERT_FALSE(HasFatalFailure());
        StoredViews();ASSERT_FALSE(HasFatalFailure());
        Fd root(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(root.ok());
        ASSERT_EQ(0,mkdirat(root.get(),"reconcile-stage",0700));
        transaction_stage.reset(openat(root.get(),"reconcile-stage",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(transaction_stage.ok());
    }
    int PrepareOwned(const std::string& digest,int shared=-2) {
        Fd source_image(openat(private_store.get(),(private_generation.image_sha256+".image").c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
        Fd prepare(Program("aegis-package-prepare")),execute(Program("aegis-package-execute-probe")),publish(Program("aegis-package-publish"));
        return aegis::BrokerPreparePlannedReconciliation(owner,10,1234,selection_job,digest,parent.get(),transaction_stage.get(),
            private_store.get(),source_image.get(),prepare.get(),execute.get(),publish.get(),shared==-2?shared_store.get():shared,Deadline());
    }
    void ExecuteOwned(aegis::PackageExecutionResult* result) {
        const auto digest=owner_bound.preparation.execution.plan_sha256;
        ASSERT_EQ(0,aegis::BrokerStartExecution(owner,10,1234,selection_job,digest,Deadline()))<<strerror(errno);
        Completed(selection_job,digest,result);ASSERT_FALSE(HasFatalFailure());
    }
    void VerifySharedRemovalPublication();
    void VerifyReopened(const aegis::PackageExecutionResult& result,const std::string& version) {
        ASSERT_EQ(aegis::PackageExecutionOutcome::Published,result.outcome)<<result.error<<" "<<result.status;
        EXPECT_EQ(current_generation.image_sha256,result.generation.shared_base_sha256);
        {
            std::unique_ptr<aegis::PackageStore> own(aegis::PackageStore::Open(private_store.get(),{true,10,1234},false));ASSERT_TRUE(own);
            aegis::PackageGeneration actual;Fd found(own->Current(&actual));ASSERT_TRUE(found.ok());EXPECT_EQ(result.generation.image_sha256,actual.image_sha256);
        }
        Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC)),prepare(Program("aegis-package-prepare"));
        aegis::PackageRuntimeSelection selection;selection.requester=10;selection.serial=1234;selection.job=901;
        selection.factory={image->original.bytes,image->original.sha256};
        ASSERT_EQ(0,aegis::PackageRuntimeSelectionStart(parent.get(),shared_store.get(),private_store.get(),factory.get(),prepare.get(),selection,&readback));
        aegis::PackagePreparationResult selected_result;int fd=-1;
        ASSERT_EQ(0,ObserveOwned([&] {
            return aegis::PackagePreparerFinish(&readback,false,1000,&selected_result,&fd);
        },ETIMEDOUT))<<strerror(errno);readback_view.reset(fd);
        ASSERT_EQ(aegis::PackagePreparationOutcome::Prepared,selected_result.outcome)<<selected_result.error;ASSERT_TRUE(readback_view.ok());
        EXPECT_EQ(result.generation.image_sha256,selected_result.generation.image_sha256);
        EXPECT_EQ("#!/bin/sh\nprintf 'app-"+version+"\\n'\n",Read(readback_view.get(),"usr/bin/aegis-probe-app"));
        EXPECT_EQ(version+"\n",Read(readback_view.get(),"usr/share/aegis-probe/library"));
        EXPECT_EQ("personal=keep\n",Read(readback_view.get(),"etc/aegis-reconcile-test.conf"));
        EXPECT_EQ(choices,Read(readback_view.get(),"var/lib/aegis/private-choices"));
        struct stat st;ASSERT_EQ(0,fstatat(readback_view.get(),"var/lib/aegis-reconcile-owned",&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(42u,st.st_uid);EXPECT_EQ(42u,st.st_gid);
        aegis::PackageInstalledRegistry registry;ASSERT_EQ(0,aegis::PackageReadInstalledRegistry(Read(readback_view.get(),"var/lib/dpkg/status"),&registry));
        EXPECT_EQ(version,std::get<0>(registry.at("aegis-probe-app")));EXPECT_EQ(version,std::get<0>(registry.at("aegis-probe-lib")));
        EXPECT_EQ(owner_bound.reviewed.reconciliation_evidence.result_registry_sha256,Hash(aegis::PackageCanonicalInstalled(registry)));
        readback_view.reset();
    }
    int StartThree() {
        const int inputs[]={selected.get(),previous.get(),current.get()};
        return aegis::PackageReconciliationPlannerStart(parent.get(),source,inputs,sources.get(),key.get(),helper.get(),request,Deadline(),&planner);
    }
    void Finish(bool success,size_t effects,const char* library_version=nullptr) {
        int fd=-1;aegis::PackagePlanningResult result;
        ASSERT_EQ(0,FinishPlanBounded(&result,&fd));Fd collected(fd);
        EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));
        EXPECT_EQ(automatic,Read(selected.get(),"var/lib/apt/extended_states"));
        EXPECT_EQ(old_status,Read(previous.get(),"var/lib/dpkg/status"));
        EXPECT_EQ(new_status,Read(current.get(),"var/lib/dpkg/status"));
        if(!choices.empty())EXPECT_EQ(choices,Read(selected.get(),"var/lib/aegis/private-choices"));
        if(!success) {
            EXPECT_EQ(aegis::PackagePlanningResult::Outcome::Failed,result.outcome);EXPECT_FALSE(collected.ok());
            if(choices.empty())EXPECT_EQ(ENODATA,result.error);
            else { EXPECT_EQ(aegis::PackageResolverResult::Phase::Simulate,result.phase);EXPECT_EQ(100,result.status);EXPECT_FALSE(result.diagnostic.empty()); }
            return;
        }
        ASSERT_EQ(aegis::PackagePlanningResult::Outcome::Collected,result.outcome)<<result.error<<" "<<result.status<<" "<<result.diagnostic;
        ASSERT_TRUE(collected.ok());ASSERT_EQ(request.request.reconciliation,result.evidence.reconciliation);
        ASSERT_EQ(request.request.private_removal,result.evidence.private_removal);
        EXPECT_EQ(std::string::npos,Read(fd,"simulate.log").find("Did not understand pin type"));
        EXPECT_EQ(effects,result.evidence.changes.size());EXPECT_EQ(choices,result.evidence.initial_private_choices);
        if(library_version) {
            auto library=std::find_if(result.evidence.changes.begin(),result.evidence.changes.end(),
                [](const auto& c){return c.name=="aegis-probe-lib";});
            ASSERT_NE(result.evidence.changes.end(),library);
            aegis::PackageInstalledRegistry installed;ASSERT_EQ(0,aegis::PackageReadInstalledRegistry(initial,&installed));
            EXPECT_EQ(std::get<0>(installed.at("aegis-probe-lib")),library->before_version);EXPECT_EQ(library_version,library->after_version);
            EXPECT_EQ(aegis::PackageInstallReason::Automatic,library->reason);
        }
        EXPECT_EQ(Hash(initial),result.evidence.initial_status_sha256);
        EXPECT_EQ(Hash(automatic),result.evidence.initial_apt_state.sha256);
        const auto& re=result.evidence.reconciliation_evidence;
        EXPECT_EQ(Hash(old_status),re.previous_status_sha256);EXPECT_EQ(Hash(new_status),re.current_status_sha256);
        aegis::PackageInstalledRegistry original,expected;std::set<std::string> marks;
        ASSERT_EQ(0,aegis::PackageReadInstalledRegistry(initial,&original));
        std::vector<aegis::PackageAptEffect> changes;
        for(const auto& c:result.evidence.changes)changes.push_back({c.name,c.architecture,c.before_version,c.after_version,c.reason==aegis::PackageInstallReason::Automatic});
        ASSERT_EQ(0,aegis::PackageReconciliationProject(original,changes,re.roots,&expected,&marks));
        EXPECT_EQ(Hash(aegis::PackageCanonicalInstalled(expected)),re.result_registry_sha256);
        EXPECT_EQ(Hash(aegis::PackageCanonicalAutomatic(marks)),re.result_automatic_sha256);

        for(const char* name:{"simulate.log","simulation.json","receipt.json"})fprintf(stderr,"Reconciliation %s:\n%s\n",name,Read(fd,name).c_str());
        EXPECT_NE(std::string::npos,Read(fd,"receipt.json").find(Hash(new_status)));
        aegis::PackageBoundPlan bound;
        EXPECT_EQ(-1,aegis::PackageBindResolvedPlan(result.evidence,time(nullptr),&bound));EXPECT_EQ(EOPNOTSUPP,errno);
    }
    void TearDown() override {
        if(owner) { EXPECT_EQ(0,aegis_broker_owner_stop_all(owner,Deadline()));EXPECT_EQ(0,aegis_broker_owner_release(&owner)); }
        if(readback) { int fd=-1;aegis::PackagePreparationResult ignored;EXPECT_EQ(0,aegis::PackagePreparerFinish(&readback,true,5000,&ignored,&fd));if(fd>=0)close(fd); }
        readback_view.reset();
        if(shared_store.ok()&&!HasFailure()) {
            Fd dir(open(image->directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(dir.ok());
            if(transaction_stage.ok()) { transaction_stage.reset();RemoveFixtureTree(dir.get(),"reconcile-stage"); }
            shared_store.reset();private_store.reset();RemoveFixtureTree(dir.get(),"reconcile-shared");RemoveFixtureTree(dir.get(),"reconcile-private");
        }
        transaction_stage.reset();shared_store.reset();private_store.reset();
        if(planner) { int fd=-1;aegis::PackagePlanningResult result;EXPECT_EQ(0,aegis::PackagePlannerFinish(&planner,true,5000,&result,&fd));if(fd>=0)close(fd); }
        previous.reset();current.reset();
        if(old_image)old_image->passed=!HasFailure();if(new_image)new_image->passed=!HasFailure();old_image.reset();new_image.reset();
        RuntimePackagePlanning::TearDown();
    }
};
class RuntimePrivateRemovalPlanning : public RuntimeReconciliationPlanning {
 protected:
    void ConfigureRemoval(bool downgrade=false,bool same=false,bool absent=false,bool private_library=false) {
        const std::string version=downgrade?"2":"1";
        std::string manifest="AEGIS-PRIVATE-CHOICES1\naegis-probe-app\tall\t"+version+"\n";
        if(private_library)manifest+="aegis-probe-lib\tall\t"+version+"\n";
        Configure(manifest,absent,false,downgrade);ASSERT_FALSE(HasFatalFailure());
        for(int fd:{selected.get(),previous.get(),current.get()})Readonly(fd,false);
        ASSERT_FALSE(HasFatalFailure());
        if(same) { new_status=initial;ASSERT_EQ(0,Write(current.get(),"var/lib/dpkg/status",new_status)); }
        old_status=new_status;ASSERT_EQ(0,Write(previous.get(),"var/lib/dpkg/status",old_status));
        ASSERT_EQ(0,Write(previous.get(),"var/lib/apt/extended_states",Read(current.get(),"var/lib/apt/extended_states")));
        if(private_library) { automatic.clear();ASSERT_EQ(0,Write(selected.get(),"var/lib/apt/extended_states",automatic)); }
        for(int fd:{selected.get(),previous.get(),current.get()})Readonly(fd,true);
        request.request.reconciliation=false;request.request.private_removal=true;
        request.request.action=aegis::PackageAction::Remove;request.request.package="aegis-probe-app";
    }
    int Start() {
        const int inputs[]={selected.get(),previous.get(),current.get()};
        return aegis::PackagePrivateRemovalPlannerStart(parent.get(),source,inputs,sources.get(),key.get(),helper.get(),request,Deadline(),&planner);
    }
};
TEST_F(RuntimePrivateRemovalPlanning, ReturnsToPublishedCommonVersionAndMatchingLibrary) {
    ConfigureRemoval();ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(0,Start())<<strerror(errno);Finish(true,2,"2");
}
TEST_F(RuntimePrivateRemovalPlanning, ReturnsToOlderCommonVersionAndMatchingLibrary) {
    ConfigureRemoval(true);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(0,Start())<<strerror(errno);Finish(true,2,"1");
}
TEST_F(RuntimePrivateRemovalPlanning, SameVersionUnpinHasNoPackageEffects) {
    ConfigureRemoval(false,true);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(0,Start())<<strerror(errno);Finish(true,0);
}
TEST_F(RuntimePrivateRemovalPlanning, UnneededPrivateOnlyRootAndLibraryAreRemoved) {
    ConfigureRemoval(false,false,true);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(0,Start())<<strerror(errno);Finish(true,2);
}
TEST_F(RuntimePrivateRemovalPlanning, RemovedLibraryChoiceRemainsForAnotherExactPrivateRoot) {
    ConfigureRemoval(false,false,false,true);ASSERT_FALSE(HasFatalFailure());request.request.package="aegis-probe-lib";
    ASSERT_EQ(0,Start())<<strerror(errno);Finish(true,0);
}
TEST_F(RuntimePrivateRemovalPlanning, ConflictingRemainingPrivateLibraryIsNotSilentlyReplaced) {
    ConfigureRemoval(false,false,false,true);ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(0,Start())<<strerror(errno);Finish(false,0);
}
TEST_F(RuntimePrivateRemovalPlanning, OwnerRetainsRemovalIntentAndCurrentBaseThroughSelectionAndReview) {
    ConfigureRemoval();ASSERT_FALSE(HasFatalFailure());StoredViews();ASSERT_FALSE(HasFatalFailure());
    TransferOwner();ASSERT_FALSE(HasFatalFailure());OwnerReview(2);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(aegis::PackageAction::Remove,owner_bound.reviewed.action);
    EXPECT_EQ("aegis-probe-app",owner_bound.reviewed.requested_package);
    EXPECT_EQ(10u,owner_bound.reviewed.requester);EXPECT_EQ(1234u,owner_bound.reviewed.serial);
    EXPECT_EQ(owner_bound.publication.previous.shared_base_sha256,owner_bound.publication.candidate.shared_base_sha256);
    EXPECT_STREQ("AEGIS-PRIVATE-CHOICES1\n",owner_bound.preparation.execution.review.result_choices);
    EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredReconciliationTransaction(owner,10,1234,selection_job,Deadline()));EXPECT_EQ(EPERM,errno);
}
TEST_F(RuntimeReconciliationPlanning, SignedTriplePlanExecutesAndPublishesPreservingPrivateFilesAndOwner) {
    Configure("AEGIS-PRIVATE-CHOICES1\n");ASSERT_FALSE(HasFatalFailure());ExecutionSource();ASSERT_FALSE(HasFatalFailure());
    TransferOwner();ASSERT_FALSE(HasFatalFailure());OwnerReview(2);ASSERT_FALSE(HasFatalFailure());
    const auto digest=owner_bound.preparation.execution.plan_sha256;
    EXPECT_EQ(-1,PrepareOwned(std::string(64,'f')));EXPECT_EQ(ESTALE,errno);
    EXPECT_EQ(-1,PrepareOwned(digest,-1));EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredReconciliationTransaction(owner,10,1234,selection_job,Deadline()));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ(-1,aegis::BrokerPreparePlannedTransaction(owner,10,1234,selection_job,digest,-1,-1,-1,-1,-1,-1,-1,Deadline()));EXPECT_EQ(EPERM,errno);
    ASSERT_EQ(0,PrepareOwned(digest))<<strerror(errno);Prepared(selection_job,digest);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageBoundPlan unavailable;EXPECT_EQ(-1,aegis::BrokerReviewPlanning(owner,10,1234,selection_job,Deadline(),&unavailable));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ(-1,aegis::BrokerStartConfiguredReconciliation(owner,10,1234,selection_job,Deadline()));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ(-1,aegis::BrokerStartConfiguredPackage(owner,10,1234,selection_job,digest,Deadline()));EXPECT_EQ(EPERM,errno);
    aegis::PackageExecutionResult result;ExecuteOwned(&result);ASSERT_FALSE(HasFatalFailure());VerifyReopened(result,"2");
}
TEST_F(RuntimeReconciliationPlanning, ChangedPackagedProgramInSignedPrivateSourcePreventsPublication) {
    Configure("AEGIS-PRIVATE-CHOICES1\n");ASSERT_FALSE(HasFatalFailure());ExecutionSource(true);ASSERT_FALSE(HasFatalFailure());
    TransferOwner();ASSERT_FALSE(HasFatalFailure());OwnerReview(2);ASSERT_FALSE(HasFatalFailure());
    const auto digest=owner_bound.preparation.execution.plan_sha256;
    ASSERT_EQ(0,PrepareOwned(digest));Prepared(selection_job,digest);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageExecutionResult result;ExecuteOwned(&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(aegis::PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(EBADMSG,result.error);
    for(const auto& item:std::vector<std::pair<int,aegis::PackageGeneration>>{
            {shared_store.get(),current_generation},{private_store.get(),private_generation}}) {
        const bool personal=item.first==private_store.get();
        std::unique_ptr<aegis::PackageStore> store(aegis::PackageStore::Open(item.first,{personal,personal?10u:0u,personal?1234u:0u},false));ASSERT_TRUE(store);
        aegis::PackageGeneration actual;Fd file(store->Current(&actual));ASSERT_TRUE(file.ok());EXPECT_EQ(item.second.image_sha256,actual.image_sha256);
    }
}
TEST_F(RuntimeReconciliationPlanning, SignedPrivateVersionOverridePublishesNewBaseWithoutPackageChanges) {
    Configure("AEGIS-PRIVATE-CHOICES1\naegis-probe-app\tall\t1\n");ASSERT_FALSE(HasFatalFailure());ExecutionSource();ASSERT_FALSE(HasFatalFailure());
    TransferOwner();ASSERT_FALSE(HasFatalFailure());OwnerReview(0);ASSERT_FALSE(HasFatalFailure());
    const auto digest=owner_bound.preparation.execution.plan_sha256;ASSERT_TRUE(owner_bound.preparation.execution.items.empty());
    ASSERT_EQ(0,PrepareOwned(digest));Prepared(selection_job,digest);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageExecutionResult result;ExecuteOwned(&result);ASSERT_FALSE(HasFatalFailure());VerifyReopened(result,"1");
}
void RuntimeReconciliationPlanning::VerifySharedRemovalPublication() {
    TransferOwner();ASSERT_FALSE(HasFatalFailure());OwnerReview(2);ASSERT_FALSE(HasFatalFailure());
    for(const auto& change:owner_bound.reviewed.changes) {
        EXPECT_FALSE(change.before_version.empty());EXPECT_TRUE(change.after_version.empty());
    }
    const auto digest=owner_bound.preparation.execution.plan_sha256;
    ASSERT_EQ(0,PrepareOwned(digest));Prepared(selection_job,digest);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageExecutionResult result;ExecuteOwned(&result);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(aegis::PackageExecutionOutcome::Published,result.outcome)<<result.error<<" "<<result.status;
    EXPECT_EQ(current_generation.image_sha256,result.generation.shared_base_sha256);
    {
        std::unique_ptr<aegis::PackageStore> own(aegis::PackageStore::Open(private_store.get(),{true,10,1234},false));ASSERT_TRUE(own);
        aegis::PackageGeneration actual;Fd found(own->Current(&actual));ASSERT_TRUE(found.ok());
        EXPECT_EQ(result.generation.image_sha256,actual.image_sha256);
        std::unique_ptr<aegis::PackageStore> common(aegis::PackageStore::Open(shared_store.get(),{false,0,0},false));ASSERT_TRUE(common);
        Fd shared(common->Current(&actual));ASSERT_TRUE(shared.ok());EXPECT_EQ(current_generation.image_sha256,actual.image_sha256);
    }
    Fd factory(open("/system_ext/etc/aegis/runtime/base.ext4",O_RDONLY|O_CLOEXEC)),prepare(Program("aegis-package-prepare"));
    aegis::PackageRuntimeSelection selection;selection.requester=10;selection.serial=1234;selection.job=902;
    selection.factory={image->original.bytes,image->original.sha256};
    ASSERT_EQ(0,aegis::PackageRuntimeSelectionStart(parent.get(),shared_store.get(),private_store.get(),factory.get(),prepare.get(),selection,&readback));
    aegis::PackagePreparationResult selected_result;int fd=-1;
    ASSERT_EQ(0,ObserveOwned([&] { return aegis::PackagePreparerFinish(&readback,false,1000,&selected_result,&fd); },ETIMEDOUT))<<strerror(errno);
    readback_view.reset(fd);
    ASSERT_EQ(aegis::PackagePreparationOutcome::Prepared,selected_result.outcome)<<selected_result.error;
    ASSERT_TRUE(readback_view.ok());EXPECT_EQ(result.generation.image_sha256,selected_result.generation.image_sha256);
    aegis::PackageInstalledRegistry expected,actual;
    ASSERT_EQ(0,aegis::PackageReadInstalledRegistry(initial,&expected));
    ASSERT_EQ(1u,expected.erase("aegis-probe-app"));ASSERT_EQ(1u,expected.erase("aegis-probe-lib"));
    ASSERT_EQ(0,aegis::PackageReadInstalledRegistry(Read(fd,"var/lib/dpkg/status"),&actual));
    EXPECT_EQ(expected,actual);
    struct stat st;
    for(const char* path:{"usr/bin/aegis-probe-app","usr/share/aegis-probe/library"}) {
        EXPECT_EQ(-1,fstatat(fd,path,&st,AT_SYMLINK_NOFOLLOW));EXPECT_EQ(ENOENT,errno);
    }
    EXPECT_EQ("AEGIS-PRIVATE-CHOICES1\n",Read(fd,"var/lib/aegis/private-choices"));
    EXPECT_EQ("personal=keep\n",Read(fd,"etc/aegis-reconcile-test.conf"));
    EXPECT_EQ("technical-owner\n",Read(fd,"var/lib/aegis-reconcile-owned"));
    ASSERT_EQ(0,fstatat(fd,"var/lib/aegis-reconcile-owned",&st,AT_SYMLINK_NOFOLLOW));
    EXPECT_EQ(42u,st.st_uid);EXPECT_EQ(42u,st.st_gid);
    EXPECT_EQ(owner_bound.reviewed.reconciliation_evidence.result_registry_sha256,Hash(aegis::PackageCanonicalInstalled(actual)));
    readback_view.reset();
}
TEST_F(RuntimeReconciliationPlanning, RemovedSharedRootsExecutePublishAndReopenWithoutPrivateChoice) {
    Configure("AEGIS-PRIVATE-CHOICES1\n",true);ASSERT_FALSE(HasFatalFailure());
    ExecutionSource();ASSERT_FALSE(HasFatalFailure());
    VerifySharedRemovalPublication();
}
TEST_F(RuntimeReconciliationPlanning, RemovedSharedRootWithRetainedAutomaticCommonLibraryPublishes) {
    Configure("AEGIS-PRIVATE-CHOICES1\n",true);ASSERT_FALSE(HasFatalFailure());
    // An ordinary common 'remove app' retains its automatic library. A later
    // private rebase must collect that orphan despite its presence in the new
    // common registry and leave the common generation itself unchanged.
    const auto library=old_status.find("\nPackage: aegis-probe-lib\n");
    ASSERT_NE(std::string::npos,library);new_status+=old_status.substr(library);
    Readonly(current.get(),false);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,Write(current.get(),"var/lib/dpkg/status",new_status));
    ASSERT_EQ(0,Write(current.get(),"var/lib/apt/extended_states",
        "Package: aegis-probe-lib\nArchitecture: all\nAuto-Installed: 1\n\n"));
    ASSERT_EQ(0,mkdirat(current.get(),"usr/share/aegis-probe",0755));
    ASSERT_EQ(0,Write(current.get(),"usr/share/aegis-probe/library","1\n"));
    ASSERT_EQ(0,Write(current.get(),"var/lib/dpkg/info/aegis-probe-lib.list",
        "/.\n/usr\n/usr/share\n/usr/share/aegis-probe\n/usr/share/aegis-probe/library\n"));
    ASSERT_EQ(0,Write(current.get(),"var/lib/dpkg/info/aegis-probe-lib.md5sums",
        "b026324c6904b2a9cb4b88d6d61c81d1  usr/share/aegis-probe/library\n"));
    Readonly(current.get(),true);ASSERT_FALSE(HasFatalFailure());
    ExecutionSource();ASSERT_FALSE(HasFatalFailure());
    VerifySharedRemovalPublication();
}
TEST_F(RuntimeReconciliationPlanning, CommonSelectionChangeAfterSignedPreparationPreventsPrivatePublication) {
    Configure("AEGIS-PRIVATE-CHOICES1\n");ASSERT_FALSE(HasFatalFailure());ExecutionSource();ASSERT_FALSE(HasFatalFailure());
    TransferOwner();ASSERT_FALSE(HasFatalFailure());OwnerReview(2);ASSERT_FALSE(HasFatalFailure());
    const auto digest=owner_bound.preparation.execution.plan_sha256;
    ASSERT_EQ(0,PrepareOwned(digest));Prepared(selection_job,digest);ASSERT_FALSE(HasFatalFailure());
    {
        std::unique_ptr<aegis::PackageStore> common(aegis::PackageStore::Open(shared_store.get(),{false,0,0},false));ASSERT_TRUE(common);
        aegis::PackageGeneration old;Fd file(common->RetainedShared(old_generation.image_sha256,&old));ASSERT_TRUE(file.ok());
        const std::atomic_bool proceed{false};ASSERT_EQ(aegis::PackagePublish::Confirmed,common->Publish(&current_generation,file.get(),old,proceed));
    }
    aegis::PackageExecutionResult result;ExecuteOwned(&result);ASSERT_FALSE(HasFatalFailure());
    EXPECT_EQ(aegis::PackageExecutionOutcome::Failed,result.outcome);EXPECT_EQ(ESTALE,result.error);
    std::unique_ptr<aegis::PackageStore> own(aegis::PackageStore::Open(private_store.get(),{true,10,1234},false));ASSERT_TRUE(own);
    aegis::PackageGeneration actual;Fd unchanged(own->Current(&actual));ASSERT_TRUE(unchanged.ok());EXPECT_EQ(private_generation.image_sha256,actual.image_sha256);
}
TEST_F(RuntimeReconciliationPlanning, StopAfterSignedPreparationClosesEvidenceCandidateAndCommonFence) {
    Configure("AEGIS-PRIVATE-CHOICES1\n");ASSERT_FALSE(HasFatalFailure());ExecutionSource();ASSERT_FALSE(HasFatalFailure());const int before=descriptors();
    TransferOwner();ASSERT_FALSE(HasFatalFailure());OwnerReview(2);ASSERT_FALSE(HasFatalFailure());
    const auto digest=owner_bound.preparation.execution.plan_sha256;
    ASSERT_EQ(0,PrepareOwned(digest));Prepared(selection_job,digest);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis_broker_owner_stop_all(owner,Deadline()));EXPECT_EQ(before,descriptors());
    EXPECT_EQ(-1,aegis::BrokerStartExecution(owner,10,1234,selection_job,digest,Deadline()));EXPECT_EQ(ENOENT,errno);
    EXPECT_EQ("populated 0\nfrozen 0\n",Read(parent.get(),"cgroup.events"));
}
TEST_F(RuntimeReconciliationPlanning, OwnerTransfersVerifiedTripleIntoSignedPlanningAndBoundReview) {
    Configure("AEGIS-PRIVATE-CHOICES1\n");ASSERT_FALSE(HasFatalFailure());StoredViews();ASSERT_FALSE(HasFatalFailure());const int before=descriptors();
    TransferOwner();ASSERT_FALSE(HasFatalFailure());OwnerReview(2);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,selection_job,Deadline()));EXPECT_EQ(before,descriptors());
}
TEST_F(RuntimeReconciliationPlanning, OwnerPreservesPrivateVersionInZeroEffectReconciliationReview) {
    Configure("AEGIS-PRIVATE-CHOICES1\naegis-probe-app\tall\t1\n");ASSERT_FALSE(HasFatalFailure());StoredViews();ASSERT_FALSE(HasFatalFailure());
    TransferOwner();ASSERT_FALSE(HasFatalFailure());OwnerReview(0);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,aegis_broker_owner_stop_all(owner,Deadline()));
    aegis::PackageBoundPlan ignored;EXPECT_EQ(-1,aegis::BrokerReviewPlanning(owner,10,1234,selection_job,Deadline(),&ignored));EXPECT_EQ(ENOENT,errno);
}
TEST_F(RuntimeReconciliationPlanning, StopAfterThreeViewHandoffOwnsAndReapsTheSamePlannerJob) {
    Configure("AEGIS-PRIVATE-CHOICES1\n");ASSERT_FALSE(HasFatalFailure());StoredViews();ASSERT_FALSE(HasFatalFailure());const int before=descriptors();
    uint64_t raw=0;EXPECT_EQ(-1,StartOwner(&raw));EXPECT_EQ(EPERM,errno);EXPECT_EQ(0u,raw);EXPECT_EQ(before,descriptors());
    TransferOwner();ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(0,aegis_broker_owner_stop_all(owner,Deadline()));EXPECT_EQ(before,descriptors());
    EXPECT_EQ("populated 0\nfrozen 0\n",Read(parent.get(),"cgroup.events"));
}
TEST_F(RuntimeReconciliationPlanning, SharedUpdateResolvesNewAppAndItsDependency) {
    Configure("AEGIS-PRIVATE-CHOICES1\n");ASSERT_FALSE(HasFatalFailure());int before=descriptors();
    ASSERT_EQ(0,StartThree())<<strerror(errno);Finish(true,2);EXPECT_EQ(before,descriptors());
}
TEST_F(RuntimeReconciliationPlanning, SharedAutomaticLibraryUpdatesWithUnchangedManualRootAndPrivateChoice) {
    ConfigureAutomaticLibrary("2");ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,StartThree())<<strerror(errno);Finish(true,1,"2");
}
TEST_F(RuntimeReconciliationPlanning, NewerRepositoryLibraryWithoutSharedPublicationIsNotAdopted) {
    ConfigureAutomaticLibrary("1");ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,StartThree())<<strerror(errno);Finish(true,0);
}
TEST_F(RuntimeReconciliationPlanning, PrivateAppKeepsItsOldDependencyInsteadOfTheNewSharedVersion) {
    Configure("AEGIS-PRIVATE-CHOICES1\naegis-probe-app\tall\t1\n");ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,StartThree())<<strerror(errno);Finish(true,0);
}
TEST_F(RuntimeReconciliationPlanning, PrivateNewerAppKeepsItsDependencyOnSharedDowngrade) {
    Configure("AEGIS-PRIVATE-CHOICES1\naegis-probe-app\tall\t2\n",false,false,true);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,StartThree())<<strerror(errno);Finish(true,0);
}
TEST_F(RuntimeReconciliationPlanning, IncompatibleSharedRootAndPrivateLibraryReturnExplainedConflict) {
    Configure("AEGIS-PRIVATE-CHOICES1\naegis-probe-lib\tall\t1\n",false,true);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,StartThree())<<strerror(errno);Finish(false,0);
}
TEST_F(RuntimeReconciliationPlanning, RemovedSharedRootsAreAutoremovedWhenNoPrivateRootNeedsThem) {
    Configure("AEGIS-PRIVATE-CHOICES1\n",true);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,StartThree())<<strerror(errno);Finish(true,2);
}
TEST_F(RuntimeReconciliationPlanning, PrivateRootSurvivesSharedRemovalWithItsDependency) {
    Configure("AEGIS-PRIVATE-CHOICES1\naegis-probe-app\tall\t1\n",true);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,StartThree())<<strerror(errno);Finish(true,0);
}
TEST_F(RuntimeReconciliationPlanning, SharedExactDowngradeResolvesAndDownloadsItsMatchingDependency) {
    Configure("AEGIS-PRIVATE-CHOICES1\n",false,false,true);ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(0,StartThree())<<strerror(errno);Finish(true,2);
}
TEST_F(RuntimeReconciliationPlanning, LegacyPrivateImageWithoutIntentCannotBeSilentlyRebased) {
    Configure("");ASSERT_FALSE(HasFatalFailure());ASSERT_EQ(0,StartThree())<<strerror(errno);Finish(false,0);
}
TEST_F(RuntimeReconciliationPlanning, CancellationClosesAllThreeViewsAfterActualReap) {
    Configure("AEGIS-PRIVATE-CHOICES1\n");ASSERT_FALSE(HasFatalFailure());int before=descriptors();
    ASSERT_EQ(0,StartThree())<<strerror(errno);int fd=-1;aegis::PackagePlanningResult result;
    ASSERT_EQ(0,aegis::PackagePlannerFinish(&planner,true,10000,&result,&fd));
    EXPECT_EQ(-1,fd);EXPECT_EQ(ECANCELED,result.error);EXPECT_EQ(before,descriptors());
    EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));
}
TEST_F(RuntimeReconciliationPlanning, WrongInterfaceAndSharedScopeRefuseBeforeOwningAnyWorker) {
    Configure("AEGIS-PRIVATE-CHOICES1\n");ASSERT_FALSE(HasFatalFailure());int before=descriptors();
    EXPECT_EQ(-1,Start());EXPECT_EQ(EINVAL,errno);EXPECT_EQ(nullptr,planner);
    request.personal=false;EXPECT_EQ(-1,StartThree());EXPECT_EQ(EINVAL,errno);EXPECT_EQ(nullptr,planner);
    EXPECT_EQ(before,descriptors());
}

TEST_F(RuntimePackagePlanning, ConfiguredReconciliationRegistersBeforeMissingCeAndCannotBecomeOrdinaryPackageIntent) {
    struct stat st;ASSERT_EQ(-1,lstat("/data/system_ce/21472",&st));ASSERT_EQ(ENOENT,errno);
    CreateOwner();ASSERT_FALSE(HasFatalFailure());ConfigureOwner(false);ASSERT_FALSE(HasFatalFailure());
    const int before=descriptors();uint64_t job=0;
    EXPECT_EQ(-1,aegis::BrokerBeginConfiguredReconciliation(owner,21472,1234,Deadline(),&job));EXPECT_EQ(ENOENT,errno);ASSERT_GT(job,0u);
    aegis::RuntimeSelectionState state;aegis::PackagePreparationResult result;
    ASSERT_EQ(0,aegis::BrokerPollRuntimeSelection(owner,21472,1234,job,&state,&result));EXPECT_EQ(aegis::RuntimeSelectionState::Failed,state);EXPECT_EQ(ENOENT,result.error);
    aegis::ConfiguredPackageStatus ordinary;EXPECT_EQ(-1,aegis::BrokerPollConfiguredPackage(owner,21472,1234,job,&ordinary));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ(-1,aegis::BrokerContinueConfiguredPackagePlanning(owner,21472,1234,job,Deadline()));EXPECT_EQ(EPERM,errno);
    ASSERT_EQ(0,aegis::BrokerCancelPackageSelection(owner,21472,1234,job,Deadline()));EXPECT_EQ(before,descriptors());
    EXPECT_EQ(-1,lstat("/data/system_ce/21472",&st));EXPECT_EQ(ENOENT,errno);
}

TEST_F(RuntimePackagePlanning, OrdinarySignedPackagePlanCannotEnterReconciliationExecution) {
    CreateOwner();ASSERT_FALSE(HasFatalFailure());request.create_store=true;uint64_t job=0;
    SelectOwner(&job);ASSERT_FALSE(HasFatalFailure());Collected(job);ASSERT_FALSE(HasFatalFailure());
    aegis::PackageBoundPlan bound;ASSERT_EQ(0,aegis::BrokerReviewPlanning(owner,10,1234,job,Deadline(),&bound));
    EXPECT_EQ(-1,aegis::BrokerPreparePlannedReconciliation(owner,10,1234,job,bound.preparation.execution.plan_sha256,
        -1,-1,-1,-1,-1,-1,-1,-1,Deadline()));EXPECT_EQ(EPERM,errno);
    EXPECT_EQ(-1,aegis::BrokerPrepareConfiguredReconciliationTransaction(owner,10,1234,job,Deadline()));EXPECT_EQ(EPERM,errno);
    auto again=bound;ASSERT_EQ(0,aegis::BrokerReviewPlanning(owner,10,1234,job,Deadline(),&again));
    EXPECT_EQ(bound.preparation.execution.plan_sha256,again.preparation.execution.plan_sha256);
    ASSERT_EQ(0,aegis::BrokerCancelPlanning(owner,10,1234,job,Deadline()));
}
