#include "package_execution_protocol.h"
#include "sandbox.h"
#include "package_apt_hook.h"
#include <string.h>
#include <errno.h>
#include <stdlib.h>
static int number(const char *s, uint32_t *result) {
    if (!*s || (*s == '0' && s[1])) return -1;
    uint64_t n = 0;
    for (; *s; s++) { if (*s < '0' || *s > '9') return -1;
        n = n * 10 + *s - '0'; if (n > INT32_MAX) return -1; }
    *result = n;return 0;
}
int main(int argc, char **argv) {
    if(argc==2 && !strcmp(argv[1],"--apt-plan-hook"))return aegis_apt_plan_hook();
    uint32_t user, serial;
    if (argc != 3 || number(argv[1], &user) < 0 || number(argv[2], &serial) < 0
            || aegis_check_package_context(user) < 0) return 78;
    return aegis_package_execute(user, serial, 0);
}
