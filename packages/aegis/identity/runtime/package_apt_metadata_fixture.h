// TEST DATA ONLY: never install this key or source in a product configuration.
// Public signing key and detached signed resolver metadata. The dummy archive
// hashes exercise planning only; no binary archive is claimed verified here.
// Private fixture key was discarded after signing. Native code builds on SSH.
#ifndef AEGIS_PACKAGE_APT_METADATA_FIXTURE_H
#define AEGIS_PACKAGE_APT_METADATA_FIXTURE_H
struct aegis_apt_metadata_fixture { const char *name, *data; };
static const struct aegis_apt_metadata_fixture aegis_apt_metadata[] = {
    {"aegis-test-key.asc",
     "-----BEGIN PGP PUBLIC KEY BLOCK-----\n"
     "\n"
     "mDMEZZIAgBYJKwYBBAHaRw8BAQdAKeDeI/+1SmJPkQeASusUiqv0o33vFrwk8s5m\n"
     "B0SRe2O0JUFFR0lTIFRFU1QgT05MWSA8YXB0LWZpeHR1cmVAaW52YWxpZD6IkwQT\n"
     "FgoAOxYhBIP7MqO50xb8t7IpLqHTTSMFaIsnBQJlkgCAAhsDBQsJCAcCAiICBhUK\n"
     "CQgLAgQWAgMBAh4HAheAAAoJEKHTTSMFaIsnsGgA+wWvU8opq8ZlqoO0+bVXg7y0\n"
     "KuAZ5plbKq9nZGdxYksxAPwJiS/XzeSyV6mFFBoqjETlylbat22Rlxw1uLQudGni\n"
     "Cw==\n"
     "=fJYQ\n"
     "-----END PGP PUBLIC KEY BLOCK-----\n"
    },
    {"aegis-repo/Packages",
     "Package: aegis-probe-app\n"
     "Version: 1\n"
     "Architecture: all\n"
     "Depends: aegis-probe-lib (= 1)\n"
     "Maintainer: AEGIS test <fixture@invalid>\n"
     "Description: Signed resolver metadata fixture only\n"
     "Filename: pool/aegis-probe-app_1_all.deb\n"
     "Size: 1024\n"
     "SHA256: 172dd4a0366000604e2c4de41457aa1eb3093bb59ead22e0f1d472a2aaade094\n"
     "\n"
     "Package: aegis-probe-app\n"
     "Version: 2\n"
     "Architecture: all\n"
     "Depends: aegis-probe-lib (= 2)\n"
     "Maintainer: AEGIS test <fixture@invalid>\n"
     "Description: Signed resolver metadata fixture only\n"
     "Filename: pool/aegis-probe-app_2_all.deb\n"
     "Size: 1024\n"
     "SHA256: d5856351bbc14599e687dac105150e8a919b21477f3c00386405228caac1e43a\n"
     "\n"
     "Package: aegis-probe-lib\n"
     "Version: 1\n"
     "Architecture: all\n"
     "Maintainer: AEGIS test <fixture@invalid>\n"
     "Description: Signed resolver metadata fixture only\n"
     "Filename: pool/aegis-probe-lib_1_all.deb\n"
     "Size: 1024\n"
     "SHA256: 2bb9eaf06f170742743e686076e1cc7e8ce43a9ef6c44cdd8b3487fea364fc5f\n"
     "\n"
     "Package: aegis-probe-lib\n"
     "Version: 2\n"
     "Architecture: all\n"
     "Maintainer: AEGIS test <fixture@invalid>\n"
     "Description: Signed resolver metadata fixture only\n"
     "Filename: pool/aegis-probe-lib_2_all.deb\n"
     "Size: 1024\n"
     "SHA256: 7cdacf15bed300bef18d0450b706e0778e49c9c31bd742e691c3f464d3539492\n"
     "\n"
    },
    {"aegis-repo/Release",
     "Origin: AEGIS TEST ONLY\n"
     "Label: AEGIS TEST ONLY\n"
     "Suite: aegis-test\n"
     "Codename: aegis-test\n"
     "Architectures: arm64 all\n"
     "Date: Tue, 29 Sep 2026 00:00:00 UTC\n"
     "Valid-Until: Tue, 29 Sep 2037 00:00:00 UTC\n"
     "SHA256:\n"
     " 2c936465614e7e3104b5f945d7f04246940635e9818e0a8189042a7a50e4887a 1150 Packages\n"
    },
    {"aegis-repo/Release.gpg",
     "-----BEGIN PGP SIGNATURE-----\n"
     "\n"
     "iHUEABYKAB0WIQSD+zKjudMW/LeyKS6h000jBWiLJwUCZZIAgAAKCRCh000jBWiL\n"
     "J7NOAP4wu3C7p2EuxZZVo+1r9tREua6rej0k0DGeWCyFKIlCewEAnRl13M7VfEcE\n"
     "BLvOx9z8NGFK/9lf7QycmX7gGQ3rvws=\n"
     "=1naE\n"
     "-----END PGP SIGNATURE-----\n"
    },
    {"aegis-expired-Release",
     "Origin: AEGIS TEST ONLY\n"
     "Label: AEGIS TEST ONLY\n"
     "Suite: aegis-test\n"
     "Codename: aegis-test\n"
     "Architectures: arm64 all\n"
     "Date: Mon, 01 Jan 2024 00:00:00 UTC\n"
     "Valid-Until: Tue, 02 Jan 2024 00:00:00 UTC\n"
     "SHA256:\n"
     " 2c936465614e7e3104b5f945d7f04246940635e9818e0a8189042a7a50e4887a 1150 Packages\n"
    },
    {"aegis-expired-Release.gpg",
     "-----BEGIN PGP SIGNATURE-----\n"
     "\n"
     "iHUEABYKAB0WIQSD+zKjudMW/LeyKS6h000jBWiLJwUCZZIAgAAKCRCh000jBWiL\n"
     "JwN/AQCS+9678qDCvgKEAaVmv4OoYXxymdCJZKOy0DZolE5ufgD8DuaVg8PyZvrR\n"
     "wj/Suk4gh76iaaWEgxu4yYd0Hwiw5QM=\n"
     "=GEq7\n"
     "-----END PGP SIGNATURE-----\n"
    },
};
#endif
