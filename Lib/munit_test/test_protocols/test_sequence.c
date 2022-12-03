
#include "munit_test/unittest.h"


static MunitResult test_concat(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

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
        ypObject *obj = rand_obj(type);
        ypObject *result = yp_concat(obj, obj);
        assert_len(result, 0);
        yp_decrefN(2, obj, result);
    }

tear_down:
    return MUNIT_OK;
}

static MunitParameterEnum test_sequence_params[] = {
        {param_key_type, param_values_types_sequence}, {NULL}};

MunitTest test_sequence_tests[] = {TEST(test_concat, test_sequence_params), {NULL}};
