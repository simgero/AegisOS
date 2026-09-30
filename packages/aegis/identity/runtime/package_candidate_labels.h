#ifndef AEGIS_PACKAGE_CANDIDATE_LABELS_H
#define AEGIS_PACKAGE_CANDIDATE_LABELS_H
#ifdef __cplusplus
extern "C" {
#endif
#define AEGIS_PACKAGE_CANDIDATE_CONTEXT "u:object_r:aegis_package_candidate_file:s0"
/* Trusted, quiescent candidate only. Reads effective labels, including symlink
 * inodes without following their targets. The caller retains exclusive mount/
 * image ownership and must reap every package child before validating again.
 * runtime_mounts=0 before handing off the bare detached mount. Value 1 is only
 * for namespace PID1 after independently checking its fixed mount inventory:
 * /dev,/proc,/tmp,/run may then be the four expected foreign mount roots.
 * No labels are changed, no file contents are read/executed, no path is logged.
 */
int aegis_package_candidate_labels(int root, int runtime_mounts);
#ifdef __cplusplus
}
#endif
#endif
