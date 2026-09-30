#ifndef AEGIS_PACKAGE_PROGRAM_H
#define AEGIS_PACKAGE_PROGRAM_H
#ifdef __cplusplus
extern "C" {
#endif
/* Child-only, after pivot, capability bounding, NNP, seccomp and descriptor
 * closure. Re-exec the same pinned immutable helper to enter the package
 * program domain BEFORE loading anything from the candidate. No host fd survives.
 * The helper entry forwards only the parent's fixed APT/dpkg commands. */
int aegis_package_exec_program(char *const arguments[], char *const environment[]);
int aegis_package_program_main(int argc, char **argv);
#ifdef __cplusplus
}
#endif
#endif
