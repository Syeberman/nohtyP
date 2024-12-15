
#include "munit_test/unittest.h"


// TODO Ensure yp_sort4 properly handles exception passthrough, even in cases where one of the
// arguments would be ignored (e.g. empty list).


// FIXME Disable these OOM tests and ensure we have natural coverage for tuple/list.
static MunitResult test_oom(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    uniqueness_t   *uq = uniqueness_new();
    ypObject       *items[8];
    obj_array_fill(items, uq, type->rand_items);

    // _ypTuple_copy
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_unfrozen_copy(sq), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // TODO _ypTuple_deepcopy, new detached array

    // _ypTuple_push
    if (type->is_mutable) {
        int       i;
        ypObject *exc = yp_None;
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        for (i = 0; i < 32; i++) {
            yp_push(sq, items[i % yp_lengthof_array(items)], &exc);
            if (yp_isexceptionC(exc)) break;
        }
        assert_isexception(exc, yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // _ypTuple_extend_fromtuple
    if (type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = yp_tupleN(N(items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_extend(sq, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq, x));
    }

    // _ypTuple_extend_fromminiiter
    if (type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = fixture_type_iter->newN(N(items[0], items[1], items[2], items[3], items[4],
                items[5], items[6], items[7], items[0], items[1], items[2], items[3], items[4],
                items[5], items[6], items[7], items[0], items[1], items[2], items[3], items[4],
                items[5], items[6], items[7], items[0], items[1], items[2], items[3], items[4],
                items[5], items[6], items[7]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_extend(sq, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq, x));
    }

    // _ypTuple_new_fromminiiter
    {
        ypObject *x = fixture_type_iter->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        if (type->type == yp_t_tuple) {
            assert_raises(yp_tuple(x), yp_MemoryError);
        } else {
            assert_raises(yp_list(x), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    // _ypTuple_new_fromiterable, empty list
    if (type->type == yp_t_list) {
        ypObject *x = yp_frozensetN(0);
        malloc_tracker_oom_after(0);
        assert_raises(yp_list(x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    // _ypTuple_concat_fromtuple
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = yp_tupleN(N(items[2], items[3]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_concat(sq, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq, x));
    }

    // _ypTuple_setslice_grow
    if (type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = yp_tupleN(N(items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7], items[0], items[1], items[2], items[3], items[4], items[5],
                items[6], items[7]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_setsliceC6(sq, 1, 2, 1, x, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq, x));
    }

    // tuple_concat, copy list
    if (type->type == yp_t_list) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = yp_frozensetN(0);
        malloc_tracker_oom_after(0);
        assert_raises(yp_concat(sq, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(x, sq));
    }

    // tuple_concat, new
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        ypObject *x = fixture_type_iter->newN(N(items[2], items[3]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_concat(sq, x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(x, sq));
    }

    // tuple_repeat
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_repeatC(sq, 2), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // tuple_getslice
    {
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_getsliceC4(sq, 0, 1, 1), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // list_irepeat
    if (type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_irepeatC(sq, 32, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // list_insert
    if (type->is_mutable) {
        int       i;
        ypObject *exc = yp_None;
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        for (i = 0; i < 32; i++) {
            yp_insertC(sq, -1, items[i % yp_lengthof_array(items)], &exc);
            if (yp_isexceptionC(exc)) break;
        }
        assert_isexception(exc, yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    // _ypTupleNV
    {
        malloc_tracker_oom_after(0);
        if (type->type == yp_t_tuple) {
            assert_raises(yp_tupleN(N(items[0], items[1])), yp_MemoryError);
        } else {
            assert_raises(yp_listN(N(items[0], items[1])), yp_MemoryError);
        }
        malloc_tracker_oom_disable();
    }

    // yp_sorted3, new list
    {
        ypObject *x = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises(yp_sorted(x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    // yp_sorted3, new detached array
    {
        ypObject *x = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(1);  // allow yp_list to succeed
        assert_raises(yp_sorted(x), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(x));
    }

    // FIXME _ypTuple_repeatCNV

    // list_sort, new detached array
    if (type->is_mutable) {
        ypObject *sq = type->newN(N(items[0], items[1]));
        malloc_tracker_oom_after(0);
        assert_raises_exc(yp_sort(sq, &exc), yp_MemoryError);
        malloc_tracker_oom_disable();
        yp_decrefN(N(sq));
    }

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    return MUNIT_OK;
}


char *param_values_test_tuple[] = {"tuple", "list", NULL};

static MunitParameterEnum test_tuple_params[] = {{param_key_type, param_values_test_tuple}, {NULL}};

MunitTest test_tuple_tests[] = {TEST(test_oom, test_tuple_params), {NULL}};


extern void test_tuple_initialize(void) {}
