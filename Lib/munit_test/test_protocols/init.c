
#include "munit_test/unittest.h"


MunitSuite test_protocols_suites[] = {
        SUITE_OF_TESTS(test_sequence), SUITE_OF_TESTS(test_string), {NULL}};


extern void test_protocols_initialize(void)
{
    test_sequence_initialize();
    test_string_initialize();
}
