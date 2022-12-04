
#include "munit_test/unittest.h"


extern MunitTest test_unittest_tests[];

extern MunitSuite test_objects_suites[];
extern MunitSuite test_protocols_suites[];

MunitSuite munit_test_suites[] = {SUITE_OF_TESTS(test_unittest), SUITE_OF_SUITES(test_objects),
        SUITE_OF_SUITES(test_protocols), {NULL}};
