
#include "munit_test/unittest.h"


MunitSuite test_objects_suites[] = {SUITE_OF_TESTS(test_exception), SUITE_OF_TESTS(test_frozendict),
        SUITE_OF_TESTS(test_frozenset), SUITE_OF_TESTS(test_function), SUITE_OF_TESTS(test_range),
        {NULL}};


extern void test_objects_initialize(void)
{
    test_exception_initialize();
    test_frozendict_initialize();
    test_frozenset_initialize();
    test_function_initialize();
    test_range_initialize();
}
