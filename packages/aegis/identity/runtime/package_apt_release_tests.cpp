#include "package_apt_release.h"
#include <gtest/gtest.h>
#include <errno.h>
using namespace aegis;
namespace {
constexpr uint64_t date=1790640000; // Tue, 29 Sep 2026 00:00:00 UTC
std::string Release() {
    return "Origin: fixture\nDate: Tue, 29 Sep 2026 00:00:00 UTC\nValid-Until: Wed, 30 Sep 2026 00:00:00 UTC\nSHA256:\n "+std::string(64,'a')+" 123 main/binary-arm64/Packages\n";
}
void Reject(const std::string& s,int error=EBADMSG,uint64_t now=date+1) {
    PackageReleaseIndex out;out.date=42;
    EXPECT_EQ(-1,PackageReadAuthenticatedRelease(s,"main/binary-arm64/Packages",now,&out));
    EXPECT_EQ(error,errno);EXPECT_EQ(42u,out.date);
}
TEST(PackageAptRelease, ExactIndexAndExpiryFromDetachedText) {
    PackageReleaseIndex out;ASSERT_EQ(0,PackageReadAuthenticatedRelease(Release(),"main/binary-arm64/Packages",date+1,&out));
    EXPECT_EQ(date,out.date);EXPECT_EQ(date+86400,out.valid_until);EXPECT_EQ(123u,out.index.bytes);EXPECT_EQ(std::string(64,'a'),out.index.sha256);
}
TEST(PackageAptRelease, ClearSignedAndCrlfPayloadsUseTheSameFields) {
    // Syntax fixture only: signature authenticity is always APT's prerequisite.
    auto s="-----BEGIN PGP SIGNED MESSAGE-----\nHash: SHA256\n\n"+Release()+"-----BEGIN PGP SIGNATURE-----\nfixture\n-----END PGP SIGNATURE-----\n";
    std::string crlf;for(char c:s) { if(c=='\n')crlf+='\r';crlf+=c; }
    PackageReleaseIndex out;ASSERT_EQ(0,PackageReadAuthenticatedRelease(crlf,"main/binary-arm64/Packages",date+1,&out));EXPECT_EQ(123u,out.index.bytes);
    Reject(s.substr(0,s.find("-----END PGP SIGNATURE-----")));
}
TEST(PackageAptRelease, MissingValidUntilUsesBoundedAgeAndDeclaredExpiryCannotExtendIt) {
    auto s=Release();auto a=s.find("Valid-Until:"),b=s.find('\n',a);s.erase(a,b-a+1);
    PackageReleaseIndex out;ASSERT_EQ(0,PackageReadAuthenticatedRelease(s,"main/binary-arm64/Packages",date+1,&out));EXPECT_EQ(date+PackageReleaseMaxAge,out.valid_until);
    Reject(s,ESTALE,date+PackageReleaseMaxAge);
    s.insert(a,"Valid-Until: Mon, 29 Sep 2036 00:00:00 UTC\n");
    ASSERT_EQ(0,PackageReadAuthenticatedRelease(s,"main/binary-arm64/Packages",date+1,&out));EXPECT_EQ(date+PackageReleaseMaxAge,out.valid_until);
}
TEST(PackageAptRelease, ExpiryFutureDateAndImpossibleCalendarAreRejected) {
    Reject(Release(),ESTALE,date+86400);Reject(Release(),EBADMSG,date-11);
    auto s=Release();s.replace(s.find("Tue, 29 Sep"),11,"Tue, 31 Sep");Reject(s);
    s=Release();s.replace(s.find("Tue,"),4,"Mon,");Reject(s);
    s=Release();s.replace(s.find("UTC"),3,"PST");Reject(s);
}
TEST(PackageAptRelease, DuplicateFieldsIndexesAndUnexpectedRecordsAreRejected) {
    Reject(Release()+"DATE: Tue, 29 Sep 2026 00:00:00 UTC\n");
    Reject(Release()+" "+std::string(64,'b')+" 123 main/binary-arm64/Packages\n");
    Reject(Release()+"\nOrigin: another\n");
}
TEST(PackageAptRelease, StrongHashSizeAndCanonicalKeyAreRequired) {
    auto s=Release();s.replace(s.find("SHA256:"),7,"SHA1:  ");Reject(s,ENOENT);
    s=Release();s.replace(s.find(" 123 "),5," 0123 ");Reject(s);
    s=Release();s.replace(s.find(" 123 "),5," 268435457 ");Reject(s,EFBIG);
    s=Release();s.replace(s.find("main/binary"),4,"../main");Reject(s);
    PackageReleaseIndex out;EXPECT_EQ(-1,PackageReadAuthenticatedRelease(Release(),"../Packages",date+1,&out));EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ(-1,PackageReadAuthenticatedRelease(Release(),"other/Packages",date+1,&out));EXPECT_EQ(ENOENT,errno);
}
}
