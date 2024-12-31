
#include "munit_test/unittest.h"


MunitSuite test_objects_suites[] = {SUITE_OF_TESTS(test_exception), SUITE_OF_TESTS(test_frozendict),
        SUITE_OF_TESTS(test_frozenset), SUITE_OF_TESTS(test_function), SUITE_OF_TESTS(test_iter),
        SUITE_OF_TESTS(test_range), SUITE_OF_TESTS(test_tuple), {NULL}};


extern void test_objects_initialize(void)
{
    test_exception_initialize();
    test_frozendict_initialize();
    test_frozenset_initialize();
    test_function_initialize();
    test_iter_initialize();
    test_range_initialize();
    test_tuple_initialize();
}
