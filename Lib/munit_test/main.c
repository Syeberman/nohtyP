
#include "munit_test/unittest.h"
#include <stdlib.h>

extern MunitSuite munit_test_suites[];

// TODO Write a pycparser script that ensures all test functions lead back to here. Also to ensure
// the suite names match the directory/file names.
static MunitSuite munit_test_suite = {
        "",                     /* prefix */
        NULL,                   /* tests */
        munit_test_suites,      /* suites */
        1,                      /* iterations */
        MUNIT_SUITE_OPTION_NONE /* options */
};

int main(int argc, char **argv)
{
    {
        yp_initialize_parameters_t args = {sizeof(yp_initialize_parameters_t)};
        yp_initialize(&args);
    }

    unittest_initialize();

    {
        void *user_data = NULL;
        return munit_suite_main(&munit_test_suite, user_data, argc, argv);
    }
}
