#ifndef NETWORK_TOOLS_VSS_BENCH_H
#define NETWORK_TOOLS_VSS_BENCH_H

/* Host-only bench helper. It builds CAN/UART-C frames with the repository
 * wire codecs and prints S32K144 debug-console commands that send them, and
 * decodes frames copied from that console. It is not product code and adds
 * no policy: every verdict is the codec verdict. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Exit codes of vss_bench_run(). */
#define VSS_BENCH_OK 0      /* frame built, or decoded frame accepted */
#define VSS_BENCH_INVALID 1 /* decoded frame rejected (CRC, length or codec) */
#define VSS_BENCH_USAGE 2   /* bad command line, or the codec refused the values */

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
    bool truncated;
} VssBenchText;

void vss_bench_text_init(VssBenchText *text, char *storage, size_t capacity);

/* argv excludes the program name: {"can", "event", "--boot", "1", ...}.
 * All output, including error messages, is appended to out. */
int vss_bench_run(int argc, const char *const *argv, VssBenchText *out);

/* Collects bytes from console-log tokens: "A5 5A ..", "data=A55A..",
 * "hex=A5", "0x1F" and offset labels such as "00:" (skipped). Returns false
 * on a non-hex token, an odd digit count or more than capacity bytes. */
bool vss_bench_parse_hex(int argc, const char *const *argv,
    uint8_t *out, size_t capacity, size_t *length);

#endif
