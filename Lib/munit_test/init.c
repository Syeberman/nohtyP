
#include "munit_test/unittest.h"


MunitSuite munit_test_suites[] = {SUITE_OF_TESTS(test_unittest), SUITE_OF_SUITES(test_objects),
        SUITE_OF_SUITES(test_protocols), {NULL}};


extern void munit_test_initialize(void)
{
    test_unittest_initialize();

    test_objects_initialize();
    test_protocols_initialize();
}
