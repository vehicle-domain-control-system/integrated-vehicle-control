/* Non-product Host test dispatcher. No runtime/router/producer registration. */
#include "net_wave3a_test_rx.h"
#include "net_wave3a_test_tx.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "one Wave3A test case name is required\n");
        return 2;
    }
    if (strncmp(argv[1], "rear_", 5u) == 0 || strcmp(argv[1], "callback_reentry") == 0)
        return net_wave3a_test_rx(argv[1]);
    if (strncmp(argv[1], "permission_", 11u) == 0)
        return net_wave3a_test_tx(argv[1]);
    fprintf(stderr, "unknown Wave3A case: %s\n", argv[1]);
    return 2;
}
