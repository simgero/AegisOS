# APT resolver metadata fixture

This header contains **test-only** public metadata signed by an independent
throwaway Ed25519 key: `83FB32A3B9D316FCB7B2292EA1D34D2305688B27`. The private key was discarded. It is
not an AEGIS product signing key, Debian archive key or trust anchor.

`Packages` describes two versions of a synthetic app and its exact-version
library dependency. Archive hashes and sizes are placeholders: the test never
downloads these files and does not claim archive verification. Separate APT
execution tests use actual generated `.deb` files. The positive and expired
Release files bind the same Packages bytes through SHA-256 and carry actual
OpenPGP signatures; the expired case must fail freshness verification.

The static metadata was authored on the Mac, without building executable
packages or native code there. All AOSP/native compilation remains on
`aegis-build`; execution stays in local QEMU. The header is included only in
the namespace test probe, never a product keyring or repository configuration.

The companion parser consumes the pinned [APT 3.0.3 JSON hook protocol
0.2](https://github.com/Debian/apt/blob/3.0.3/apt-private/private-json-hooks.cc).
It accepts only the pre-prompt notification and records the selected install
version (which can differ from the repository candidate), current version,
dependent removals, and automatic/manual state. Human-readable APT terminal
output is not an input. The collector publishes one bounded notification only
after the completed protocol exchange, without replacing a previous result.

The isolated fixture uses a fixed configuration with a separate key, sources,
empty configuration-parts directory, fresh index directory for each negative
case and a disabled dpkg executable for simulation. No package script or dpkg
status change is allowed during planning. The existing actual installation,
upgrade and purge tests use their separate offline-archive configuration.

This is a component test, not the production planning authority. It does not
implement trusted network acquisition, signature-to-archive evidence binding,
immutable product planner mounts, request lifecycle/approval, exact execution
effect comparison or automatic-state propagation into the bound plan. These
must be integrated before exposing a public package operation.
