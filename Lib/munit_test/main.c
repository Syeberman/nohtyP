
#include "munit_test/unittest.h"
#include <stdlib.h>

// TODO Write a pycparser script that ensures all test functions lead back to here. Also to ensure
// the suite names match the directory/file names.
static const MunitSuite test_suite = {
        "",                      // prefix
        NULL,                    // tests
        NULL,                    // suites
        1,                       // iterations
        MUNIT_SUITE_OPTION_NONE  // options
};

int main(int argc, char **argv)
{
    {
        yp_initialize_parameters_t args = {sizeof(yp_initialize_parameters_t)};
        yp_initialize(&args);
    }

    {
        void *user_data = NULL;
        return munit_suite_main(&test_suite, user_data, argc, argv);
    }
}
