
// XXX The protocol tests may check for particular behaviours that are not, strictly speaking, part
// of their protocols, but that are instead common among all types that currently implement those
// protocols. There is room to adjust these tests as new types are created.

#include "munit_test/unittest.h"


MunitSuite test_protocols_suites[] = {SUITE_OF_TESTS(test_collection),
        SUITE_OF_TESTS(test_iterable), SUITE_OF_TESTS(test_mapping), SUITE_OF_TESTS(test_sequence),
        SUITE_OF_TESTS(test_setlike), SUITE_OF_TESTS(test_string), {NULL}};


extern void test_protocols_initialize(void)
{
    test_collection_initialize();
    test_iterable_initialize();
    test_mapping_initialize();
    test_sequence_initialize();
    test_setlike_initialize();
    test_string_initialize();
}
