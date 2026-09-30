// Compile on aegis-build; execute only in local Android QEMU.
#include "base_image.h"
#include <gtest/gtest.h>
#include <errno.h>
#include <string.h>
#include <string>

namespace {
std::string receipt() {
    const std::string sha(64, 'a');
    return "{\"schema\":1,\"status\":\"BUILT_VERIFIED_NOT_MOUNTED\",\"generation\":\"" + sha
        + "\",\"recipe_input_sha256\":\"" + sha + "\",\"plan_sha256\":\"" + sha
        + "\",\"tool_sha256\":{\"mkuserimg_mke2fs\":\"" + sha + "\",\"mke2fs\":\"" + sha
        + "\",\"e2fsdroid\":\"" + sha + "\",\"e2fsck\":\"" + sha + "\",\"debugfs\":\"" + sha
        + "\"},\"uuid\":\"aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa\",\"image_sha256\":\"" + sha
        + "\",\"image_bytes\":268435456,\"repeated_build_identical\":true,\"scope\":\"test fixture\"}";
}
std::string changed(std::string text, const std::string& before, const std::string& after) {
    size_t offset = text.find(before);
    EXPECT_NE(std::string::npos, offset);
    if (offset != std::string::npos) text.replace(offset, before.size(), after);
    return text;
}
void reject(const std::string& text) {
    SCOPED_TRACE(text); // Inert fixtures only; identify the rejected variant.
    aegis_base_receipt output, before;
    memset(&output, 0xa5, sizeof(output)); before = output;
    EXPECT_EQ(-1, aegis_base_parse_receipt(text.data(), text.size(), &output));
    EXPECT_EQ(0, memcmp(&before, &output, sizeof(output)));
}
}

TEST(RuntimeBaseImage, ReceiptBindsExactGenerationAndBytes) {
    auto text = receipt();
    aegis_base_receipt output = {};
    ASSERT_EQ(0, aegis_base_parse_receipt(text.data(), text.size(), &output));
    EXPECT_EQ(268435456u, output.bytes);
    EXPECT_EQ(std::string(64, 'a'), output.sha256);
}

TEST(RuntimeBaseImage, DuplicateUnknownAndTrailingDataCannotAuthorizeMount) {
    auto text = receipt();
    reject("{\"schema\":1," + text.substr(1));
    reject("{\"image_path\":\"/data/local/tmp/other\"," + text.substr(1));
    reject(text + "{}");
    reject(changed(text, "\"schema\":1,", ""));
    reject(changed(text, "\"schema\":1", "\"schema\":2"));
    reject(changed(text, "\"schema\":1", "\"schema\":1.0"));
    reject(changed(text, "\"schema\":1", "\"schema\":true"));
    reject(changed(text, "\"schema\":1", "\"schema\":1/* comment */"));
    reject("/* before */" + text);
    reject(text + "/* after */");
    reject(changed(text, "\"schema\":1,", "\"schema\":1,// comment\n"));
    reject(changed(text, "\"schema\":1", "\"schema\"/* name */:1"));
    reject(changed(text, "\"schema\":1", "\"schema\":/* value */1"));
}

TEST(RuntimeBaseImage, WrongHashSizeOrBuildStatusIsRejected) {
    auto text = receipt();
    reject(changed(text, "268435456", "268435457"));
    reject(changed(text, "268435456", "268435456.0"));
    reject(changed(text, "268435456", "-1"));
    reject(changed(text, "BUILT_VERIFIED_NOT_MOUNTED", "BUILDING"));
    reject(changed(text, "true", "false"));
    reject(changed(text, std::string(64, 'a'), std::string(64, 'b')));
    reject(changed(text, std::string(64, 'a'), std::string(64, 'A')));
    reject(changed(text, "aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa", "bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbbbbb"));
    reject(changed(text, "\"debugfs\"", "\"different_tool\""));
}

TEST(RuntimeBaseImage, OversizedTruncatedNulAndDeeplyNestedReceiptsAreBounded) {
    auto text = receipt();
    reject(""); reject(text.substr(0, text.size() - 1));
    reject(std::string(16385, ' '));
    reject(text + std::string(1, '\0'));
    reject(std::string(1000, '[') + "0" + std::string(1000, ']'));
    reject(changed(text, "\"test fixture\"", "[[[[[[[[[0]]]]]]]]]"));
    aegis_base_receipt output = {};
    EXPECT_EQ(-1, aegis_base_parse_receipt(nullptr, 1, &output));
    EXPECT_EQ(-1, aegis_base_parse_receipt(text.data(), text.size(), nullptr));
}

TEST(RuntimeBaseImage, QuotedBracketsAndEscapesDoNotAffectNestingLimit) {
    auto text = changed(receipt(), "test fixture", "[[[[[[[[[[[[[[[[\\\"\\\\test]]]]]]]]]]]]]]]");
    aegis_base_receipt output = {};
    EXPECT_EQ(0, aegis_base_parse_receipt(text.data(), text.size(), &output));
    text = changed(receipt(), "test fixture", "https://example.invalid/path/* data */");
    EXPECT_EQ(0, aegis_base_parse_receipt(text.data(), text.size(), &output));
}

TEST(RuntimeBaseImage, SelectionOutputsRejectAliasedOrMissingOutputBeforeOpeningSystemFiles) {
    aegis_base_receipt value={};value.bytes=42;int image=-1;
    EXPECT_EQ(-1,aegis_base_open_selection(nullptr,&value));EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ(-1,aegis_base_open_selection(&image,nullptr));EXPECT_EQ(EINVAL,errno);EXPECT_EQ(-1,image);
    image=7;EXPECT_EQ(-1,aegis_base_open_selection(&image,&value));EXPECT_EQ(EINVAL,errno);
    EXPECT_EQ(7,image);EXPECT_EQ(42u,value.bytes);
}
