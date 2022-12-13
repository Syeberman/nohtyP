
#include "munit_test/unittest.h"


extern MunitTest test_sequence_tests[];
extern MunitTest test_string_tests[];

MunitSuite test_protocols_suites[] = {
        SUITE_OF_TESTS(test_sequence), SUITE_OF_TESTS(test_string), {NULL}};


extern void test_protocols_initialize(void)
{
    test_sequence_initialize();
    test_string_initialize();
}
