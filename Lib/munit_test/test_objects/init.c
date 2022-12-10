
#include "munit_test/unittest.h"


extern MunitTest test_frozenset_tests[];

MunitSuite test_objects_suites[] = {SUITE_OF_TESTS(test_frozenset), {NULL}};


extern void test_objects_initialize(void) {
    test_frozenset_initialize();
}
