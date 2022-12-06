
#include "munit_test/unittest.h"


static MunitResult test_concat(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    obj_array_init(items, 4, type->rand_item());

    // Not all sequences support concatenation. For example, range only stores integers following a
    // given pattern. "Insertion ordered" is the closest I've come to describing the difference.
    if (!type->is_insertion_ordered) {
        ypObject *obj = rand_obj(type);
        ypObject *result = yp_concat(obj, obj);
        assert_isexception2(result, yp_MethodError);
        yp_decrefN(2, obj, result);
        goto tear_down;
    }

    {
        ypObject *first = type->newN(2, items[0], items[1]);
        ypObject *second = type->newN(2, items[2], items[3]);
        ypObject *result = yp_concat(first, second);
        assert_sequence(result, 4, items[0], items[1], items[2], items[3]);
        yp_decrefN(2, first, second, result);
    }

tear_down:
    obj_array_fini(items);
    return MUNIT_OK;
}

static MunitParameterEnum test_sequence_params[] = {
        {param_key_type, param_values_types_sequence}, {NULL}};

MunitTest test_sequence_tests[] = {TEST(test_concat, test_sequence_params), {NULL}};
