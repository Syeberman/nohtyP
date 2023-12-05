
#include "munit_test/unittest.h"


// TODO Enable and expand this test. One issue is that rand_items may return items that don't
// support deepcopy, like iterators.
// static MunitResult test_deepcopy(const MunitParameter params[], fixture_t *fixture)
// {
//     fixture_type_t *type = fixture->type;
//     ypObject       *items[4];
//     obj_array_fill(items, type->rand_items);

//     // Basic deepcopy. Recall immortals may not actually be copied, and that newN might return an
//     // immortal for empty or even single-item collections. But four-item collections are unlikely to
//     // be immortals.
//     {
//         ypObject *self = type->newN(N(items[0], items[1], items[2], items[3]));
//         ypObject *copy;

//         assert_not_raises(copy = yp_deepcopy(self));

//         assert_obj(copy, is_not, self);
//         assert_obj(copy, eq, self);
//         assert_len(copy, 4);
//         assert_obj(items[0], in, copy);
//         assert_obj(items[1], in, copy);
//         assert_obj(items[2], in, copy);
//         assert_obj(items[3], in, copy);

//         yp_decrefN(N(self, copy));
//     }

//     obj_array_decref(items);
//     return MUNIT_OK;
// }

static MunitResult test_bool(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *item = type->rand_item();

    // Empty collections are falsy, all others are truthy.
    ead(x, type->newN(1, item), assert_obj(yp_bool(x), is, yp_True));
    ead(x, type->newN(0), assert_obj(yp_bool(x), is, yp_False));

    yp_decrefN(N(item));
    return MUNIT_OK;
}


static MunitParameterEnum test_collection_params[] = {
        {param_key_type, param_values_types_collection}, {NULL}};

MunitTest test_collection_tests[] = {TEST(test_bool, test_collection_params), {NULL}};

extern void test_collection_initialize(void) {}
