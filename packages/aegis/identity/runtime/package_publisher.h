#ifndef AEGIS_PACKAGE_PUBLISHER_H
#define AEGIS_PACKAGE_PUBLISHER_H
#include "package_store.h"

namespace aegis {
// Internal trusted publisher, NOT an APT/dpkg executor or authorization endpoint.
// Requester is the authenticated session owner, never the confirming admin.
struct PackagePublication {
    uint32_t requester = 0, serial = 0;
    uint64_t job = 0;
    std::string plan_sha256;
    bool personal = false, create = false, has_previous = false;
    PackageGeneration previous, candidate;
};
struct PackagePublicationResult {
    PackagePublish publication = PackagePublish::Unconfirmed;
    int error = 0;
};
struct PackagePublisher;

// All FDs originate from the trusted broker, never a CLI. The caller verifies
// immutable helper provenance, source semantics, store anchoring, fresh AOSP
// approval and (for private scope) CE/serial. Use a separate private memory-
// enabled cgroup parent, not the ordinary runtime contexts parent. One active
// publisher per requester/serial in that parent. No hash/copy in this call.
// Requires the existing pinned, single-threaded host-root broker namespace.
// *output begins null and retains ALL partial ownership on failure. It must
// be registered with requester quiescence before admission is released, even
// when Start fails. The caller closes its original CE/source FDs separately.
int PackagePublisherStart(int groups, int store, int source, int helper,
                          const PackagePublication& request, PackagePublisher** output);

// Wait for actual pidfd reaping, empty/removed cgroup and closure of every owned
// store/source FD. cancel first seals/kills the owned tree; no numeric PID.
// One total 0..10000ms deadline. Timeout/error retains *publisher and resources
// for retry; never treat it as completed teardown. On success frees/nulls it.
// Teardown confirmation does NOT prove publication or rollback: abrupt exit or
// lost response yields Unconfirmed, and the store must be inspected afresh.
// A reply alone is never completion. No implicit destructor abandons live work.
int PackagePublisherFinish(PackagePublisher** publisher, bool cancel, int timeout_ms,
                           PackagePublicationResult* result);
} // namespace aegis
#endif
