/*
 * Testing unittest.c, unittest.h, and other munit_test self-tests.
 */

#include "munit_test/unittest.h"


// Ensure PRIint, PRIssize, and similar are correct. Part of this test is ensuring it passes the
// compiler's format string checks. Recall that these macros are copied in nohtyP.c and unittest.h.
static MunitResult test_PRI_formats(const MunitParameter params[], fixture_t *fixture)
{
#define assert_PRI_format(fmt, T, _value, _expected)                                           \
    do {                                                                                       \
        T          value = _value;                                                             \
        char       expected[] = _expected;                                                     \
        yp_ssize_t expected_len = yp_lengthof_array(expected) - 1;                             \
        char       buffer[64];                                                                 \
        yp_ssize_t result = (yp_ssize_t)unittest_snprintf(buffer, sizeof(buffer), fmt, value); \
        assert_ssizeC(result, ==, expected_len);                                               \
        munit_assert_string_equal(buffer, expected);                                           \
    } while (0)

    // PRIint
    assert_PRI_format("%" PRIint, yp_int_t, -1, "-1");
    assert_PRI_format("%" PRIint, yp_int_t, 0x0102030405060708LL, "72623859790382856");

    // PRIssize
    assert_PRI_format("%" PRIssize, yp_ssize_t, -1, "-1");
#ifdef yp_ARCH_32_BIT
    assert_PRI_format("%" PRIssize, yp_ssize_t, 0x01020304, "16909060");
#else
    assert_PRI_format("%" PRIssize, yp_ssize_t, 0x0102030405060708LL, "72623859790382856");
#endif

#undef assert_PRI_format
    return MUNIT_OK;
}

