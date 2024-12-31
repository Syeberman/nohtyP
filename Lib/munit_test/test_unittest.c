/*
 * Testing unittest.c, unittest.h, and other munit_test self-tests.
 */

#include "munit_test/unittest.h"


// Finds the peer matching type in the null-terminated array of peers.
static peer_type_t *find_peer_type(peer_type_t *peers, fixture_type_t *type)
{
    peer_type_t *peer;
    for (peer = peers; peer->type != NULL; peer++) {
        if (peer->type == type) return peer;
    }
    return NULL;  // GCOVR_EXCL_LINE
}

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

static MunitResult test_assert_setlike_helper(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[2];
    obj_array_fill(items, uq, fixture_type_frozenset->rand_items);

    // "mi is exhausted"
    {
        ypObject   *so = yp_frozensetN(0);
        yp_uint64_t mi_state;
        ypObject   *mi = yp_miniiter(so, &mi_state);
        ypObject   *actual;
        assert_false(_assert_setlike_helper(mi, &mi_state, 0, NULL, &actual, NULL));
        yp_decrefN(N(so, mi));
    }

    // "yp_miniiter_next failed with an exception"
    {
        ypObject   *not_iterable = rand_obj_any_not_iterable(NULL);
        yp_uint64_t mi_state = 0;
        ypObject   *actual;
        assert_true(_assert_setlike_helper(not_iterable, &mi_state, 0, NULL, &actual, NULL));
        assert_isexception(actual, yp_TypeError);
        yp_decref(not_iterable);
    }

    // "equal item found, *items_i is less than n"
    {
        ypObject   *so = yp_frozensetN(N(items[0]));
        yp_uint64_t mi_state;
        ypObject   *mi = yp_miniiter(so, &mi_state);
        ypObject   *actual;
        yp_ssize_t  items_i;
        assert_true(_assert_setlike_helper(mi, &mi_state, 1, items, &actual, &items_i));
        assert_obj(actual, eq, items[0]);
        assert_ssizeC(items_i, ==, 0);
        yp_decrefN(N(so, mi, actual));
    }

    // "equal item not found, *items_i is equal to n"
    {
        ypObject   *so = yp_frozensetN(N(items[1]));
        yp_uint64_t mi_state;
        ypObject   *mi = yp_miniiter(so, &mi_state);
        ypObject   *actual;
        yp_ssize_t  items_i;
        assert_true(_assert_setlike_helper(mi, &mi_state, 1, items, &actual, &items_i));
        assert_obj(actual, eq, items[1]);
        assert_ssizeC(items_i, ==, 1);
        yp_decrefN(N(so, mi, actual));
    }

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}

