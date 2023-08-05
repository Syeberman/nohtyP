
#include "munit_test/unittest.h"

// XXX Because Python calls both the protocol and the object "set", I'm using the term "set-like" to
// refer to the protocol where there may be confusion.

// FIXME For read-only operations, I believe we allow to use non-hashable types, but for operations
// that might modify we should always require hashable types.


// Sets should accept themselves, their pairs, iterators, tuple/list, and frozenset/set as valid
// types for the "x" (i.e. "other iterable") argument.
#define x_types_init(type)                                                              \
    {                                                                                   \
        (type), (type)->pair, fixture_type_iter, fixture_type_tuple, fixture_type_list, \
                fixture_type_frozenset, fixture_type_set, NULL                          \
    }


static MunitResult test_isdisjoint(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    ypObject        *so;
    ypObject        *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    so = type->newN(N(items[0], items[1]));

    // Basic isdisjoint.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // x has the same items.
        ead(x, (*x_type)->newN(N(items[0], items[1])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));

        // x is empty.
        ead(x, (*x_type)->newN(0), assert_obj(yp_isdisjoint(so, x), is, yp_True));

        // x is is a subset.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_isdisjoint(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1])), assert_obj(yp_isdisjoint(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));

        // x is a superset.
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[3])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1], items[2])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[2])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));

        // x overlaps.
        ead(x, (*x_type)->newN(N(items[0], items[2])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[3])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2], items[3])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[2])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[2])),
                assert_obj(yp_isdisjoint(so, x), is, yp_False));

        // x does not overlap.
        ead(x, (*x_type)->newN(N(items[2])), assert_obj(yp_isdisjoint(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[2], items[3])),
                assert_obj(yp_isdisjoint(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[2], items[2])),
                assert_obj(yp_isdisjoint(so, x), is, yp_True));

        // x is so.
        assert_obj(yp_isdisjoint(so, so), is, yp_False);
    }

    // Empty so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Non-empty x.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_isdisjoint(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1])),
                assert_obj(yp_isdisjoint(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0])),
                assert_obj(yp_isdisjoint(empty, x), is, yp_True));

        // Empty x.
        ead(x, (*x_type)->newN(0), assert_obj(yp_isdisjoint(empty, x), is, yp_True));

        // x is so.
        assert_obj(yp_isdisjoint(empty, empty), is, yp_True);
    }

    // FIXME What if x contains unhashable objects?

    // x is an iterator that fails at the start.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises(yp_isdisjoint(so, x), yp_SyntaxError);
        yp_decrefN(N(x_supplier, x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises(yp_isdisjoint(so, x), yp_SyntaxError);
        yp_decrefN(N(x_supplier, x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        assert_obj(yp_isdisjoint(so, x), is, yp_True);
        yp_decrefN(N(x_supplier, x));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        assert_obj(yp_isdisjoint(so, x), is, yp_True);
        yp_decrefN(N(x_supplier, x));
    }

    // x is not an iterable.
    assert_raises(yp_isdisjoint(so, int_1), yp_TypeError);

    obj_array_decref(items);
    yp_decrefN(N(int_1, so, empty));
    return MUNIT_OK;
}

static MunitResult test_issubset(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    fixture_type_t  *x_types[] = x_types_init(type);
    fixture_type_t **x_type;
    ypObject        *int_1 = yp_intC(1);
    ypObject        *items[4];
    ypObject        *so;
    ypObject        *empty = type->newN(0);
    obj_array_fill(items, type->rand_items);
    so = type->newN(N(items[0], items[1]));

    // Basic issubset.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // x has the same items.
        ead(x, (*x_type)->newN(N(items[0], items[1])), assert_obj(yp_issubset(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1])),
                assert_obj(yp_issubset(so, x), is, yp_True));

        // x is empty.
        ead(x, (*x_type)->newN(0), assert_obj(yp_issubset(so, x), is, yp_False));

        // x is is a subset.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1])), assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0])),
                assert_obj(yp_issubset(so, x), is, yp_False));

        // x is a superset.
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[3])),
                assert_obj(yp_issubset(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[1], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1], items[2], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_True));

        // x overlaps.
        ead(x, (*x_type)->newN(N(items[0], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[3])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[1], items[2], items[3])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[0], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[0], items[2], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_False));

        // x does not overlap.
        ead(x, (*x_type)->newN(N(items[2])), assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[2], items[3])),
                assert_obj(yp_issubset(so, x), is, yp_False));
        ead(x, (*x_type)->newN(N(items[2], items[2])),
                assert_obj(yp_issubset(so, x), is, yp_False));

        // x is so.
        assert_obj(yp_issubset(so, so), is, yp_True);
    }

    // Empty so.
    for (x_type = x_types; (*x_type) != NULL; x_type++) {
        // Non-empty x.
        ead(x, (*x_type)->newN(N(items[0])), assert_obj(yp_issubset(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[1])),
                assert_obj(yp_issubset(empty, x), is, yp_True));
        ead(x, (*x_type)->newN(N(items[0], items[0])),
                assert_obj(yp_issubset(empty, x), is, yp_True));

        // Empty x.
        ead(x, (*x_type)->newN(0), assert_obj(yp_issubset(empty, x), is, yp_True));

        // x is so.
        assert_obj(yp_issubset(empty, empty), is, yp_True);
    }

    // FIXME What if x contains unhashable objects?

    // x is an iterator that fails at the start.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 0, yp_SyntaxError, 2);
        assert_raises(yp_issubset(so, x), yp_SyntaxError);
        yp_decrefN(N(x_supplier, x));
    }

    // x is an iterator that fails mid-way.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 1, yp_SyntaxError, 2);
        assert_raises(yp_issubset(so, x), yp_SyntaxError);
        yp_decrefN(N(x_supplier, x));
    }

    // x is an iterator with a too-small length_hint.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 1);
        assert_obj(yp_issubset(so, x), is, yp_False);
        yp_decrefN(N(x_supplier, x));
    }

    // x is an iterator with a too-large length_hint.
    {
        ypObject *x_supplier = type->newN(N(items[2], items[3]));
        ypObject *x = new_faulty_iter(x_supplier, 3, yp_SyntaxError, 99);
        assert_obj(yp_issubset(so, x), is, yp_False);
        yp_decrefN(N(x_supplier, x));
    }

    // x is not an iterable.
    assert_raises(yp_issubset(so, int_1), yp_TypeError);

    obj_array_decref(items);
    yp_decrefN(N(int_1, so, empty));
    return MUNIT_OK;
}

// FIXME Move test_remove from test_frozenset here.


static MunitParameterEnum test_setlike_params[] = {
        {param_key_type, param_values_types_set}, {NULL}};

MunitTest test_setlike_tests[] = {TEST(test_isdisjoint, test_setlike_params),
        TEST(test_issubset, test_setlike_params), {NULL}};

// FIXME The protocol and the object share the same name. Distinction between test_setlike and
// test_frozenset is flimsy. Rename?

extern void test_setlike_initialize(void) {}
