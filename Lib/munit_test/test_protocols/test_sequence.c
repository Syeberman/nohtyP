
#include "munit_test/unittest.h"


// FIXME fixture? data? user_data?
static MunitResult test_concat(const MunitParameter params[], void *fixture)
{
    ypObject *result;

    result = yp_concat(yp_tuple_empty, yp_tuple_empty);
    assert_int(yp_lenC(result, NULL), ==, 0);

    return MUNIT_OK;
}

MunitTest test_sequence_tests[] = {{
                                           "/test_concat",              // name
                                           (MunitTestFunc)test_concat,  // test
                                           (MunitTestSetup)NULL,        // setup
                                           (MunitTestTearDown)NULL,     // tear_down
                                           MUNIT_TEST_OPTION_NONE,      // options
                                           NULL                         // parameters
                                   },
        {NULL}};
