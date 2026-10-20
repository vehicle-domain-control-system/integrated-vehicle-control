#include "vss_bench.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    static char storage[32768];
    VssBenchText out;
    int rc;
    vss_bench_text_init(&out, storage, sizeof(storage));
    rc = vss_bench_run(argc - 1, (const char *const *)(argv + 1), &out);
    fputs(out.data, rc == VSS_BENCH_USAGE ? stderr : stdout);
    if (out.truncated) {
        fputs("error: output truncated\n", stderr);
        return VSS_BENCH_USAGE;
    }
    return rc;
}
