/*
 * Testing unittest.c, unittest.h, and other munit_test self-tests.
 */

#include "munit_test/unittest.h"


// Ensure the various fixture_types_* arrays were initialzed properly.
static MunitResult test_fixture_types(const MunitParameter params[], fixture_t *fixture)
{
    assert_ptr_array(fixture_types_all, 21, &fixture_type_type, &fixture_type_NoneType,
            &fixture_type_bool, &fixture_type_int, &fixture_type_intstore, &fixture_type_float,
            &fixture_type_floatstore, &fixture_type_iter, &fixture_type_range, &fixture_type_bytes,
            &fixture_type_bytearray, &fixture_type_str, &fixture_type_chrarray, &fixture_type_tuple,
            &fixture_type_list, &fixture_type_frozenset, &fixture_type_set,
            &fixture_type_frozendict, &fixture_type_dict, &fixture_type_function, NULL);

    assert_ptr_array(fixture_types_numeric, 5, &fixture_type_int, &fixture_type_intstore,
            &fixture_type_float, &fixture_type_floatstore, NULL);

    assert_ptr_array(fixture_types_iterable, 13, &fixture_type_iter, &fixture_type_range,
            &fixture_type_bytes, &fixture_type_bytearray, &fixture_type_str, &fixture_type_chrarray,
            &fixture_type_tuple, &fixture_type_list, &fixture_type_frozenset, &fixture_type_set,
            &fixture_type_frozendict, &fixture_type_dict, NULL);

    assert_ptr_array(fixture_types_collection, 12, &fixture_type_range, &fixture_type_bytes,
            &fixture_type_bytearray, &fixture_type_str, &fixture_type_chrarray, &fixture_type_tuple,
            &fixture_type_list, &fixture_type_frozenset, &fixture_type_set,
            &fixture_type_frozendict, &fixture_type_dict, NULL);

    assert_ptr_array(fixture_types_sequence, 8, &fixture_type_range, &fixture_type_bytes,
            &fixture_type_bytearray, &fixture_type_str, &fixture_type_chrarray, &fixture_type_tuple,
            &fixture_type_list, NULL);

    assert_ptr_array(fixture_types_string, 5, &fixture_type_bytes, &fixture_type_bytearray,
            &fixture_type_str, &fixture_type_chrarray, NULL);

    assert_ptr_array(fixture_types_set, 3, &fixture_type_frozenset, &fixture_type_set, NULL);

    assert_ptr_array(fixture_types_mapping, 3, &fixture_type_frozendict, &fixture_type_dict, NULL);

    return MUNIT_OK;
}

// Ensure the various param_values_types_* arrays were initialzed properly.
static MunitResult test_param_type(const MunitParameter params[], fixture_t *fixture)
{
    assert_ptr_array(param_values_types_all, 21, fixture_type_type.name, fixture_type_NoneType.name,
            fixture_type_bool.name, fixture_type_int.name, fixture_type_intstore.name,
            fixture_type_float.name, fixture_type_floatstore.name, fixture_type_iter.name,
            fixture_type_range.name, fixture_type_bytes.name, fixture_type_bytearray.name,
            fixture_type_str.name, fixture_type_chrarray.name, fixture_type_tuple.name,
            fixture_type_list.name, fixture_type_frozenset.name, fixture_type_set.name,
            fixture_type_frozendict.name, fixture_type_dict.name, fixture_type_function.name, NULL);

    assert_ptr_array(param_values_types_numeric, 5, fixture_type_int.name,
            fixture_type_intstore.name, fixture_type_float.name, fixture_type_floatstore.name,
            NULL);

    assert_ptr_array(param_values_types_iterable, 13, fixture_type_iter.name,
            fixture_type_range.name, fixture_type_bytes.name, fixture_type_bytearray.name,
            fixture_type_str.name, fixture_type_chrarray.name, fixture_type_tuple.name,
            fixture_type_list.name, fixture_type_frozenset.name, fixture_type_set.name,
            fixture_type_frozendict.name, fixture_type_dict.name, NULL);

    assert_ptr_array(param_values_types_collection, 12, fixture_type_range.name,
            fixture_type_bytes.name, fixture_type_bytearray.name, fixture_type_str.name,
            fixture_type_chrarray.name, fixture_type_tuple.name, fixture_type_list.name,
            fixture_type_frozenset.name, fixture_type_set.name, fixture_type_frozendict.name,
            fixture_type_dict.name, NULL);

    assert_ptr_array(param_values_types_sequence, 8, fixture_type_range.name,
            fixture_type_bytes.name, fixture_type_bytearray.name, fixture_type_str.name,
            fixture_type_chrarray.name, fixture_type_tuple.name, fixture_type_list.name, NULL);

    assert_ptr_array(param_values_types_string, 5, fixture_type_bytes.name,
            fixture_type_bytearray.name, fixture_type_str.name, fixture_type_chrarray.name, NULL);

    assert_ptr_array(
            param_values_types_set, 3, fixture_type_frozenset.name, fixture_type_set.name, NULL);

    assert_ptr_array(param_values_types_mapping, 3, fixture_type_frozendict.name,
            fixture_type_dict.name, NULL);

    return MUNIT_OK;
}

// Ensure rand_falsy and rand_truthy return falsy and truthy values.
// TODO Run a few iterations of this?
static MunitResult test_rand_falsy_truthy(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Some types don't have falsy values.
    if (type != &fixture_type_type && type != &fixture_type_iter &&
            type != &fixture_type_function) {
        ypObject *falsy = type->rand_falsy();
        assert_falsy(falsy);
        assert_type_is(falsy, type->type);
        yp_decref(falsy);
    }

    // Some types don't have truthy values.
    if (type != &fixture_type_NoneType) {
        ypObject *truthy = type->rand_truthy();
        assert_truthy(truthy);
        assert_type_is(truthy, type->type);
        yp_decref(truthy);
    }

    return MUNIT_OK;
}


static MunitParameterEnum test_types_all_params[] = {
        {param_key_type, param_values_types_all}, {NULL}};

MunitTest test_unittest_tests[] = {TEST(test_fixture_types, NULL), TEST(test_param_type, NULL),
        TEST(test_rand_falsy_truthy, test_types_all_params), {NULL}};