static MunitResult test_assert_mapping_helper(const MunitParameter params[], fixture_t *fixture)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *keys[2];
    ypObject     *values[2];
    obj_array_fill(keys, uq, fixture_type_frozendict->rand_items);
    obj_array_fill(values, uq, fixture_type_frozendict->rand_values);

    // "mi is exhausted"
    {
        ypObject   *mp = yp_frozendictK(0);
        yp_uint64_t mi_state;
        ypObject   *mi = yp_miniiter_items(mp, &mi_state);
        ypObject   *actual_key;
        ypObject   *actual_value;
        assert_false(
                _assert_mapping_helper(mi, &mi_state, 0, NULL, &actual_key, &actual_value, NULL));
        yp_decrefN(N(mp, mi));
    }

    // "yp_miniiter_items_next failed with an exception"
    {
        ypObject   *not_iterable = rand_obj_any_not_iterable(NULL);
        yp_uint64_t mi_state = 0;
        ypObject   *actual_key;
        ypObject   *actual_value;
        assert_true(_assert_mapping_helper(
                not_iterable, &mi_state, 0, NULL, &actual_key, &actual_value, NULL));
        assert_isexception(actual_key, yp_TypeError);
        // actual_value is undefined
        yp_decref(not_iterable);
    }

    // "equal key found, *items_ki is less than k"
    {
        ypObject   *mp = yp_frozendictK(K(keys[0], values[0]));
        yp_uint64_t mi_state;
        ypObject   *mi = yp_miniiter_items(mp, &mi_state);
        ypObject   *actual_key;
        ypObject   *actual_value;
        ypObject   *items[] = {keys[0], values[0]};  // borrowed refs
        yp_ssize_t  items_ki;
        assert_true(_assert_mapping_helper(
                mi, &mi_state, 1, items, &actual_key, &actual_value, &items_ki));
        assert_obj(actual_key, eq, keys[0]);
        assert_obj(actual_value, eq, values[0]);
        assert_ssizeC(items_ki, ==, 0);
        yp_decrefN(N(mp, mi, actual_key, actual_value));
    }

    // "equal key not found, *items_ki is equal to k"
    {
        ypObject   *mp = yp_frozendictK(K(keys[1], values[1]));
        yp_uint64_t mi_state;
        ypObject   *mi = yp_miniiter_items(mp, &mi_state);
        ypObject   *actual_key;
        ypObject   *actual_value;
        ypObject   *items[] = {keys[0], values[0]};  // borrowed refs
        yp_ssize_t  items_ki;
        assert_true(_assert_mapping_helper(
                mi, &mi_state, 1, items, &actual_key, &actual_value, &items_ki));
        assert_obj(actual_key, eq, keys[1]);
        assert_obj(actual_value, eq, values[1]);
        assert_ssizeC(items_ki, ==, 1);
        yp_decrefN(N(mp, mi, actual_key, actual_value));
    }

    obj_array_decref(values);
    obj_array_decref(keys);
    uniqueness_dealloc(uq);
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
    assert_type_is(type->yp_type, yp_t_type);
    if (type->is_mutable) {
        assert_null(type->falsy);
    } else if (type->falsy != NULL) {
        assert_type_is(type->falsy, type->yp_type);
    }
    assert_not_null(type->pair);

    // Property flags are tested implicitly by test_fixture_types.

    // new_ is used by collection types.
    if (type->is_collection && type != fixture_type_range) {
        ypObject *x = rand_obj(NULL, type);
        ypObject *self = type->new_(x);
        assert_type_is(self, type->yp_type);
        yp_decrefN(N(self, x));
    }

    // peers should always contain the type and its pair.
    assert_not_null(find_peer_type(type->peers, type));
    assert_not_null(find_peer_type(type->peers, type->pair));

    // peers should be reciprocal (i.e. a tuple is a peer of a str, so a str is a peer of a tuple).
    {
        peer_type_t *peer;
        for (peer = type->peers; peer->type != NULL; peer++) {
            peer_type_t *peer_peer = find_peer_type(peer->type->peers, type);
            assert_not_null(peer_peer);
            assert_ptr(peer_peer->rand_items, ==, peer->rand_items);
            assert_ptr(peer_peer->rand_values, ==, peer->rand_values);
        }
    }

    return MUNIT_OK;
}

// Most of these tests are not actually dependent on type.
static MunitResult test_rand_obj(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    {
        ypObject *any = rand_obj_any(NULL);
        assert_not_exception(any);
        yp_decref(any);
    }

    {
        ypObject *mutable = rand_obj_any_mutable(NULL);
        assert_not_exception(mutable);
        assert_raises_exc(yp_hashC(mutable, &exc), yp_TypeError);
        yp_decref(mutable);
    }

    {
        ypObject *hashable = rand_obj_any_hashable(NULL);
        assert_not_exception(hashable);
        assert_not_raises_exc(yp_hashC(hashable, &exc));
        yp_decref(hashable);
    }

    {
        ypObject *hashable_not_str = rand_obj_any_hashable_not_str(NULL);
        assert_not_exception(hashable_not_str);
        assert_not_raises_exc(yp_hashC(hashable_not_str, &exc));
        assert_obj(yp_type(hashable_not_str), is_not, yp_t_str);
        yp_decref(hashable_not_str);
    }

    {
        hashability_pair_t pair = rand_obj_any_hashability_pair(NULL);
        assert_not_exception(pair.hashable);
        assert_not_exception(pair.unhashable);
        assert_not_raises_exc(yp_hashC(pair.hashable, &exc));
        assert_raises_exc(yp_hashC(pair.unhashable, &exc), yp_TypeError);
        assert_obj(pair.hashable, eq, pair.unhashable);
        yp_decrefN(N(pair.hashable, pair.unhashable));
    }

    {
        ypObject *not_iterable = rand_obj_any_not_iterable(NULL);
        assert_not_exception(not_iterable);
        assert_raises(yp_iter(not_iterable), yp_TypeError);
        yp_decref(not_iterable);
    }

    {
        ypObject *of_type = rand_obj(NULL, type);
        assert_not_exception(of_type);
        assert_type_is(of_type, type->yp_type);
        yp_decref(of_type);
    }

    // Ensure generated functions are callable.
    if (type->yp_type == yp_t_function) {
        ypObject *function = rand_obj(NULL, type);
        ypObject *result;
        assert_not_raises(result = yp_callN(function, 0));
        yp_decrefN(N(result, function));
    }

    // Ensure fixture_type_from_object returns the correct type.
    {
        ypObject       *of_type = rand_obj(NULL, type);
        fixture_type_t *type_from_object = fixture_type_from_object(of_type);
        if (type == fixture_type_frozenset_dirty) {
            assert_ptr(type_from_object, ==, fixture_type_frozenset);
        } else if (type == fixture_type_set_dirty) {
            assert_ptr(type_from_object, ==, fixture_type_set);
        } else if (type == fixture_type_frozendict_dirty) {
            assert_ptr(type_from_object, ==, fixture_type_frozendict);
        } else if (type == fixture_type_dict_dirty) {
            assert_ptr(type_from_object, ==, fixture_type_dict);
        } else {
            assert_ptr(type_from_object, ==, type);
        }
        yp_decref(of_type);
    }

    return MUNIT_OK;
}

