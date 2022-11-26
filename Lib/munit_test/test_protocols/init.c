
#include "munit_test/unittest.h"


extern MunitTest test_sequence_tests[];

MunitSuite test_protocols_suites[] = {SUITE_OF_TESTS(test_sequence), {NULL}};
