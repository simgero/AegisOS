#include "package_execution_protocol.h"
#include "sandbox.h"
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
    uint32_t user, serial;
    if (argc != 3 || number(argv[1], &user) < 0 || number(argv[2], &serial) < 0
            || aegis_check_package_context(user) < 0) return 78;
    return aegis_package_execute(user, serial);
}
