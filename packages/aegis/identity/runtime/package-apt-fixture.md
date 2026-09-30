# APT resolver metadata fixture

This header contains **test-only** public metadata signed by an independent
throwaway Ed25519 key: `C2DC8C2FB8575A771FE65F4D9FC544D4B3434A8D`. The private key was discarded. It is
not an AEGIS product signing key, Debian archive key or trust anchor.

`Packages` describes two versions of a synthetic app and its exact-version
library dependency, with actual archive sizes and SHA-256 values. Both the
valid and expired Release bind the full index and have genuine OpenPGP
signatures. APT must reject a modified archive and download the unchanged files;
the native matcher independently checks each downloaded file's authenticated
index size/hash and retains the resolver's automatic/manual state.

[Public builder fixture](https://github.com/simgero/AegisOS/releases/tag/apt-fixture-20260930T060011Z-2d618d0d),
created by `scripts/runtime/build-apt-fixture.py` at commit `2d618d0d`.
The four architecture-all `.deb` archives were built on `aegis-build` without
executing their package contents. The generated header and public receipt came
back exclusively through a verified GitHub release. The private signing key was
discarded on the builder. Neither key nor repository is installed in production.
The pre-existing separate execution fixture still builds local synthetic script
packages inside QEMU to exercise maintainer scripts/config preservation.

The companion parser consumes the pinned [APT 3.0.3 JSON hook protocol
0.2](https://github.com/Debian/apt/blob/3.0.3/apt-private/private-json-hooks.cc).
It accepts only the pre-prompt notification and records the selected install
version (which can differ from the repository candidate), current version,
dependent removals, and automatic/manual state. Human-readable APT terminal
output is not an input. The collector publishes one bounded notification only
after the completed protocol exchange, without replacing a previous result.

The isolated fixture uses APT’s local `copy:` transport and a fixed configuration with a separate key, sources,
empty configuration-parts directory, fresh index directory for each negative
case and a disabled dpkg executable for simulation. No package script or dpkg
status change is allowed during planning. The existing actual installation,
upgrade and purge tests use their separate offline-archive configuration.

This is a component test, not the production planning authority. It does not
implement product network acquisition, lifecycle-owned trusted acquisition,
immutable product planner mounts, request lifecycle/approval, exact execution
effect comparison or automatic-state propagation into the bound plan. These
must be integrated before exposing a public package operation.

The index matcher follows the [Debian control-file format](https://www.debian.org/doc/debian-policy/ch-controlfields.html):
case-insensitive unique field names, record separators and no folding of the
identity/filename/size/hash fields. It hashes complete readonly index FDs against
already-authenticated receipts and rejects conflicting selected archive content
across sources. Identical content from multiple sources has a deterministic
choice; no resolver effect or automatic mark is omitted. Size/SHA-256 checks on
pinned quiescent archive FDs run before preparation. These streaming reads must
remain in an owned worker; the library is not an authentication or network API.

The new actual-archive fixture and matcher are not yet compiled or guest-tested.
The previous 53-test proof applies to the earlier metadata-only fixture.

APT 3.0.3's [copy method](https://github.com/Debian/apt/blob/3.0.3/methods/copy.cc)
compares expected hashes after copying but returns a bare failure on mismatch;
its user-facing diagnostic is only `Undetermined Error`. The controlled test
therefore requires nonzero acquisition status, independently confirms that the
changed file has the same size but different bytes, restores the original and
requires successful acquisition with identical policy/key/index inputs.
The complete native index/archive hash checks remain separate requirements.
The earlier file-source-only test could assert a more specific message; requiring
that message for copy transport incorrectly stopped the otherwise valid refusal.

The pinned [APT install flow](https://github.com/Debian/apt/blob/3.0.3/apt-private/private-install.cc)
processes `--no-download` before its simulation branch. With `copy:` sources this
rejects uncached archives even during simulation. Planning therefore uses only
`--simulate`, which returns before acquisition or dpkg execution. The fixture
independently requires the archive cache to remain empty around the initial
simulation, unchanged dpkg state and no maintainer-script output. Actual archive
acquisition remains a separate `--download-only` operation with unchanged trust.
