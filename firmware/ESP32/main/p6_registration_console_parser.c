#include "p6_registration_console_parser.h"

#include <string.h>

void p6_registration_console_parser_init(
    p6_registration_console_parser_t *parser)
{
    if (parser != NULL) {
        memset(parser, 0, sizeof(*parser));
    }
}

p6_registration_console_parse_result_t
p6_registration_console_parser_feed(
    p6_registration_console_parser_t *parser,
    uint8_t byte,
    const char **out_line)
{
    bool discarded;

    if (out_line != NULL) {
        *out_line = NULL;
    }
    if (parser == NULL || out_line == NULL) {
        return P6_REGISTRATION_CONSOLE_INPUT_PENDING;
    }

    if (parser->ignore_lf_after_cr) {
        parser->ignore_lf_after_cr = false;
        if (byte == (uint8_t)'\n') {
            return P6_REGISTRATION_CONSOLE_INPUT_PENDING;
        }
    }

    if (byte == (uint8_t)'\r' || byte == (uint8_t)'\n') {
        discarded = parser->discarding;
        if (byte == (uint8_t)'\r') {
            parser->ignore_lf_after_cr = true;
        }
        parser->discarding = false;
        if (discarded) {
            parser->length = 0U;
            parser->line[0] = '\0';
            return P6_REGISTRATION_CONSOLE_LINE_DISCARDED;
        }
        parser->line[parser->length] = '\0';
        parser->length = 0U;
        *out_line = parser->line;
        return P6_REGISTRATION_CONSOLE_LINE_READY;
    }

    if (parser->discarding) {
        return P6_REGISTRATION_CONSOLE_INPUT_PENDING;
    }
    if (parser->length >= (sizeof(parser->line) - 1U)) {
        parser->length = 0U;
        parser->line[0] = '\0';
        parser->discarding = true;
        return P6_REGISTRATION_CONSOLE_INPUT_PENDING;
    }
    parser->line[parser->length++] = (char)byte;
    return P6_REGISTRATION_CONSOLE_INPUT_PENDING;
}