// Ensure the various fixture_types_* arrays were initialzed properly.
static MunitResult test_fixture_types(const MunitParameter params[], fixture_t *fixture)
{
    assert_ssizeC(fixture_types_all->len, ==, 24);
    assert_ptr_array(fixture_types_all->types, fixture_type_type, fixture_type_NoneType,
            fixture_type_bool, fixture_type_int, fixture_type_intstore, fixture_type_float,
            fixture_type_floatstore, fixture_type_iter, fixture_type_range, fixture_type_bytes,
            fixture_type_bytearray, fixture_type_str, fixture_type_chrarray, fixture_type_tuple,
            fixture_type_list, fixture_type_frozenset, fixture_type_set,
            fixture_type_frozenset_dirty, fixture_type_set_dirty, fixture_type_frozendict,
            fixture_type_dict, fixture_type_frozendict_dirty, fixture_type_dict_dirty,
            fixture_type_function, NULL);

    assert_ssizeC(fixture_types_mutable->len, ==, 9);
    assert_ptr_array(fixture_types_mutable->types, fixture_type_intstore, fixture_type_floatstore,
            fixture_type_bytearray, fixture_type_chrarray, fixture_type_list, fixture_type_set,
            fixture_type_set_dirty, fixture_type_dict, fixture_type_dict_dirty, NULL);

    assert_ssizeC(fixture_types_numeric->len, ==, 4);
    assert_ptr_array(fixture_types_numeric->types, fixture_type_int, fixture_type_intstore,
            fixture_type_float, fixture_type_floatstore, NULL);

    assert_ssizeC(fixture_types_iterable->len, ==, 16);
    assert_ptr_array(fixture_types_iterable->types, fixture_type_iter, fixture_type_range,
            fixture_type_bytes, fixture_type_bytearray, fixture_type_str, fixture_type_chrarray,
            fixture_type_tuple, fixture_type_list, fixture_type_frozenset, fixture_type_set,
            fixture_type_frozenset_dirty, fixture_type_set_dirty, fixture_type_frozendict,
            fixture_type_dict, fixture_type_frozendict_dirty, fixture_type_dict_dirty, NULL);

    assert_ssizeC(fixture_types_collection->len, ==, 15);
    assert_ptr_array(fixture_types_collection->types, fixture_type_range, fixture_type_bytes,
            fixture_type_bytearray, fixture_type_str, fixture_type_chrarray, fixture_type_tuple,
            fixture_type_list, fixture_type_frozenset, fixture_type_set,
            fixture_type_frozenset_dirty, fixture_type_set_dirty, fixture_type_frozendict,
            fixture_type_dict, fixture_type_frozendict_dirty, fixture_type_dict_dirty, NULL);

    assert_ssizeC(fixture_types_sequence->len, ==, 7);
    assert_ptr_array(fixture_types_sequence->types, fixture_type_range, fixture_type_bytes,
            fixture_type_bytearray, fixture_type_str, fixture_type_chrarray, fixture_type_tuple,
            fixture_type_list, NULL);

    assert_ssizeC(fixture_types_string->len, ==, 4);
    assert_ptr_array(fixture_types_string->types, fixture_type_bytes, fixture_type_bytearray,
            fixture_type_str, fixture_type_chrarray, NULL);

    assert_ssizeC(fixture_types_setlike->len, ==, 4);
    assert_ptr_array(fixture_types_setlike->types, fixture_type_frozenset, fixture_type_set,
            fixture_type_frozenset_dirty, fixture_type_set_dirty, NULL);

    assert_ssizeC(fixture_types_mapping->len, ==, 4);
    assert_ptr_array(fixture_types_mapping->types, fixture_type_frozendict, fixture_type_dict,
            fixture_type_frozendict_dirty, fixture_type_dict_dirty, NULL);

    assert_ssizeC(fixture_types_immutable->len, ==, 15);
    assert_ptr_array(fixture_types_immutable->types, fixture_type_type, fixture_type_NoneType,
            fixture_type_bool, fixture_type_int, fixture_type_float, fixture_type_iter,
            fixture_type_range, fixture_type_bytes, fixture_type_str, fixture_type_tuple,
            fixture_type_frozenset, fixture_type_frozenset_dirty, fixture_type_frozendict,
            fixture_type_frozendict_dirty, fixture_type_function, NULL);

    assert_ssizeC(fixture_types_not_numeric->len, ==, 20);
    assert_ptr_array(fixture_types_not_numeric->types, fixture_type_type, fixture_type_NoneType,
            fixture_type_bool, fixture_type_iter, fixture_type_range, fixture_type_bytes,
            fixture_type_bytearray, fixture_type_str, fixture_type_chrarray, fixture_type_tuple,
            fixture_type_list, fixture_type_frozenset, fixture_type_set,
            fixture_type_frozenset_dirty, fixture_type_set_dirty, fixture_type_frozendict,
            fixture_type_dict, fixture_type_frozendict_dirty, fixture_type_dict_dirty,
            fixture_type_function, NULL);

    assert_ssizeC(fixture_types_not_iterable->len, ==, 8);
    assert_ptr_array(fixture_types_not_iterable->types, fixture_type_type, fixture_type_NoneType,
            fixture_type_bool, fixture_type_int, fixture_type_intstore, fixture_type_float,
            fixture_type_floatstore, fixture_type_function, NULL);

    assert_ssizeC(fixture_types_not_collection->len, ==, 9);
    assert_ptr_array(fixture_types_not_collection->types, fixture_type_type, fixture_type_NoneType,
            fixture_type_bool, fixture_type_int, fixture_type_intstore, fixture_type_float,
            fixture_type_floatstore, fixture_type_iter, fixture_type_function, NULL);

    assert_ssizeC(fixture_types_not_sequence->len, ==, 17);
    assert_ptr_array(fixture_types_not_sequence->types, fixture_type_type, fixture_type_NoneType,
            fixture_type_bool, fixture_type_int, fixture_type_intstore, fixture_type_float,
            fixture_type_floatstore, fixture_type_iter, fixture_type_frozenset, fixture_type_set,
            fixture_type_frozenset_dirty, fixture_type_set_dirty, fixture_type_frozendict,
            fixture_type_dict, fixture_type_frozendict_dirty, fixture_type_dict_dirty,
            fixture_type_function, NULL);

    assert_ssizeC(fixture_types_not_string->len, ==, 20);
    assert_ptr_array(fixture_types_not_string->types, fixture_type_type, fixture_type_NoneType,
            fixture_type_bool, fixture_type_int, fixture_type_intstore, fixture_type_float,
            fixture_type_floatstore, fixture_type_iter, fixture_type_range, fixture_type_tuple,
            fixture_type_list, fixture_type_frozenset, fixture_type_set,
            fixture_type_frozenset_dirty, fixture_type_set_dirty, fixture_type_frozendict,
            fixture_type_dict, fixture_type_frozendict_dirty, fixture_type_dict_dirty,
            fixture_type_function, NULL);

    assert_ssizeC(fixture_types_not_setlike->len, ==, 20);
    assert_ptr_array(fixture_types_not_setlike->types, fixture_type_type, fixture_type_NoneType,
            fixture_type_bool, fixture_type_int, fixture_type_intstore, fixture_type_float,
            fixture_type_floatstore, fixture_type_iter, fixture_type_range, fixture_type_bytes,
            fixture_type_bytearray, fixture_type_str, fixture_type_chrarray, fixture_type_tuple,
            fixture_type_list, fixture_type_frozendict, fixture_type_dict,
            fixture_type_frozendict_dirty, fixture_type_dict_dirty, fixture_type_function, NULL);

    assert_ssizeC(fixture_types_not_mapping->len, ==, 20);
    assert_ptr_array(fixture_types_not_mapping->types, fixture_type_type, fixture_type_NoneType,
            fixture_type_bool, fixture_type_int, fixture_type_intstore, fixture_type_float,
            fixture_type_floatstore, fixture_type_iter, fixture_type_range, fixture_type_bytes,
            fixture_type_bytearray, fixture_type_str, fixture_type_chrarray, fixture_type_tuple,
            fixture_type_list, fixture_type_frozenset, fixture_type_set,
            fixture_type_frozenset_dirty, fixture_type_set_dirty, fixture_type_function, NULL);

    assert_ssizeC(fixture_types_immutable_not_str->len, ==, 14);
    assert_ptr_array(fixture_types_immutable_not_str->types, fixture_type_type,
            fixture_type_NoneType, fixture_type_bool, fixture_type_int, fixture_type_float,
            fixture_type_iter, fixture_type_range, fixture_type_bytes, fixture_type_tuple,
            fixture_type_frozenset, fixture_type_frozenset_dirty, fixture_type_frozendict,
            fixture_type_frozendict_dirty, fixture_type_function, NULL);

    assert_ssizeC(fixture_types_immutable_paired->len, ==, 9);
    assert_ptr_array(fixture_types_immutable_paired->types, fixture_type_int, fixture_type_float,
            fixture_type_bytes, fixture_type_str, fixture_type_tuple, fixture_type_frozenset,
            fixture_type_frozenset_dirty, fixture_type_frozendict, fixture_type_frozendict_dirty,
            NULL);

    return MUNIT_OK;
}

