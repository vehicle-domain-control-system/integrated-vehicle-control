#ifndef P6_REGISTRATION_CONSOLE_PARSER_H
#define P6_REGISTRATION_CONSOLE_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define P6_REGISTRATION_CONSOLE_LINE_CAPACITY 48U

typedef enum {
    P6_REGISTRATION_CONSOLE_INPUT_PENDING = 0,
    P6_REGISTRATION_CONSOLE_LINE_READY,
    P6_REGISTRATION_CONSOLE_LINE_DISCARDED,
} p6_registration_console_parse_result_t;

typedef struct {
    char line[P6_REGISTRATION_CONSOLE_LINE_CAPACITY + 1U];
    size_t length;
    bool discarding;
    bool ignore_lf_after_cr;
} p6_registration_console_parser_t;

void p6_registration_console_parser_init(
    p6_registration_console_parser_t *parser);

p6_registration_console_parse_result_t
p6_registration_console_parser_feed(
    p6_registration_console_parser_t *parser,
    uint8_t byte,
    const char **out_line);

#ifdef __cplusplus
}
#endif

#endif
