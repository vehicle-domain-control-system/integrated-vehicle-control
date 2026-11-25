#include "p6_registration_console_parser.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);       \
            return false;                                                       \
        }                                                                       \
    } while (0)

static p6_registration_console_parse_result_t feed_bytes(
    p6_registration_console_parser_t *parser,
    const char *bytes,
    size_t length,
    const char **out_line)
{
    p6_registration_console_parse_result_t result =
        P6_REGISTRATION_CONSOLE_INPUT_PENDING;
    size_t index;

    for (index = 0U; index < length; ++index) {
        result = p6_registration_console_parser_feed(
            parser, (uint8_t)bytes[index], out_line);
    }
    return result;
}

static bool test_character_at_a_time_waits_for_terminator(void)
{
    static const char command[] = "p6-reg-request";
    p6_registration_console_parser_t parser;
    const char *line = NULL;
    size_t index;

    p6_registration_console_parser_init(&parser);
    for (index = 0U; index < strlen(command); ++index) {
        CHECK(p6_registration_console_parser_feed(
                  &parser, (uint8_t)command[index], &line) ==
              P6_REGISTRATION_CONSOLE_INPUT_PENDING);
        CHECK(line == NULL);
    }
    CHECK(p6_registration_console_parser_feed(&parser, (uint8_t)'\n',
                                               &line) ==
          P6_REGISTRATION_CONSOLE_LINE_READY);
    CHECK(strcmp(line, command) == 0);
    return true;
}

static bool test_split_input_and_line_endings(void)
{
    static const char request_lf[] = "p6-reg-request\n";
    p6_registration_console_parser_t parser;
    const char *line = NULL;

    p6_registration_console_parser_init(&parser);
    CHECK(feed_bytes(&parser, "p6-reg-", sizeof("p6-reg-") - 1U, &line) ==
          P6_REGISTRATION_CONSOLE_INPUT_PENDING);
    CHECK(feed_bytes(&parser, "approve\r", sizeof("approve\r") - 1U,
                     &line) == P6_REGISTRATION_CONSOLE_LINE_READY);
    CHECK(strcmp(line, "p6-reg-approve") == 0);

    CHECK(p6_registration_console_parser_feed(&parser, (uint8_t)'\n',
                                               &line) ==
          P6_REGISTRATION_CONSOLE_INPUT_PENDING);
    CHECK(feed_bytes(&parser, request_lf, sizeof(request_lf) - 1U, &line) ==
          P6_REGISTRATION_CONSOLE_LINE_READY);
    CHECK(strcmp(line, "p6-reg-request") == 0);
    return true;
}

static bool test_overlong_line_is_discarded_and_recovers(void)
{
    static const char request_cr[] = "p6-reg-request\r";
    p6_registration_console_parser_t parser;
    const char *line = NULL;
    size_t index;

    p6_registration_console_parser_init(&parser);
    for (index = 0U; index < P6_REGISTRATION_CONSOLE_LINE_CAPACITY; ++index) {
        CHECK(p6_registration_console_parser_feed(&parser, (uint8_t)'y',
                                                   &line) ==
              P6_REGISTRATION_CONSOLE_INPUT_PENDING);
    }
    CHECK(p6_registration_console_parser_feed(&parser, (uint8_t)'\n',
                                               &line) ==
          P6_REGISTRATION_CONSOLE_LINE_READY);
    CHECK(strlen(line) == P6_REGISTRATION_CONSOLE_LINE_CAPACITY);

    for (index = 0U; index < P6_REGISTRATION_CONSOLE_LINE_CAPACITY + 20U;
         ++index) {
        CHECK(p6_registration_console_parser_feed(&parser, (uint8_t)'x',
                                                   &line) ==
              P6_REGISTRATION_CONSOLE_INPUT_PENDING);
    }
    CHECK(p6_registration_console_parser_feed(&parser, (uint8_t)'\r',
                                               &line) ==
          P6_REGISTRATION_CONSOLE_LINE_DISCARDED);
    CHECK(line == NULL);
    CHECK(p6_registration_console_parser_feed(&parser, (uint8_t)'\n',
                                               &line) ==
          P6_REGISTRATION_CONSOLE_INPUT_PENDING);
    CHECK(feed_bytes(&parser, request_cr, sizeof(request_cr) - 1U, &line) ==
          P6_REGISTRATION_CONSOLE_LINE_READY);
    CHECK(strcmp(line, "p6-reg-request") == 0);
    return true;
}

int main(void)
{
    static const struct {
        const char *name;
        bool (*run)(void);
    } tests[] = {
        {"character_at_a_time", test_character_at_a_time_waits_for_terminator},
        {"split_and_line_endings", test_split_input_and_line_endings},
        {"overlong_recovery", test_overlong_line_is_discarded_and_recovers},
    };
    size_t passed = 0U;
    size_t index;

    for (index = 0U; index < sizeof(tests) / sizeof(tests[0]); ++index) {
        if (!tests[index].run()) {
            printf("FAILED: %s\n", tests[index].name);
            return 1;
        }
        passed++;
    }
    printf("PASS: %zu/%zu P6 console parser tests\n", passed,
           sizeof(tests) / sizeof(tests[0]));
    return 0;
}
