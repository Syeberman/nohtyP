
#include "munit_test/unittest.h"


static MunitResult test_bool(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    uniqueness_t   *uq = uniqueness_new();
    ypObject       *items[1];
    obj_array_fill(items, uq, type->rand_items);

    // Empty collections are falsy, all others are truthy.
    ead(x, type->newN(1, items[0]), assert_obj(yp_bool(x), is, yp_True));
    ead(x, type->newN(0), assert_obj(yp_bool(x), is, yp_False));

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_contains(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    uniqueness_t   *uq = uniqueness_new();
    ypObject       *items[4];
    obj_array_fill(items, uq, type->rand_items);

    // Basic contains (and in and not_in).
    {
        ypObject *self = type->newN(N(items[0], items[1]));
        assert_obj(yp_contains(self, items[1]), is, yp_True);
        assert_obj(yp_in(items[1], self), is, yp_True);
        assert_obj(yp_not_in(items[1], self), is, yp_False);
        yp_decrefN(N(self));
    }

    // x not in self.
    {
        ypObject *self = type->newN(N(items[0], items[1]));
        assert_obj(yp_contains(self, items[2]), is, yp_False);
        assert_obj(yp_in(items[2], self), is, yp_False);
        assert_obj(yp_not_in(items[2], self), is, yp_True);
        yp_decrefN(N(self));
    }

    // Previously-deleted item.
    if (type->is_mutable) {
        ypObject *self = type->newN(N(items[0]));
        assert_not_raises_exc(yp_clear(self, &exc));
        assert_obj(yp_contains(self, items[0]), is, yp_False);
        assert_obj(yp_in(items[0], self), is, yp_False);
        assert_obj(yp_not_in(items[0], self), is, yp_True);
        yp_decrefN(N(self));
    }

    // self is empty.
    {
        ypObject *self = type->newN(0);
        assert_obj(yp_contains(self, items[0]), is, yp_False);
        assert_obj(yp_in(items[0], self), is, yp_False);
        assert_obj(yp_not_in(items[0], self), is, yp_True);
        yp_decrefN(N(self));
    }

    // x is self. Recall `"abc" in "abc"` is True for strings.
    {
        ypObject *self = type->newN(N(items[0], items[1]));
        assert_obj(yp_contains(self, self), is, type->is_string ? yp_True : yp_False);
        assert_obj(yp_in(self, self), is, type->is_string ? yp_True : yp_False);
        assert_obj(yp_not_in(self, self), is, type->is_string ? yp_False : yp_True);
        yp_decrefN(N(self));
    }

    // Exception passthrough.
    {
        ypObject *self = type->newN(N(items[0], items[1]));
        ypObject *empty = type->newN(0);
        assert_isexception(yp_contains(self, yp_SyntaxError), yp_SyntaxError);
        assert_isexception(yp_in(yp_SyntaxError, self), yp_SyntaxError);
        assert_isexception(yp_not_in(yp_SyntaxError, self), yp_SyntaxError);
        assert_isexception(yp_contains(empty, yp_SyntaxError), yp_SyntaxError);
        assert_isexception(yp_in(yp_SyntaxError, empty), yp_SyntaxError);
        assert_isexception(yp_not_in(yp_SyntaxError, empty), yp_SyntaxError);
        yp_decrefN(N(self, empty));
    }

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_clear(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    uniqueness_t   *uq = uniqueness_new();
    ypObject       *items[32];
    obj_array_fill(items, uq, type->rand_items);

    // Immutables don't support clear.
    if (!type->is_mutable) {
        ypObject *self = type->newN(N(items[0], items[1]));
        assert_raises_exc(yp_clear(self, &exc), yp_MethodError);
        assert_len(self, 2);
        assert_obj(yp_contains(self, items[0]), is, yp_True);
        assert_obj(yp_contains(self, items[1]), is, yp_True);
        yp_decref(self);
        goto tear_down;  // Skip remaining tests.
    }

    // Basic clear.
    {
        ypObject *self = type->newN(N(items[0], items[1]));
        assert_not_raises_exc(yp_clear(self, &exc));
        assert_len(self, 0);
        assert_obj(yp_contains(self, items[0]), is, yp_False);
        assert_obj(yp_contains(self, items[1]), is, yp_False);
        yp_decref(self);
    }

    // self is empty.
    {
        ypObject *self = type->newN(0);
        assert_not_raises_exc(yp_clear(self, &exc));
        assert_len(self, 0);
        assert_obj(yp_contains(self, items[0]), is, yp_False);
        assert_obj(yp_contains(self, items[1]), is, yp_False);
        yp_decref(self);
    }

    // Clear a large collection (with a separate ob_data).
    {
        ypObject *self = type->newN(N(items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[8], items[9], items[10], items[11], items[12], items[13],
                items[14], items[15], items[16], items[17], items[18], items[19], items[20],
                items[21], items[22], items[23], items[24], items[25], items[26], items[27],
                items[28], items[29], items[30], items[31]));
        assert_not_raises_exc(yp_clear(self, &exc));
        assert_len(self, 0);
        assert_obj(yp_contains(self, items[0]), is, yp_False);
        assert_obj(yp_contains(self, items[1]), is, yp_False);
        yp_decref(self);
    }

tear_down:
    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

// TODO Enable and expand this test. One issue is that rand_item may return items that don't
// support deepcopy, like iterators.
// static MunitResult test_deepcopy(const MunitParameter params[], fixture_t *fixture)
// {
//     fixture_type_t *type = fixture->type;
//     uniqueness_t   *uq = uniqueness_new();
//     ypObject       *items[4];
//     obj_array_fill(items, uq, type->rand_items);
//
//     // Basic deepcopy. Recall immortals may not actually be copied, and that newN might return an
//     // immortal for empty or even single-item collections. But four-item collections are unlikely
//     // to be immortals.
//     {
//         ypObject *self = type->newN(N(items[0], items[1], items[2], items[3]));
//         ypObject *copy;
//
//         assert_not_raises(copy = yp_deepcopy(self));
//
//         assert_obj(copy, is_not, self);
//         assert_obj(copy, eq, self);
//         assert_len(copy, 4);
//         assert_obj(items[0], in, copy);
//         assert_obj(items[1], in, copy);
//         assert_obj(items[2], in, copy);
//         assert_obj(items[3], in, copy);
//
//         yp_decrefN(N(self, copy));
//     }
//
//     obj_array_decref(items);
//     uniqueness_dealloc(uq);
//     return MUNIT_OK;
// }


static MunitParameterEnum test_collection_params[] = {
        {param_key_type, param_values_types_collection}, {NULL}};

MunitTest test_collection_tests[] = {TEST(test_bool, test_collection_params),
        TEST(test_contains, test_collection_params), TEST(test_clear, test_collection_params),
        {NULL}};

extern void test_collection_initialize(void) {}
