#ifndef AEGIS_PACKAGE_GUARD_H
#define AEGIS_PACKAGE_GUARD_H
#include "package_execution_protocol.h"
#ifdef __cplusplus
extern "C" {
#endif
struct aegis_package_guard;
/* Trusted PID1 snapshots real status before invoking candidate package tools.
 * All reads bounded, regular, root-owned, no symlinks or FIFO blocking. */
int aegis_package_guard_begin(int root,const struct aegis_package_execution_request *request,
                              struct aegis_package_guard **guard);
/* Structured simulation must cover exactly the sealed effects/argv. */
int aegis_package_guard_simulation(struct aegis_package_guard *guard,int json_fd);
/* After every child/descendant is reaped and marks applied: native full installed
 * registry and automatic-state comparison, independent of tool success/output. */
int aegis_package_guard_finish(struct aegis_package_guard *guard,int root);
/* Reconciliation only: after package children are reaped, verify the full
 * resulting registry and atomically apply all bound automatic/manual marks.
 * Never writes a selected image, only the unpublished owned candidate. */
int aegis_package_guard_reconcile_marks(struct aegis_package_guard *guard,int root);
/* Last trusted step after every package child is reaped and validation succeeds.
 * Write only the sealed private selection inside the unpublished candidate. */
int aegis_package_guard_commit(struct aegis_package_guard *guard,int root);
void aegis_package_guard_free(struct aegis_package_guard *guard);
#ifdef __cplusplus
}
#endif
#endif
