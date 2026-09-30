#ifndef AEGIS_PACKAGE_VALIDATE_H
#define AEGIS_PACKAGE_VALIDATE_H
/* Trusted namespace PID1 only, after every package child is reaped. The root
 * belongs to the inactive candidate; no HOME or foreign CE is mounted. Checks
 * AEGIS account/NSS/owner policy without executing code from that candidate.
 * This is not repository authentication or publication of an image digest. */
int aegis_package_validate(int root);
#endif
