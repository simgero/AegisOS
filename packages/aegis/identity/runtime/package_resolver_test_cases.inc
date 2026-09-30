// Included inside the namespace fixture to reuse its checked host namespace
// handoff. Device-only real APT tests; no active generation or AOSP user changes.
class RuntimePackageResolver : public RuntimeNamespace {
 protected:
    using Fd=android::base::unique_fd;
    std::unique_ptr<AptImageFixture> image;
    Fd selected,output,cgroups,parent;
    aegis_memory_group* group=nullptr;
    std::string group_name,initial,automatic;
    bool group_created=false;
    int Write(int dir,const char* name,const std::string& bytes,int flags=O_CREAT|O_TRUNC) {
        Fd fd(openat(dir,name,O_WRONLY|O_NOFOLLOW|O_CLOEXEC|flags,0644));if(!fd.ok())return -1;
        for(size_t at=0;at<bytes.size();) {
            ssize_t n=TEMP_FAILURE_RETRY(write(fd.get(),bytes.data()+at,bytes.size()-at));
            if(n<=0)return -1;at+=n;
        }
        return 0;
    }
    std::string Read(int dir,const char* name) {
        Fd fd(openat(dir,name,O_RDONLY|O_CLOEXEC|O_NOFOLLOW));if(!fd.ok())return "<unavailable>";
        std::string data;char buffer[8192];
        for(;;) { ssize_t n=TEMP_FAILURE_RETRY(read(fd.get(),buffer,sizeof(buffer)));
            if(n<0)return "<unavailable>";if(!n)return data;data.append(buffer,n);
            if(data.size()>(64u<<20))return "<unavailable>"; }
    }
    std::string Hash(const std::string& bytes) {
        unsigned char digest[32];char text[65];
        if(!SHA256(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),digest))return {};
        for(unsigned i=0;i<32;++i)snprintf(text+2*i,3,"%02x",digest[i]);return text;
    }
    void SetUp() override {
        RuntimeNamespace::SetUp();ASSERT_FALSE(HasFatalFailure());
        image=std::make_unique<AptImageFixture>();selected.reset(image->create());ASSERT_TRUE(selected.ok());
        source=aegis_base_open();ASSERT_GE(source,0);ASSERT_EQ(0,anchor_source());
        cgroups.reset(open("/sys/fs/cgroup",O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(cgroups.ok());
        group_name="aegis-resolver-test-"+std::to_string(getpid());
        ASSERT_EQ(0,mkdirat(cgroups.get(),group_name.c_str(),0700));group_created=true;
        parent.reset(openat(cgroups.get(),group_name.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));ASSERT_TRUE(parent.ok());
        ASSERT_EQ(0,fchown(parent.get(),0,0));ASSERT_EQ(0,fchmod(parent.get(),0700));
        ASSERT_EQ(0,Write(parent.get(),"cgroup.subtree_control","+memory\n",0));
    }
    void TearDown() override {
        RuntimeNamespace::TearDown();
        if(group) { EXPECT_EQ(0,aegis_memory_group_kill_and_wait(group,5000));EXPECT_EQ(0,aegis_memory_group_remove(&group)); }
        parent.reset();if(group_created)EXPECT_EQ(0,unlinkat(cgroups.get(),group_name.c_str(),AT_REMOVEDIR));
        output.reset();selected.reset();if(image)image->passed=!HasFailure();image.reset();
    }
    void Run(unsigned mode) {
        // This selected generation is malicious: neither its executable nor its
        // APT configuration may enter the immutable planner root.
        ASSERT_EQ(0,Write(selected.get(),"usr/bin/apt-get","#!/bin/sh\necho SELECTED_APT_EXECUTED\nexit 88\n"));
        ASSERT_EQ(0,Write(selected.get(),"etc/apt/apt.conf.d/99aegis-hostile","APT::Update::Pre-Invoke { \"exit 89\"; };\n"));
        initial=Read(selected.get(),"var/lib/dpkg/status");ASSERT_NE("<unavailable>",initial);
        if(mode>=6) {
            initial+="\nPackage: aegis-probe-app\nStatus: install ok installed\nPriority: optional\nSection: misc\nArchitecture: all\nVersion: 1\nDepends: aegis-probe-lib (= 1)\nDescription: selected app\n\nPackage: aegis-probe-lib\nStatus: install ok installed\nPriority: optional\nSection: misc\nArchitecture: all\nVersion: 1\nDescription: selected library\n\n";
            ASSERT_EQ(0,Write(selected.get(),"var/lib/dpkg/status",initial));
        }
        automatic="Package: aegis-probe-lib\nArchitecture: arm64\nAuto-Installed: 1\n\n";
        ASSERT_EQ(0,Write(selected.get(),"var/lib/apt/extended_states",automatic));
        ASSERT_EQ(0,aegis_memory_group_create(parent.get(),10,1234,&group));
        int pair[2];ASSERT_EQ(0,socketpair(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC,0,pair));
        peers[0]=pair[0];Fd child(pair[1]);
        ASSERT_EQ(0,aegis_namespace_create_limited(10,1234,setup,child.get(),group,&contexts[0]))<<strerror(errno);
        child.reset();ASSERT_EQ(0,aegis_namespace_prepare_package(contexts[0]));
        mounts[0]=aegis_namespace_base_mount(contexts[0],source);ASSERT_GE(mounts[0],0)<<strerror(errno);
        mounts[1]=aegis_namespace_devices_mount(contexts[0]);ASSERT_GE(mounts[1],0);
        ASSERT_EQ(0,aegis_namespace_map_candidate(contexts[0],selected.get()));
        ASSERT_EQ(0,aegis_namespace_resume(contexts[0]));
        aegis_namespace_probe state={};ASSERT_EQ(static_cast<ssize_t>(sizeof(state)),report(0,&state));check(state,10,true);
        ASSERT_EQ(1,send(peers[0],"L",1,MSG_NOSIGNAL));
        uint32_t request[]={0x41525051,mode};int fds[]={mounts[0],mounts[1],selected.get()};
        iovec io={request,sizeof(request)};alignas(cmsghdr) char control[CMSG_SPACE(sizeof(fds))]={};
        msghdr message={};message.msg_iov=&io;message.msg_iovlen=1;message.msg_control=control;message.msg_controllen=sizeof(control);
        auto* h=CMSG_FIRSTHDR(&message);h->cmsg_level=SOL_SOCKET;h->cmsg_type=SCM_RIGHTS;h->cmsg_len=CMSG_LEN(sizeof(fds));
        memcpy(CMSG_DATA(h),fds,sizeof(fds));ASSERT_EQ(static_cast<ssize_t>(sizeof(request)),sendmsg(peers[0],&message,MSG_NOSIGNAL));
        pollfd ready={peers[0],POLLIN,0};ASSERT_EQ(1,poll(&ready,1,90000));
        aegis_resolver_probe_reply reply={};io={&reply,sizeof(reply)};
        memset(control,0,sizeof(control));message={};message.msg_iov=&io;message.msg_iovlen=1;
        message.msg_control=control;message.msg_controllen=sizeof(control);
        ssize_t received=recvmsg(peers[0],&message,MSG_CMSG_CLOEXEC|MSG_TRUNC);unsigned count=0;
        for(auto* entry=CMSG_FIRSTHDR(&message);entry;entry=CMSG_NXTHDR(&message,entry)) {
            ASSERT_EQ(SOL_SOCKET,entry->cmsg_level);ASSERT_EQ(SCM_RIGHTS,entry->cmsg_type);
            ASSERT_GE(entry->cmsg_len,CMSG_LEN(0));
            for(size_t at=0;at<(entry->cmsg_len-CMSG_LEN(0))/sizeof(int);++at) {
                int fd;memcpy(&fd,CMSG_DATA(entry)+at*sizeof(fd),sizeof(fd));++count;
                if(count==1)output.reset(fd);else close(fd);
            }
        }
        aegis_child_exit exited={};ASSERT_EQ(0,aegis_namespace_wait(contexts[0],5000,&exited));
        ASSERT_EQ(CLD_EXITED,exited.code);ASSERT_EQ(0,exited.status);
        ASSERT_EQ(0,aegis_memory_group_kill_and_wait(group,5000));
        ASSERT_EQ(static_cast<ssize_t>(sizeof(reply)),received);ASSERT_EQ(0,message.msg_flags&(MSG_TRUNC|MSG_CTRUNC));
        ASSERT_LE(count,1u);ASSERT_EQ(0x41525052u,reply.magic);
        fprintf(stderr,"Resolver mode=%u phase=%u status=%u error=%u effects=%u\n",mode,reply.phase,reply.status,reply.error,reply.count);
        if(output.ok())for(const char* name:{"update.log","simulate.log","download.log","simulation.json","receipt.json","index-targets"})
            fprintf(stderr,"Resolver %s:\n%s\n",name,Read(output.get(),name).c_str());
        EXPECT_EQ(initial,Read(selected.get(),"var/lib/dpkg/status"));
        EXPECT_EQ(automatic,Read(selected.get(),"var/lib/apt/extended_states"));
        if(mode==5) { EXPECT_EQ(0u,reply.phase);EXPECT_EQ(EPERM,static_cast<int>(reply.error));EXPECT_FALSE(output.ok());return; }
        ASSERT_TRUE(output.ok());
        if(mode>=1&&mode<=4) {
            EXPECT_EQ(mode==4?3u:1u,reply.phase);EXPECT_EQ(100u,reply.status);EXPECT_EQ(0u,reply.error);
            auto log=Read(output.get(),mode==4?"download.log":"update.log");
            const char* reason=mode==1?"not signed":mode==2?"expired":"Hash Sum mismatch";
            EXPECT_NE(std::string::npos,log.find(reason));
            EXPECT_EQ("<unavailable>",Read(output.get(),"receipt.json"));return;
        }
        ASSERT_EQ(5u,reply.phase);ASSERT_EQ(0u,reply.status);ASSERT_EQ(0u,reply.error);ASSERT_EQ(2u,reply.count);
        std::vector<aegis::PackageAptEffect> effects;
        auto action=mode==6?aegis::PackageAction::Remove:mode==7?aegis::PackageAction::Update:aegis::PackageAction::Install;
        ASSERT_EQ(0,aegis::PackageReadAptPlan(Read(output.get(),"simulation.json"),action,
            mode==6?"aegis-probe-lib":mode==7?"":"aegis-probe-app",mode>=6?"":"2",&effects));
        ASSERT_EQ(2u,effects.size());EXPECT_EQ("aegis-probe-app",effects[0].name);EXPECT_EQ("aegis-probe-lib",effects[1].name);
        EXPECT_FALSE(effects[0].automatic);EXPECT_TRUE(effects[1].automatic);
        for(const auto& effect:effects) { EXPECT_EQ(mode>=6?"1":"",effect.before_version);EXPECT_EQ(mode==6?"":"2",effect.after_version); }
        Json::Value receipt;Json::CharReaderBuilder parser;std::string errors,bytes=Read(output.get(),"receipt.json");
        std::unique_ptr<Json::CharReader> reader(parser.newCharReader());ASSERT_TRUE(reader->parse(bytes.data(),bytes.data()+bytes.size(),&receipt,&errors));
        EXPECT_EQ(Hash(initial),receipt["initial_status_sha256"].asString());
        EXPECT_TRUE(receipt["apt_state_present"].asBool());EXPECT_EQ(Hash(automatic),receipt["initial_apt_state_sha256"].asString());
        EXPECT_EQ(automatic.size(),receipt["apt_state_bytes"].asUInt64());
        if(mode!=6)for(const auto& file:aegis_apt_metadata) {
            std::string name=file.name;
            if(name=="aegis-repo/aegis-probe-app_2_all.deb"||name=="aegis-repo/aegis-probe-lib_2_all.deb") {
                auto downloaded=Read(output.get(),("archives/"+name.substr(11)).c_str());
                EXPECT_EQ(Hash(std::string(reinterpret_cast<const char*>(file.data),file.size)),Hash(downloaded));
            }
        }
        EXPECT_NE(std::string::npos,Read(output.get(),"index-targets").find("Packages"));
    }
};
TEST_F(RuntimePackageResolver, ImmutableFactoryIgnoresHostileSelectedExecutablesAndConfiguration) { Run(0); }
TEST_F(RuntimePackageResolver, RejectsUnsignedRepository) { Run(1); }
TEST_F(RuntimePackageResolver, RejectsExpiredSignedRepository) { Run(2); }
TEST_F(RuntimePackageResolver, RejectsAlteredAuthenticatedIndex) { Run(3); }
TEST_F(RuntimePackageResolver, RejectsAlteredDownloadedArchive) { Run(4); }
TEST_F(RuntimePackageResolver, RejectsChangedImmutableConfigurationBeforeApt) { Run(5); }
TEST_F(RuntimePackageResolver, PlansDependentRemovalFromSelectedMetadata) { Run(6); }
TEST_F(RuntimePackageResolver, PlansUpgradeFromSelectedMetadataAndPreservesAutomaticState) { Run(7); }
TEST(PackageResolverRequest, RejectsAptOptionsPathsSuffixOperatorsAndInvalidAction) {
    aegis::PackageResolverRequest r;r.package="bash";ASSERT_EQ(0,aegis::PackageResolverCheck(r));
    for(const char* name:{"--allow-unauthenticated","../bash","bash+","bash-","bash;id","bash\nx"}) {
        r.package=name;EXPECT_EQ(-1,aegis::PackageResolverCheck(r));
    }
    r.package="bash";r.version="1;id";EXPECT_EQ(-1,aegis::PackageResolverCheck(r));
    r.version="2";ASSERT_EQ(0,aegis::PackageResolverCheck(r));r.action=aegis::PackageAction::Remove;
    EXPECT_EQ(-1,aegis::PackageResolverCheck(r));r.version.clear();ASSERT_EQ(0,aegis::PackageResolverCheck(r));
    r.action=aegis::PackageAction::Update;EXPECT_EQ(-1,aegis::PackageResolverCheck(r));
    r.package.clear();ASSERT_EQ(0,aegis::PackageResolverCheck(r));
    r.action=static_cast<aegis::PackageAction>(99);EXPECT_EQ(-1,aegis::PackageResolverCheck(r));
}
