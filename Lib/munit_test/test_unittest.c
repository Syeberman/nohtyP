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

// TODO Run a few iterations of this?
static MunitResult test_rand_obj(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    {
        ypObject *any = rand_obj_any();  // not dependent on type
        assert_not_exception(any);
        yp_decref(any);
    }

    {
        ypObject *hashable = rand_obj_any_hashable();  // not dependent on type
        assert_not_exception(hashable);
        assert_not_raises_exc(yp_hashC(hashable, &exc));
        yp_decref(hashable);
    }

    {
        ypObject *of_type = rand_obj(type);
        assert_not_exception(of_type);
        assert_type_is(of_type, type->type);
        yp_decref(of_type);
    }

    if (!type->is_mutable) {
        ypObject *hashable_of_type = rand_obj_hashable(type);
        assert_not_exception(hashable_of_type);
        assert_type_is(hashable_of_type, type->type);
        assert_not_raises_exc(yp_hashC(hashable_of_type, &exc));
        yp_decref(hashable_of_type);
    }

    return MUNIT_OK;
}


static MunitParameterEnum test_types_all_params[] = {
        {param_key_type, param_values_types_all}, {NULL}};

MunitTest test_unittest_tests[] = {TEST(test_fixture_types, NULL), TEST(test_param_type, NULL),
        TEST(test_rand_obj, test_types_all_params), {NULL}};