// In order to trigger duplicates in rand_obj, we manipulate the random seed. As such, these tests
// are separate from test_rand_obj. Most of these tests are not actually dependent on type.
static MunitResult test_rand_obj_uniqueness(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    munit_uint32_t  seed = munit_rand_uint32();

    // By resetting the seed between executions of statement, we ensure statement will create the
    // same object at first, detect the duplicate, then create a new object.
#define test_uniqueness(statement)           \
    do {                                     \
        uniqueness_t *uq = uniqueness_new(); \
        ypObject     *first;                 \
        ypObject     *second;                \
        munit_rand_seed(seed);               \
        first = (statement);                 \
        munit_rand_seed(seed);               \
        second = (statement);                \
        assert_obj(first, ne, second);       \
        yp_decrefN(2, first, second);        \
        uniqueness_dealloc(uq);              \
    } while (0)

    test_uniqueness(rand_obj_any(uq));
    test_uniqueness(rand_obj_any_mutable(uq));
    test_uniqueness(rand_obj_any_hashable(uq));
    test_uniqueness(rand_obj_any_hashable_not_str(uq));
    test_uniqueness(rand_obj_any_not_iterable(uq));
    if (type != fixture_type_NoneType && type != fixture_type_bool) {
        // rand_obj cannot ensure uniqueness for None and bool.
        test_uniqueness(rand_obj(uq, type));
    }

#undef test_uniqueness

    {
        uniqueness_t      *uq = uniqueness_new();
        hashability_pair_t first;
        hashability_pair_t second;
        munit_rand_seed(seed);
        first = rand_obj_any_hashability_pair(uq);
        munit_rand_seed(seed);
        second = rand_obj_any_hashability_pair(uq);
        assert_obj(first.hashable, ne, second.hashable);
        assert_obj(first.unhashable, ne, second.unhashable);
        assert_obj(first.hashable, eq, first.unhashable);
        assert_obj(second.hashable, eq, second.unhashable);
        yp_decrefN(4, first.hashable, first.unhashable, second.hashable, second.unhashable);
        uniqueness_dealloc(uq);
    }

#define test_uniqueness_array(statement)         \
    do {                                         \
        uniqueness_t *uq = uniqueness_new();     \
        ypObject     *first[1];                  \
        ypObject     *second[1];                 \
        munit_rand_seed(seed);                   \
        obj_array_fill(first, uq, (statement));  \
        munit_rand_seed(seed);                   \
        obj_array_fill(second, uq, (statement)); \
        assert_obj(first[0], ne, second[0]);     \
        yp_decrefN(2, first[0], second[0]);      \
        uniqueness_dealloc(uq);                  \
    } while (0)

    if (type->is_collection) {
        test_uniqueness_array(type->rand_items);
    }
    if (type->is_mapping) {
        test_uniqueness_array(type->rand_values);
    }

#undef test_uniqueness_array

    return MUNIT_OK;
}

static MunitResult test_munit_rand(const MunitParameter params[], fixture_t *fixture)
{
    // There was a divide-by-zero error when min equals max.
    // https://github.com/nemequ/munit/pull/106
    munit_assert_int(munit_rand_int_range(42, 42), ==, 42);

    return MUNIT_OK;
}

static MunitParameterEnum test_all_params[] = {{param_key_type, param_values_types_all}, {NULL}};

MunitTest test_unittest_tests[] = {TEST(test_PRI_formats, NULL),
        TEST(test_assert_setlike_helper, NULL), TEST(test_assert_mapping_helper, NULL),
        TEST(test_fixture_types, NULL), TEST(test_param_values_types, NULL),
        TEST(test_fixture_type, test_all_params), TEST(test_rand_obj, test_all_params),
        TEST(test_rand_obj_uniqueness, test_all_params), TEST(test_munit_rand, NULL), {NULL}};


extern void test_unittest_initialize(void) {}
