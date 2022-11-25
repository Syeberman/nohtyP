
#include "munit_test/unittest.h"


extern MunitTest test_sequence_tests[];

MunitSuite test_protocols_suite[] = {{
                                             "/test_sequence",        // prefix
                                             test_sequence_tests,     // tests
                                             NULL,                    // suites
                                             1,                       // iterations
                                             MUNIT_SUITE_OPTION_NONE  // options
                                     },
        {NULL}};
