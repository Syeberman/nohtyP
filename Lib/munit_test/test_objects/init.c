
#include "munit_test/unittest.h"


MunitSuite test_objects_suites[] = {SUITE_OF_TESTS(test_frozenset), {NULL}};


extern void test_objects_initialize(void) { test_frozenset_initialize(); }
