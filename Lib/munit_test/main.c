
#include "munit_test/unittest.h"
#include <stdlib.h>


static MunitSuite munit_test_suite = {
        "",                     /* prefix */
        NULL,                   /* tests */
        munit_test_suites,      /* suites */
        1,                      /* iterations */
        MUNIT_SUITE_OPTION_NONE /* options */
};

int main(int argc, char **argv)
{
    disable_debugger_popups();

    {
        yp_initialize_parameters_t args = {sizeof(yp_initialize_parameters_t),
                malloc_tracker_malloc, malloc_tracker_malloc_resize, malloc_tracker_free};
        yp_initialize(&args);
    }

    unittest_initialize();

    munit_test_initialize();

    {
        void *user_data = NULL;
        return munit_suite_main(&munit_test_suite, user_data, argc, argv);
    }
}
