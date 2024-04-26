
#include "munit_test/unittest.h"


MunitSuite test_objects_suites[] = {SUITE_OF_TESTS(test_frozendict), SUITE_OF_TESTS(test_frozenset),
        SUITE_OF_TESTS(test_function), {NULL}};


extern void test_objects_initialize(void)
{
    test_frozendict_initialize();
    test_frozenset_initialize();
    test_function_initialize();
}