// Ensure the various param_values_types_* arrays were initialzed properly.
static MunitResult test_param_values_types(const MunitParameter params[], fixture_t *fixture)
{
    assert_ptr_array(param_values_types_all, fixture_type_type->name, fixture_type_NoneType->name,
            fixture_type_bool->name, fixture_type_int->name, fixture_type_intstore->name,
            fixture_type_float->name, fixture_type_floatstore->name, fixture_type_iter->name,
            fixture_type_range->name, fixture_type_bytes->name, fixture_type_bytearray->name,
            fixture_type_str->name, fixture_type_chrarray->name, fixture_type_tuple->name,
            fixture_type_list->name, fixture_type_frozenset->name, fixture_type_set->name,
            fixture_type_frozenset_dirty->name, fixture_type_set_dirty->name,
            fixture_type_frozendict->name, fixture_type_dict->name,
            fixture_type_frozendict_dirty->name, fixture_type_dict_dirty->name,
            fixture_type_function->name, NULL);

    assert_ptr_array(param_values_types_mutable, fixture_type_intstore->name,
            fixture_type_floatstore->name, fixture_type_bytearray->name,
            fixture_type_chrarray->name, fixture_type_list->name, fixture_type_set->name,
            fixture_type_set_dirty->name, fixture_type_dict->name, fixture_type_dict_dirty->name,
            NULL);

    assert_ptr_array(param_values_types_numeric, fixture_type_int->name,
            fixture_type_intstore->name, fixture_type_float->name, fixture_type_floatstore->name,
            NULL);

    assert_ptr_array(param_values_types_iterable, fixture_type_iter->name, fixture_type_range->name,
            fixture_type_bytes->name, fixture_type_bytearray->name, fixture_type_str->name,
            fixture_type_chrarray->name, fixture_type_tuple->name, fixture_type_list->name,
            fixture_type_frozenset->name, fixture_type_set->name,
            fixture_type_frozenset_dirty->name, fixture_type_set_dirty->name,
            fixture_type_frozendict->name, fixture_type_dict->name,
            fixture_type_frozendict_dirty->name, fixture_type_dict_dirty->name, NULL);

    assert_ptr_array(param_values_types_collection, fixture_type_range->name,
            fixture_type_bytes->name, fixture_type_bytearray->name, fixture_type_str->name,
            fixture_type_chrarray->name, fixture_type_tuple->name, fixture_type_list->name,
            fixture_type_frozenset->name, fixture_type_set->name,
            fixture_type_frozenset_dirty->name, fixture_type_set_dirty->name,
            fixture_type_frozendict->name, fixture_type_dict->name,
            fixture_type_frozendict_dirty->name, fixture_type_dict_dirty->name, NULL);

    assert_ptr_array(param_values_types_sequence, fixture_type_range->name,
            fixture_type_bytes->name, fixture_type_bytearray->name, fixture_type_str->name,
            fixture_type_chrarray->name, fixture_type_tuple->name, fixture_type_list->name, NULL);

    assert_ptr_array(param_values_types_string, fixture_type_bytes->name,
            fixture_type_bytearray->name, fixture_type_str->name, fixture_type_chrarray->name,
            NULL);

    assert_ptr_array(param_values_types_setlike, fixture_type_frozenset->name,
            fixture_type_set->name, fixture_type_frozenset_dirty->name,
            fixture_type_set_dirty->name, NULL);

    assert_ptr_array(param_values_types_mapping, fixture_type_frozendict->name,
            fixture_type_dict->name, fixture_type_frozendict_dirty->name,
            fixture_type_dict_dirty->name, NULL);

    assert_ptr_array(param_values_types_immutable, fixture_type_type->name,
            fixture_type_NoneType->name, fixture_type_bool->name, fixture_type_int->name,
            fixture_type_float->name, fixture_type_iter->name, fixture_type_range->name,
            fixture_type_bytes->name, fixture_type_str->name, fixture_type_tuple->name,
            fixture_type_frozenset->name, fixture_type_frozenset_dirty->name,
            fixture_type_frozendict->name, fixture_type_frozendict_dirty->name,
            fixture_type_function->name, NULL);

    assert_ptr_array(param_values_types_not_numeric, fixture_type_type->name,
            fixture_type_NoneType->name, fixture_type_bool->name, fixture_type_iter->name,
            fixture_type_range->name, fixture_type_bytes->name, fixture_type_bytearray->name,
            fixture_type_str->name, fixture_type_chrarray->name, fixture_type_tuple->name,
            fixture_type_list->name, fixture_type_frozenset->name, fixture_type_set->name,
            fixture_type_frozenset_dirty->name, fixture_type_set_dirty->name,
            fixture_type_frozendict->name, fixture_type_dict->name,
            fixture_type_frozendict_dirty->name, fixture_type_dict_dirty->name,
            fixture_type_function->name, NULL);

    assert_ptr_array(param_values_types_not_iterable, fixture_type_type->name,
            fixture_type_NoneType->name, fixture_type_bool->name, fixture_type_int->name,
            fixture_type_intstore->name, fixture_type_float->name, fixture_type_floatstore->name,
            fixture_type_function->name, NULL);

    assert_ptr_array(param_values_types_not_collection, fixture_type_type->name,
            fixture_type_NoneType->name, fixture_type_bool->name, fixture_type_int->name,
            fixture_type_intstore->name, fixture_type_float->name, fixture_type_floatstore->name,
            fixture_type_iter->name, fixture_type_function->name, NULL);

    assert_ptr_array(param_values_types_not_sequence, fixture_type_type->name,
            fixture_type_NoneType->name, fixture_type_bool->name, fixture_type_int->name,
            fixture_type_intstore->name, fixture_type_float->name, fixture_type_floatstore->name,
            fixture_type_iter->name, fixture_type_frozenset->name, fixture_type_set->name,
            fixture_type_frozenset_dirty->name, fixture_type_set_dirty->name,
            fixture_type_frozendict->name, fixture_type_dict->name,
            fixture_type_frozendict_dirty->name, fixture_type_dict_dirty->name,
            fixture_type_function->name, NULL);

    assert_ptr_array(param_values_types_not_string, fixture_type_type->name,
            fixture_type_NoneType->name, fixture_type_bool->name, fixture_type_int->name,
            fixture_type_intstore->name, fixture_type_float->name, fixture_type_floatstore->name,
            fixture_type_iter->name, fixture_type_range->name, fixture_type_tuple->name,
            fixture_type_list->name, fixture_type_frozenset->name, fixture_type_set->name,
            fixture_type_frozenset_dirty->name, fixture_type_set_dirty->name,
            fixture_type_frozendict->name, fixture_type_dict->name,
            fixture_type_frozendict_dirty->name, fixture_type_dict_dirty->name,
            fixture_type_function->name, NULL);

    assert_ptr_array(param_values_types_not_setlike, fixture_type_type->name,
            fixture_type_NoneType->name, fixture_type_bool->name, fixture_type_int->name,
            fixture_type_intstore->name, fixture_type_float->name, fixture_type_floatstore->name,
            fixture_type_iter->name, fixture_type_range->name, fixture_type_bytes->name,
            fixture_type_bytearray->name, fixture_type_str->name, fixture_type_chrarray->name,
            fixture_type_tuple->name, fixture_type_list->name, fixture_type_frozendict->name,
            fixture_type_dict->name, fixture_type_frozendict_dirty->name,
            fixture_type_dict_dirty->name, fixture_type_function->name, NULL);

    assert_ptr_array(param_values_types_not_mapping, fixture_type_type->name,
            fixture_type_NoneType->name, fixture_type_bool->name, fixture_type_int->name,
            fixture_type_intstore->name, fixture_type_float->name, fixture_type_floatstore->name,
            fixture_type_iter->name, fixture_type_range->name, fixture_type_bytes->name,
            fixture_type_bytearray->name, fixture_type_str->name, fixture_type_chrarray->name,
            fixture_type_tuple->name, fixture_type_list->name, fixture_type_frozenset->name,
            fixture_type_set->name, fixture_type_frozenset_dirty->name,
            fixture_type_set_dirty->name, fixture_type_function->name, NULL);

    assert_ptr_array(param_values_types_immutable_not_str, fixture_type_type->name,
            fixture_type_NoneType->name, fixture_type_bool->name, fixture_type_int->name,
            fixture_type_float->name, fixture_type_iter->name, fixture_type_range->name,
            fixture_type_bytes->name, fixture_type_tuple->name, fixture_type_frozenset->name,
            fixture_type_frozenset_dirty->name, fixture_type_frozendict->name,
            fixture_type_frozendict_dirty->name, fixture_type_function->name, NULL);

    assert_ptr_array(param_values_types_immutable_paired, fixture_type_int->name,
            fixture_type_float->name, fixture_type_bytes->name, fixture_type_str->name,
            fixture_type_tuple->name, fixture_type_frozenset->name,
            fixture_type_frozenset_dirty->name, fixture_type_frozendict->name,
            fixture_type_frozendict_dirty->name, NULL);

    return MUNIT_OK;
}

// Tests the properties of each fixture type.
static MunitResult test_fixture_type(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    assert_not_null(type->name);
    assert_type_is(type->type, yp_t_type);
    if (type->is_mutable) {
        assert_null(type->falsy);
    } else if (type->falsy != NULL) {
        assert_type_is(type->falsy, type->type);
    }
    assert_not_null(type->pair);
    // Property flags are tested implicitly by test_fixture_types.

    return MUNIT_OK;
}

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

MunitTest test_unittest_tests[] = {TEST(test_PRI_formats, NULL), TEST(test_fixture_types, NULL),
        TEST(test_param_values_types, NULL), TEST(test_fixture_type, test_types_all_params),
        TEST(test_rand_obj, test_types_all_params), {NULL}};


extern void test_unittest_initialize(void) {}
