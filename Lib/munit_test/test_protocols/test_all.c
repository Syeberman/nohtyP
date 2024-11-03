#include "munit_test/unittest.h"


// TODO Ensure yp_or/yp_orN/yp_anyN and yp_and/yp_andN/yp_allN properly handles exception
// passthrough, even in cases where one of the arguments would be ignored.

// TODO Test deepcopy/etc generally here. Specific tests can then be added lower (i.e.
// test_collections can test with various items, knowing that items are stored in the object).

// Ensures that yp_TypeError or similar is raised when a type doesn't support a protocol. This
// doesn't deal with methods only supported on mutable or immutable types: mutability is not a
// "protocol".
// TODO Is the distinction between yp_TypeError and yp_MethodError really that important? Should
// everything below just be yp_TypeError? I'm choosing the equivalent to what Python raises but the
// distinction is sometimes arbitrary (i.e. in Python next is a function but close is a method).
// TODO This test could also check exceptions and invalidateds return correct exceptions.
// TODO Wherever we ensure exceptions and invalidateds return the correct exceptions, we should also
// ensure that they do so where the arguments would otherwise be "ignored" (i.e. an empty slice
// might ignore the object, but we need to ensure exception passthrough works).
static MunitResult test_unsupported_protocols(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *self = rand_obj(NULL, type);

    if (type != fixture_type_bool && !type->is_numeric && !type->is_sequence && !type->is_setlike) {
        assert_raises(yp_lt(self, self), yp_TypeError);
        assert_raises(yp_le(self, self), yp_TypeError);
        assert_raises(yp_ge(self, self), yp_TypeError);
        assert_raises(yp_gt(self, self), yp_TypeError);
    }

    if (!type->is_iterable) {
        assert_raises(yp_iter(self), yp_TypeError);
        // yp_iter2 is tested elsewhere.
        assert_raises_exc(yp_unpackN(self, 1, &exc), yp_TypeError);
        // TODO yp_unpackNV?
        assert_raises(yp_filter(yp_None, self), yp_NotImplementedError);
        assert_raises(yp_filterfalse(yp_None, self), yp_NotImplementedError);
        assert_raises(yp_max_key(self, yp_None), yp_NotImplementedError);
        assert_raises(yp_min_key(self, yp_None), yp_NotImplementedError);
        assert_raises(yp_max(self), yp_NotImplementedError);
        assert_raises(yp_min(self), yp_NotImplementedError);
        assert_raises(yp_reversed(self), yp_TypeError);
        assert_raises(yp_sorted3(self, yp_None, yp_False), yp_TypeError);
        assert_raises(yp_sorted(self), yp_TypeError);
        assert_raises(yp_zipN(1, self), yp_NotImplementedError);
        // TODO yp_zipNV?
        assert_raises(yp_sum(self), yp_NotImplementedError);
    }

    if (type != fixture_type_iter) {
        // The Python equivalents of yp_send and yp_throw are methods on generator/etc objects,
        // so raise AttributeError if unsupported. nohtyP treats these as part of the iterator
        // protocol, so yp_TypeError is more appropriate.
        assert_raises(yp_send(self, yp_None), yp_TypeError);
        assert_raises(yp_next(self), yp_TypeError);
        assert_raises(yp_next2(self, yp_None), yp_TypeError);
        assert_raises(yp_throw(self, yp_Exception), yp_TypeError);
        // In Python, operator.length_hint returns zero for None, int, float, function, etc.
        assert_ssizeC_raises_exc(yp_length_hintC(self, &exc), ==, 0, yp_TypeError);
        assert_raises_exc(yp_close(self, &exc), yp_MethodError);
    }

    if (!type->is_collection) {
        assert_raises(yp_contains(self, yp_None), yp_TypeError);
        assert_raises(yp_in(yp_None, self), yp_TypeError);
        assert_raises(yp_not_in(yp_None, self), yp_TypeError);
        assert_ssizeC_raises_exc(yp_lenC(self, &exc), ==, 0, yp_TypeError);
        assert_raises_exc(yp_clear(self, &exc), yp_MethodError);
    }

    if (!type->is_sequence && !type->is_mapping) {
        assert_raises(yp_getitem(self, yp_i_zero), yp_TypeError);
        assert_raises(yp_getdefault(self, yp_i_zero, yp_None), yp_TypeError);
        assert_raises_exc(yp_setitem(self, yp_i_zero, yp_None, &exc), yp_TypeError);
        assert_raises_exc(yp_delitem(self, yp_i_zero, &exc), yp_TypeError);
        assert_raises_exc(yp_dropitem(self, yp_i_zero, &exc), yp_TypeError);
    }

    if (!type->is_sequence && !type->is_setlike) {
        assert_raises_exc(yp_push(self, yp_None, &exc), yp_MethodError);
        assert_raises(yp_pop(self), yp_MethodError);
        assert_raises_exc(yp_remove(self, yp_None, &exc), yp_MethodError);
        assert_raises_exc(yp_discard(self, yp_None, &exc), yp_MethodError);
    }

    if (!type->is_setlike && !type->is_mapping) {
        assert_raises_exc(yp_update(self, self, &exc), yp_MethodError);
    }

    if (!type->is_sequence) {
        assert_raises(yp_concat(self, self), yp_TypeError);
        assert_raises(yp_repeatC(self, 0), yp_TypeError);
        assert_raises(yp_getindexC(self, 0), yp_TypeError);
        assert_raises(yp_getsliceC4(self, 0, 0, 1), yp_TypeError);
        // yp_getitem and yp_getdefault are tested elsewhere.
        assert_ssizeC_raises_exc(yp_findC5(self, yp_None, 0, 0, &exc), ==, -1, yp_MethodError);
        assert_ssizeC_raises_exc(yp_findC(self, yp_None, &exc), ==, -1, yp_MethodError);
        assert_ssizeC_raises_exc(yp_indexC5(self, yp_None, 0, 0, &exc), ==, -1, yp_MethodError);
        assert_ssizeC_raises_exc(yp_indexC(self, yp_None, &exc), ==, -1, yp_MethodError);
        assert_ssizeC_raises_exc(yp_rfindC5(self, yp_None, 0, 0, &exc), ==, -1, yp_MethodError);
        assert_ssizeC_raises_exc(yp_rfindC(self, yp_None, &exc), ==, -1, yp_MethodError);
        assert_ssizeC_raises_exc(yp_rindexC5(self, yp_None, 0, 0, &exc), ==, -1, yp_MethodError);
        assert_ssizeC_raises_exc(yp_rindexC(self, yp_None, &exc), ==, -1, yp_MethodError);
        assert_ssizeC_raises_exc(yp_countC5(self, yp_None, 0, 0, &exc), ==, 0, yp_MethodError);
        assert_ssizeC_raises_exc(yp_countC(self, yp_None, &exc), ==, 0, yp_MethodError);
        assert_raises_exc(yp_setindexC(self, 0, yp_None, &exc), yp_TypeError);
        assert_raises_exc(yp_setsliceC6(self, 0, 0, 1, yp_None, &exc), yp_TypeError);
        // yp_setitem is tested elsewhere.
        assert_raises_exc(yp_delindexC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_dropindexC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_delsliceC5(self, 0, 0, 1, &exc), yp_TypeError);
        // yp_delitem and yp_dropitem are tested elsewhere.
        assert_raises_exc(yp_append(self, yp_None, &exc), yp_MethodError);
        // yp_push is tested elsewhere.
        assert_raises_exc(yp_extend(self, self, &exc), yp_MethodError);
        assert_raises_exc(yp_irepeatC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_insertC(self, 0, yp_None, &exc), yp_MethodError);
        assert_raises(yp_popindexC(self, 0), yp_MethodError);
        // yp_pop, yp_remove, and yp_discard are tested elsewhere.
        assert_raises_exc(yp_reverse(self, &exc), yp_MethodError);
    }

    if (type != fixture_type_list) {
        assert_raises_exc(yp_sort4(self, yp_None, yp_False, &exc), yp_MethodError);
        assert_raises_exc(yp_sort(self, &exc), yp_MethodError);
    }

    if (!type->is_setlike) {
        assert_raises(yp_isdisjoint(self, self), yp_MethodError);
        assert_raises(yp_issubset(self, self), yp_MethodError);
        // yp_lt is tested elsewhere.
        assert_raises(yp_issuperset(self, self), yp_MethodError);
        // yp_gt is tested elsewhere.
        assert_raises(yp_union(self, self), yp_MethodError);
        assert_raises(yp_intersection(self, self), yp_MethodError);
        assert_raises(yp_difference(self, self), yp_MethodError);
        assert_raises(yp_symmetric_difference(self, self), yp_MethodError);
        // yp_update is tested elsewhere.
        assert_raises_exc(yp_intersection_update(self, self, &exc), yp_MethodError);
        assert_raises_exc(yp_difference_update(self, self, &exc), yp_MethodError);
        assert_raises_exc(yp_symmetric_difference_update(self, self, &exc), yp_MethodError);
        // yp_push is tested elsewhere.
        assert_raises_exc(yp_pushunique(self, yp_None, &exc), yp_MethodError);
        // yp_remove, yp_discard, and yp_pop are tested elsewhere.
    }

    if (!type->is_mapping) {
        assert_raises(yp_iter_items(self), yp_MethodError);
        assert_raises(yp_iter_keys(self), yp_MethodError);
        assert_raises(yp_iter_values(self), yp_MethodError);
        assert_raises(yp_popvalue3(self, yp_None, yp_None), yp_MethodError);
        assert_raises(yp_popvalue2(self, yp_None), yp_MethodError);
        {
            ypObject *value = NULL;
            assert_raises_exc(yp_popitem(self, &exc, &value), yp_MethodError);
            assert_isexception(value, yp_MethodError);
        }
        assert_raises(yp_setdefault(self, yp_None, yp_None), yp_MethodError);
        assert_raises_exc(yp_updateK(self, &exc, 0), yp_MethodError);
        // TODO yp_updateKV?
    }

    if (!type->is_string) {
        assert_raises(yp_isalnum(self), yp_MethodError);
        assert_raises(yp_isalpha(self), yp_MethodError);
        assert_raises(yp_isdecimal(self), yp_MethodError);
        assert_raises(yp_isdigit(self), yp_MethodError);
        assert_raises(yp_isidentifier(self), yp_MethodError);
        assert_raises(yp_islower(self), yp_MethodError);
        assert_raises(yp_isnumeric(self), yp_MethodError);
        assert_raises(yp_isprintable(self), yp_MethodError);
        assert_raises(yp_isspace(self), yp_MethodError);
        assert_raises(yp_isupper(self), yp_MethodError);
        assert_raises(yp_startswithC4(self, yp_str_empty, 0, 0), yp_MethodError);
        assert_raises(yp_startswith(self, yp_str_empty), yp_MethodError);
        assert_raises(yp_endswithC4(self, yp_str_empty, 0, 0), yp_MethodError);
        assert_raises(yp_endswith(self, yp_str_empty), yp_MethodError);
        assert_raises(yp_lower(self), yp_MethodError);
        assert_raises(yp_upper(self), yp_MethodError);
        assert_raises(yp_casefold(self), yp_MethodError);
        assert_raises(yp_swapcase(self), yp_MethodError);
        assert_raises(yp_capitalize(self), yp_MethodError);
        assert_raises(yp_ljustC3(self, 0, ' '), yp_MethodError);
        assert_raises(yp_ljustC(self, 0), yp_MethodError);
        assert_raises(yp_rjustC3(self, 0, ' '), yp_MethodError);
        assert_raises(yp_rjustC(self, 0), yp_MethodError);
        assert_raises(yp_centerC3(self, 0, ' '), yp_MethodError);
        assert_raises(yp_centerC(self, 0), yp_MethodError);
        assert_raises(yp_expandtabsC(self, 4), yp_MethodError);
        assert_raises(yp_replaceC4(self, self, self, 0), yp_MethodError);
        assert_raises(yp_replace(self, self, self), yp_MethodError);
        assert_raises(yp_lstrip2(self, yp_None), yp_MethodError);
        assert_raises(yp_lstrip(self), yp_MethodError);
        assert_raises(yp_rstrip2(self, yp_None), yp_MethodError);
        assert_raises(yp_rstrip(self), yp_MethodError);
        assert_raises(yp_strip2(self, yp_None), yp_MethodError);
        assert_raises(yp_strip(self), yp_MethodError);
        assert_raises(yp_joinN(self, 0), yp_MethodError);
        assert_raises(yp_joinNV(self, 0, NULL), yp_MethodError);
        {
            ypObject *part1 = NULL;
            ypObject *part2 = NULL;
            assert_raises_exc(yp_partition(self, self, &exc, &part1, &part2), yp_MethodError);
            assert_isexception(part1, yp_MethodError);
            assert_isexception(part2, yp_MethodError);
        }
        {
            ypObject *part1 = NULL;
            ypObject *part2 = NULL;
            assert_raises_exc(yp_rpartition(self, self, &exc, &part1, &part2), yp_MethodError);
            assert_isexception(part1, yp_MethodError);
            assert_isexception(part2, yp_MethodError);
        }
        assert_raises(yp_splitC3(self, yp_None, 0), yp_MethodError);
        assert_raises(yp_split2(self, yp_None), yp_MethodError);
        assert_raises(yp_split(self), yp_MethodError);
        assert_raises(yp_rsplitC3(self, yp_None, 0), yp_MethodError);
        assert_raises(yp_splitlines2(self, yp_False), yp_MethodError);
        assert_raises(yp_encode3(self, yp_s_utf_8, yp_s_strict), yp_MethodError);
        assert_raises(yp_encode(self), yp_MethodError);
        assert_raises(yp_decode3(self, yp_s_utf_8, yp_s_strict), yp_MethodError);
        assert_raises(yp_decode(self), yp_MethodError);
        assert_raises(yp_formatN(self, 0), yp_NotImplementedError);
        assert_raises(yp_formatNV(self, 0, NULL), yp_NotImplementedError);
        assert_raises(yp_formatK(self, 0), yp_NotImplementedError);
        assert_raises(yp_formatKV(self, 0, NULL), yp_NotImplementedError);
        assert_raises(yp_format(self, yp_tuple_empty, yp_frozendict_empty), yp_NotImplementedError);
    }

    if (!type->is_callable) {
        assert_raises(yp_iter2(self, yp_None), yp_TypeError);
        assert_false(yp_iscallableC(self));
        assert_raises(yp_callN(self, 0), yp_TypeError);
        assert_raises(yp_callNV(self, 0, NULL), yp_TypeError);
        assert_raises(yp_call_stars(self, yp_tuple_empty, yp_frozendict_empty), yp_TypeError);
    }

    if (!type->is_numeric && type != fixture_type_bool) {
        assert_intC_raises_exc(yp_asintC(self, &exc), ==, 0, yp_TypeError);
        assert_intC_raises_exc(yp_asint8C(self, &exc), ==, 0, yp_TypeError);
        assert_intC_raises_exc(yp_asuint8C(self, &exc), ==, 0, yp_TypeError);
        assert_intC_raises_exc(yp_asint16C(self, &exc), ==, 0, yp_TypeError);
        assert_intC_raises_exc(yp_asuint16C(self, &exc), ==, 0, yp_TypeError);
        assert_intC_raises_exc(yp_asint32C(self, &exc), ==, 0, yp_TypeError);
        assert_intC_raises_exc(yp_asuint32C(self, &exc), ==, 0, yp_TypeError);
        assert_intC_raises_exc(yp_asint64C(self, &exc), ==, 0, yp_TypeError);
        assert_intC_raises_exc((yp_int_t)yp_asuint64C(self, &exc), ==, 0, yp_TypeError);
        assert_floatC_raises_exc(yp_asfloatC(self, &exc), ==, 0.0, yp_TypeError);
        assert_floatC_raises_exc(yp_asfloat32C(self, &exc), ==, 0.0, yp_NotImplementedError);
        assert_floatC_raises_exc(yp_asfloat64C(self, &exc), ==, 0.0, yp_NotImplementedError);
        assert_intC_raises_exc(yp_asssizeC(self, &exc), ==, 0, yp_TypeError);
        assert_intC_raises_exc(yp_ashashC(self, &exc), ==, 0, yp_TypeError);
    }

    if (!type->is_numeric) {
        assert_raises(yp_add(self, self), yp_TypeError);
        assert_raises(yp_sub(self, self), yp_TypeError);
        assert_raises(yp_mul(self, self), yp_TypeError);
        assert_raises(yp_truediv(self, self), yp_TypeError);
        assert_raises(yp_floordiv(self, self), yp_TypeError);
        assert_raises(yp_mod(self, self), yp_TypeError);
        {
            ypObject *mod = NULL;
            assert_raises_exc(yp_divmod(self, self, &exc, &mod), yp_TypeError);
            assert_isexception(mod, yp_TypeError);
        }
        assert_raises(yp_pow(self, self), yp_TypeError);
        assert_raises(yp_pow3(self, self, self), yp_NotImplementedError);
        assert_raises(yp_neg(self), yp_TypeError);
        assert_raises(yp_pos(self), yp_TypeError);
        assert_raises(yp_abs(self), yp_TypeError);
        assert_raises(yp_lshift(self, self), yp_TypeError);
        assert_raises(yp_rshift(self, self), yp_TypeError);
        assert_raises(yp_amp(self, self), yp_TypeError);
        assert_raises(yp_xor(self, self), yp_TypeError);
        assert_raises(yp_bar(self, self), yp_TypeError);
        assert_raises(yp_invert(self), yp_TypeError);

        assert_raises_exc(yp_iadd(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_isub(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_imul(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_itruediv(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_ifloordiv(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_imod(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_ipow(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_ipow4(self, self, self, &exc), yp_NotImplementedError);
        assert_raises_exc(yp_ineg(self, &exc), yp_TypeError);
        assert_raises_exc(yp_ipos(self, &exc), yp_TypeError);
        assert_raises_exc(yp_iabs(self, &exc), yp_TypeError);
        assert_raises_exc(yp_ilshift(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_irshift(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_iamp(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_ixor(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_ibar(self, self, &exc), yp_TypeError);
        assert_raises_exc(yp_iinvert(self, &exc), yp_TypeError);

        assert_raises_exc(yp_iaddC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_isubC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_imulC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_itruedivC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_ifloordivC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_imodC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_ipowC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_ipowC4(self, 0, 0, &exc), yp_NotImplementedError);
        assert_raises_exc(yp_ilshiftC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_irshiftC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_iampC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_ixorC(self, 0, &exc), yp_TypeError);
        assert_raises_exc(yp_ibarC(self, 0, &exc), yp_TypeError);

        assert_raises_exc(yp_iaddCF(self, 0.0, &exc), yp_TypeError);
        assert_raises_exc(yp_isubCF(self, 0.0, &exc), yp_TypeError);
        assert_raises_exc(yp_imulCF(self, 0.0, &exc), yp_TypeError);
        assert_raises_exc(yp_itruedivCF(self, 0.0, &exc), yp_TypeError);
        assert_raises_exc(yp_ifloordivCF(self, 0.0, &exc), yp_TypeError);
        assert_raises_exc(yp_imodCF(self, 0.0, &exc), yp_TypeError);
        assert_raises_exc(yp_ipowCF(self, 0.0, &exc), yp_TypeError);

        // yp_asintC et al. are tested elsewhere.

        assert_raises(yp_roundC(self, 0), yp_NotImplementedError);
        assert_raises(yp_sumN(2, self, self), yp_NotImplementedError);
        // TODO yp_sumNV?
        ead(iterable, yp_listN(N(self, self)),
                assert_raises(yp_sum(iterable), yp_NotImplementedError));
    }

    if (type != fixture_type_int && type != fixture_type_intstore) {
        assert_intC_raises_exc(yp_bit_lengthC(self, &exc), ==, 0, yp_MethodError);
    }

    if (!type->is_iterable) {
        yp_uint64_t sentinel = munit_rand_uint32();
        yp_uint64_t state = sentinel;
        assert_raises(yp_miniiter(self, &state), yp_TypeError);
        munit_assert_uint64(state, ==, sentinel);
        assert_raises(yp_miniiter_next(self, &state), yp_TypeError);
        munit_assert_uint64(state, ==, sentinel);
        assert_ssizeC_raises_exc(yp_miniiter_length_hintC(self, &state, &exc), ==, 0, yp_TypeError);
        munit_assert_uint64(state, ==, sentinel);
    }

    if (!type->is_mapping) {
        yp_uint64_t sentinel = munit_rand_uint32();
        yp_uint64_t state = sentinel;
        assert_raises(yp_miniiter_keys(self, &state), yp_MethodError);
        munit_assert_uint64(state, ==, sentinel);
        assert_raises(yp_miniiter_values(self, &state), yp_MethodError);
        munit_assert_uint64(state, ==, sentinel);
        assert_raises(yp_miniiter_items(self, &state), yp_MethodError);
        munit_assert_uint64(state, ==, sentinel);
        {
            ypObject *value = NULL;
            assert_raises_exc(yp_miniiter_items_next(self, &state, &exc, &value), yp_TypeError);
            munit_assert_uint64(state, ==, sentinel);
            assert_isexception(value, yp_TypeError);
        }
    }

    if (type != fixture_type_iter) {
        yp_ssize_t size = -1;
        void      *state = (void *)-1;
        // TODO More tests for yp_iter_stateCX in test_objects/test_iter.
        assert_raises(yp_iter_stateCX(self, &size, &state), yp_TypeError);
        assert_ssizeC(size, ==, 0);
        assert_ptr(state, ==, NULL);
    }
    if (type != fixture_type_function) {
        yp_ssize_t size = -1;
        void      *state = (void *)-1;
        assert_raises(yp_function_stateCX(self, &size, &state), yp_NotImplementedError);
        assert_ssizeC(size, ==, 0);
        assert_ptr(state, ==, NULL);
    }
    if (type != fixture_type_bytes && type != fixture_type_bytearray) {
        yp_ssize_t        len = -1;
        const yp_uint8_t *bytes = (yp_uint8_t *)-1;
        // TODO More tests for yp_asbytesCX in test_objects/test_bytes.
        assert_raises(yp_asbytesCX(self, &len, &bytes), yp_TypeError);
        assert_ssizeC(len, ==, 0);
        assert_ptr(bytes, ==, NULL);
    }
    if (type != fixture_type_str && type != fixture_type_chrarray) {
        yp_ssize_t        size = -1;
        const yp_uint8_t *encoded = (yp_uint8_t *)-1;
        ypObject         *encoding = (ypObject *)-1;
        // TODO More tests for yp_asencodedCX in test_objects/test_str.
        assert_raises(yp_asencodedCX(self, &size, &encoded, &encoding), yp_TypeError);
        assert_ssizeC(size, ==, 0);
        assert_ptr(encoded, ==, NULL);
        assert_isexception(encoding, yp_TypeError);
    }
    if (type != fixture_type_tuple && type != fixture_type_list) {
        yp_ssize_t       len = -1;
        ypObject *const *array = (ypObject **)-1;
        // TODO More tests for yp_itemarrayCX in test_objects/test_tuple.
        assert_raises(yp_itemarrayCX(self, &len, &array), yp_TypeError);
        assert_ssizeC(len, ==, 0);
        assert_ptr(array, ==, NULL);
    }
    if (!type->is_callable) {
        ypObject *args[] = {self};
        assert_raises(yp_call_arrayX(1, args), yp_TypeError);
        assert_obj(args[0], is, self);
    }

    yp_decrefN(N(self));
    return MUNIT_OK;
}


static MunitParameterEnum test_all_params[] = {{param_key_type, param_values_types_all}, {NULL}};

MunitTest test_all_tests[] = {TEST(test_unsupported_protocols, test_all_params), {NULL}};

extern void test_all_initialize(void) {}
