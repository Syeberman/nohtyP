
#include "munit_test/unittest.h"


// Shared tests for yp_findC5, yp_indexC5, yp_rfindC5, yp_rindexC5, etc. The _test_findC in
// test_sequence checks for the behaviour shared amongst all sequences; this _test_findC considers
// the behaviour unique to strings, namely substring matching.
static MunitResult _test_findC(fixture_type_t *type,
        yp_ssize_t (*any_findC)(ypObject *, ypObject *, ypObject **),
        yp_ssize_t (*any_findC5)(ypObject *, ypObject *, yp_ssize_t, yp_ssize_t, ypObject **),
        int forward, int raises)
{
    ypObject *items[3];
    ypObject *self;
    ypObject *other_0_1;
    ypObject *other_1_2;
    ypObject *other_0_2;
    ypObject *other_1_0;
    ypObject *empty = type->newN(0);

    obj_array_fill(items, type->rand_items);
    self = type->newN(N(items[0], items[1], items[2]));
    // FIXME Test against different "other" types (the other pair, really)
    other_0_1 = type->newN(N(items[0], items[1]));
    other_1_2 = type->newN(N(items[1], items[2]));
    other_0_2 = type->newN(N(items[0], items[2]));
    other_1_0 = type->newN(N(items[1], items[0]));

#define assert_not_found_exc(expression)                    \
    do {                                                    \
        ypObject *exc = yp_None;                            \
        assert_ssizeC(expression, ==, -1);                  \
        if (raises) assert_isexception(exc, yp_ValueError); \
    } while (0)

    assert_ssizeC_exc(any_findC(self, other_0_1, &exc), ==, 0);            // Sub-string.
    assert_ssizeC_exc(any_findC(self, other_1_2, &exc), ==, 1);            // Sub-string.
    assert_not_found_exc(any_findC(self, other_0_2, &exc));                // Out-of-order.
    assert_not_found_exc(any_findC(self, other_1_0, &exc));                // Out-of-order.
    assert_ssizeC_exc(any_findC(self, empty, &exc), ==, forward ? 0 : 3);  // Empty.
    assert_ssizeC_exc(any_findC(self, self, &exc), ==, 0);                 // Self.

    assert_ssizeC_exc(any_findC5(self, other_0_1, 0, 3, &exc), ==, 0);  // Total slice.
    assert_ssizeC_exc(any_findC5(self, other_0_1, 0, 2, &exc), ==, 0);  // Exact slice.
    assert_not_found_exc(any_findC5(self, other_0_1, 0, 1, &exc));      // Too-small slice.
    assert_not_found_exc(any_findC5(self, other_0_1, 0, 0, &exc));      // Empty slice.

    assert_ssizeC_exc(any_findC5(self, other_1_2, 1, 3, &exc), ==, 1);  // Exact slice.
    assert_not_found_exc(any_findC5(self, other_1_2, 1, 2, &exc));      // Too-small slice.
    assert_not_found_exc(any_findC5(self, other_1_2, 1, 1, &exc));      // Empty slice.

    assert_ssizeC_exc(any_findC5(self, empty, 0, 3, &exc), ==, forward ? 0 : 3);  // Empty, total.
    assert_ssizeC_exc(any_findC5(self, empty, 1, 2, &exc), ==, forward ? 1 : 2);  // Empty, partial.
    assert_ssizeC_exc(any_findC5(self, empty, 2, 2, &exc), ==, 2);                // Empty, empty.

    // FIXME That empty slice bug thing.
    // FIXME !forward substrings?
    // FIXME Anything else to add here?

#undef assert_not_found_exc

    obj_array_decref(items);
    yp_decrefN(N(self, empty, other_0_1, other_1_2, other_0_2, other_1_0));
    return MUNIT_OK;
}

static MunitResult test_findC(const MunitParameter params[], fixture_t *fixture)
{
    return _test_findC(fixture->type, yp_findC, yp_findC5, /*forward=*/TRUE, /*raises=*/FALSE);
}

static MunitResult test_indexC(const MunitParameter params[], fixture_t *fixture)
{
    return _test_findC(fixture->type, yp_indexC, yp_indexC5, /*forward=*/TRUE, /*raises=*/TRUE);
}

static MunitResult test_rfindC(const MunitParameter params[], fixture_t *fixture)
{
    return _test_findC(fixture->type, yp_rfindC, yp_rfindC5, /*forward=*/FALSE, /*raises=*/FALSE);
}

static MunitResult test_rindexC(const MunitParameter params[], fixture_t *fixture)
{
    return _test_findC(fixture->type, yp_rindexC, yp_rindexC5, /*forward=*/FALSE, /*raises=*/TRUE);
}

// FIXME test_countC, for non-overlapping substrings.

// FIXME test_remove and test_discard, for substrings.

static MunitParameterEnum test_string_params[] = {
        {param_key_type, param_values_types_string}, {NULL}};

MunitTest test_string_tests[] = {TEST(test_findC, test_string_params),
        TEST(test_indexC, test_string_params), TEST(test_rfindC, test_string_params),
        TEST(test_rindexC, test_string_params), {NULL}};


extern void test_string_initialize(void) {}
