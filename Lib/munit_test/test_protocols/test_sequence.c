
#include "munit_test/unittest.h"


static MunitResult test_concat(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    {
        ypObject *obj = rand_obj(type);
        ypObject *result = yp_concat(obj, obj);
        assert_len(result, 0);
        yp_decrefN(2, obj, result);
    }

    return MUNIT_OK;
}

static MunitParameterEnum test_sequence_params[] = {
        {param_key_type, param_values_types_sequence}, {NULL}};

MunitTest test_sequence_tests[] = {TEST(test_concat, test_sequence_params), {NULL}};
