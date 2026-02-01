// Tests for objects that support the string protocol.
//
// These tests are written such that we can test operations between strings of different encodings.
// As str and chrarray store their characters in the smallest encoding required by their characters,
// the choice of characters impacts the encoding of the string. The variants fixture_type_str_1byte
// et al. enforce a minimum encoding: they require at least one character from the type's
// rand_elems, but for ease-of-use they allow other characters with smaller or larger encodings.
// Because of this flexibility, it's possible to accidentally write tests below that do *not*
// actually test with different encodings. To avoid this, it's important to ensure that at least one
// of the strings is initialized *only* with the characters from its own rand_elems.

#include "munit_test/unittest.h"

// FIXME replace copy/paste sq with s.

// FIXME _ypStringLib_checkenc_contiguous_ucs_2 is barely covered because our test strings are too
// small, we need to test a variety of lengths. (But why is _ypStringLib_checkenc_ucs_4 so well
// covered?)

// FIXME Why is ypStringLib_checkenc_delslice on latin-1 not covered?? test_sequence should be
// sufficient here.

// FIXME Ensure yp_replaceC4/yp_lstrip2/yp_splitlines2/yp_encode3/etc properly handles exception
// passthrough, even in cases where one of the arguments would be ignored (e.g. empty str, empty
// slice).
// TODO This (exception passthrough) even includes yp_formatN/etc where the argument is never
// referenced in the format string.

typedef struct _slice_args_t {
    yp_ssize_t start;
    yp_ssize_t stop;
    yp_ssize_t step;
} slice_args_t;

// FIXME Strings are either "binary" or "text".
static int isbinary(fixture_type_t *type)
{
    return type == fixture_type_bytes || type == fixture_type_bytearray;
}


// expected is either the exception which is expected to be raised, or the boolean expected to be
// returned.
static void _test_comparisons_not_supported(fixture_type_t *type, fixture_type_t *x_type,
        ypObject *(*any_cmp)(ypObject *, ypObject *), ypObject                   *expected)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[2];
    ypObject     *s;
    ypObject     *empty = type->newN(0);
    obj_array_fill(items, uq, type->rand_elems->items);
    s = type->newN(N(items[0], items[1]));

#define assert_not_supported(expression)      \
    do {                                      \
        ypObject *result = (expression);      \
        if (yp_isexceptionC(expected)) {      \
            assert_raises(result, expected);  \
        } else {                              \
            assert_obj(result, is, expected); \
        }                                     \
    } while (0)

    ead(x, rand_obj(NULL, x_type), assert_not_supported(any_cmp(s, x)));
    ead(x, rand_obj(NULL, x_type), assert_not_supported(any_cmp(empty, x)));
    ead(x, x_type->newN(0), assert_not_supported(any_cmp(s, x)));
    ead(x, x_type->newN(0), assert_not_supported(any_cmp(empty, x)));

#undef assert_not_supported

    obj_array_decref(items);
    yp_decrefN(N(s, empty));
    uniqueness_dealloc(uq);
}

// String-specific tests not covered by test_sequence. In particular, this tests peers of differing
// encodings, for example comparing str_1byte to str_4bytes.
static void _test_comparisons(fixture_type_t *type, fixture_type_t *x_type,
        ypObject *(*any_cmp)(ypObject *, ypObject *), ypObject *x_lt, ypObject *x_gt)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[6];  // items are in ascending order
    ypObject     *xc;        // This item must be present when creating x.
    ypObject     *i1_to_xc;  // One of x_lt or x_gt, when items[1] is compared to xc. Borrowed.
    ypObject     *i4_to_xc;
    // s contains only items; x contains either items or x_items. (See note at top of file.)
    obj_array_fill(items, uq, type->rand_elems->items_ordered);
    x_type->rand_elems->items(uq, 1, &xc);
    i1_to_xc = yp_ltC_not_raises(items[1], xc) ? x_lt : x_gt;
    i4_to_xc = yp_ltC_not_raises(items[4], xc) ? x_lt : x_gt;

    // Two-item s, ascending order.
    {
        ypObject *s = type->newN(N(items[1], items[4]));

        // x has the same items, plus xc.
        ead(x, x_type->newN(N(items[1], items[4], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // x has the same items, reversed, plus xc.
        ead(x, x_type->newN(N(items[4], items[1], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // Both items in x are different, plus xc.
        ead(x, x_type->newN(N(items[0], items[3], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[3], items[0], xc)), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(items[0], items[5], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[5], items[0], xc)), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(items[2], items[3], xc)), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(items[3], items[2], xc)), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(items[2], items[5], xc)), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(items[5], items[2], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // The first item in x is different, plus xc.
        ead(x, x_type->newN(N(items[0], items[4], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[2], items[4], xc)), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(items[5], items[4], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // The second item in x is different, plus xc.
        ead(x, x_type->newN(N(items[1], items[0], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[1], items[3], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[1], items[5], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // One-item x, plus xc.
        ead(x, x_type->newN(N(items[0], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[1], xc)), assert_obj(any_cmp(s, x), is, i4_to_xc));
        ead(x, x_type->newN(N(items[2], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // x starts with xc.
        ead(x, x_type->newN(N(xc)), assert_obj(any_cmp(s, x), is, i1_to_xc));
        ead(x, x_type->newN(N(xc, items[1])), assert_obj(any_cmp(s, x), is, i1_to_xc));
        ead(x, x_type->newN(N(xc, items[4])), assert_obj(any_cmp(s, x), is, i1_to_xc));

        // "Empty x", "x is s" and "exception passthrough" are tested in test_sequence.

        assert_sequence(s, items[1], items[4]);  // s unchanged.
        yp_decrefN(N(s));
    }

    // Two-item s, descending order.
    {
        ypObject *s = type->newN(N(items[4], items[1]));

        // x has the same items, plus xc.
        ead(x, x_type->newN(N(items[4], items[1], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // x has the same items, reversed, plus xc.
        ead(x, x_type->newN(N(items[1], items[4], xc)), assert_obj(any_cmp(s, x), is, x_gt));

        // Both items in x are different, plus xc.
        ead(x, x_type->newN(N(items[3], items[0], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[0], items[3], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[5], items[0], xc)), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(items[0], items[5], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[3], items[2], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[2], items[3], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[5], items[2], xc)), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(items[2], items[5], xc)), assert_obj(any_cmp(s, x), is, x_gt));

        // The first item in x is different, plus xc.
        ead(x, x_type->newN(N(items[0], items[1], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[3], items[1], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[5], items[1], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // The second item in x is different, plus xc.
        ead(x, x_type->newN(N(items[4], items[0], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[4], items[2], xc)), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(items[4], items[5], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // One-item x, plus xc.
        ead(x, x_type->newN(N(items[3], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[4], xc)), assert_obj(any_cmp(s, x), is, i1_to_xc));
        ead(x, x_type->newN(N(items[5], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // x starts with xc.
        ead(x, x_type->newN(N(xc)), assert_obj(any_cmp(s, x), is, i4_to_xc));
        ead(x, x_type->newN(N(xc, items[1])), assert_obj(any_cmp(s, x), is, i4_to_xc));
        ead(x, x_type->newN(N(xc, items[4])), assert_obj(any_cmp(s, x), is, i4_to_xc));

        // "Empty x", "x is s" and "exception passthrough" are tested in test_sequence.

        assert_sequence(s, items[4], items[1]);  // s unchanged.
        yp_decrefN(N(s));
    }

    // One-item s.
    {
        ypObject *s = type->newN(N(items[1]));

        // x has the same items, plus xc.
        ead(x, x_type->newN(N(items[1], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // x has a different item, plus xc.
        ead(x, x_type->newN(N(items[0], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[2], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // Two-item x, plus xc.
        ead(x, x_type->newN(N(items[1], items[4], xc)), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(items[0], items[4], xc)), assert_obj(any_cmp(s, x), is, x_gt));
        ead(x, x_type->newN(N(items[2], items[4], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // x starts with xc.
        ead(x, x_type->newN(N(xc)), assert_obj(any_cmp(s, x), is, i1_to_xc));
        ead(x, x_type->newN(N(xc, items[1])), assert_obj(any_cmp(s, x), is, i1_to_xc));
        ead(x, x_type->newN(N(xc, items[4])), assert_obj(any_cmp(s, x), is, i1_to_xc));

        // "Empty x", "x is s" and "exception passthrough" are tested in test_sequence.

        assert_sequence(s, items[1]);  // s unchanged.
        yp_decrefN(N(s));
    }

    // Empty s.
    {
        ypObject *s = type->newN(0);

        // Two-item x, plus xc.
        ead(x, x_type->newN(N(items[1], items[4], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // One-item x, plus xc.
        ead(x, x_type->newN(N(items[1], xc)), assert_obj(any_cmp(s, x), is, x_lt));

        // x starts with xc.
        ead(x, x_type->newN(N(xc)), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(xc, items[1])), assert_obj(any_cmp(s, x), is, x_lt));
        ead(x, x_type->newN(N(xc, items[4])), assert_obj(any_cmp(s, x), is, x_lt));

        // "Empty x", "x is s" and "exception passthrough" are tested in test_sequence.

        assert_len(s, 0);  // s unchanged.
        yp_decrefN(N(s));
    }

#undef assert_cmp_fails

    yp_decref(xc);
    obj_array_decref(items);
    uniqueness_dealloc(uq);
}

static MunitResult test_lt(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    peer_type_t     *peer;
    fixture_type_t **x_type;

    // lt is only supported for friendly x.
    for (peer = type->peers; peer->type != NULL; peer++) {
        if (peer->type->is_string) {
            _test_comparisons(type, peer->type, yp_lt, /*x_lt=*/yp_True, /*x_gt=*/yp_False);
        } else {
            _test_comparisons_not_supported(type, peer->type, yp_lt, yp_TypeError);
        }
    }

    // Binary strings cannot be compared with text strings.
    for (x_type = fixture_types_string->types; (*x_type) != NULL; x_type++) {
        if (isbinary(type) == isbinary(*x_type)) continue;
        _test_comparisons_not_supported(type, *x_type, yp_lt, yp_TypeError);
    }

    return MUNIT_OK;
}

static MunitResult test_le(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    peer_type_t     *peer;
    fixture_type_t **x_type;

    // le is only supported for friendly x.
    for (peer = type->peers; peer->type != NULL; peer++) {
        if (peer->type->is_string) {
            _test_comparisons(type, peer->type, yp_le, /*x_lt=*/yp_True, /*x_gt=*/yp_False);
        } else {
            _test_comparisons_not_supported(type, peer->type, yp_le, yp_TypeError);
        }
    }

    // Binary strings cannot be compared with text strings.
    for (x_type = fixture_types_string->types; (*x_type) != NULL; x_type++) {
        if (isbinary(type) == isbinary(*x_type)) continue;
        _test_comparisons_not_supported(type, *x_type, yp_le, yp_TypeError);
    }

    return MUNIT_OK;
}

static MunitResult test_eq(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    peer_type_t     *peer;
    fixture_type_t **x_type;

    // eq is only supported for friendly x.
    for (peer = type->peers; peer->type != NULL; peer++) {
        if (peer->type->is_string) {
            _test_comparisons(type, peer->type, yp_eq, /*x_lt=*/yp_False, /*x_gt=*/yp_False);
        } else {
            _test_comparisons_not_supported(type, peer->type, yp_eq, yp_False);
        }
    }

    // Binary strings cannot be compared with text strings.
    for (x_type = fixture_types_string->types; (*x_type) != NULL; x_type++) {
        if (isbinary(type) == isbinary(*x_type)) continue;
        _test_comparisons_not_supported(type, *x_type, yp_eq, yp_False);
    }

    return MUNIT_OK;
}

static MunitResult test_ne(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    peer_type_t     *peer;
    fixture_type_t **x_type;

    // ne is only supported for friendly x.
    for (peer = type->peers; peer->type != NULL; peer++) {
        if (peer->type->is_string) {
            _test_comparisons(type, peer->type, yp_ne, /*x_lt=*/yp_True, /*x_gt=*/yp_True);
        } else {
            _test_comparisons_not_supported(type, peer->type, yp_ne, yp_True);
        }
    }

    // Binary strings cannot be compared with text strings.
    for (x_type = fixture_types_string->types; (*x_type) != NULL; x_type++) {
        if (isbinary(type) == isbinary(*x_type)) continue;
        _test_comparisons_not_supported(type, *x_type, yp_ne, yp_True);
    }

    return MUNIT_OK;
}

static MunitResult test_ge(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    peer_type_t     *peer;
    fixture_type_t **x_type;

    // ge is only supported for friendly x.
    for (peer = type->peers; peer->type != NULL; peer++) {
        if (peer->type->is_string) {
            _test_comparisons(type, peer->type, yp_ge, /*x_lt=*/yp_False, /*x_gt=*/yp_True);
        } else {
            _test_comparisons_not_supported(type, peer->type, yp_ge, yp_TypeError);
        }
    }

    // Binary strings cannot be compared with text strings.
    for (x_type = fixture_types_string->types; (*x_type) != NULL; x_type++) {
        if (isbinary(type) == isbinary(*x_type)) continue;
        _test_comparisons_not_supported(type, *x_type, yp_ge, yp_TypeError);
    }

    return MUNIT_OK;
}

static MunitResult test_gt(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    peer_type_t     *peer;
    fixture_type_t **x_type;

    // gt is only supported for friendly x.
    for (peer = type->peers; peer->type != NULL; peer++) {
        if (peer->type->is_string) {
            _test_comparisons(type, peer->type, yp_gt, /*x_lt=*/yp_False, /*x_gt=*/yp_True);
        } else {
            _test_comparisons_not_supported(type, peer->type, yp_gt, yp_TypeError);
        }
    }

    // Binary strings cannot be compared with text strings.
    for (x_type = fixture_types_string->types; (*x_type) != NULL; x_type++) {
        if (isbinary(type) == isbinary(*x_type)) continue;
        _test_comparisons_not_supported(type, *x_type, yp_gt, yp_TypeError);
    }

    return MUNIT_OK;
}

// String-specific tests not covered by test_sequence. In particular, this tests peers of differing
// encodings, for example concatenating str_1byte with str_4bytes.
static void _test_concat(fixture_type_t *type, fixture_type_t *x_type)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[2];
    ypObject     *x_items[2];
    // s contains only items; x contains only x_items. (See note at top of file.)
    obj_array_fill(items, uq, type->rand_elems->items);
    obj_array_fill(x_items, uq, x_type->rand_elems->items);

    // Basic concatenation.
    {
        ypObject *s = type->newN(N(items[0], items[1]));
        ypObject *x = x_type->newN(N(x_items[0], x_items[1]));
        ypObject *result = yp_concat(s, x);
        assert_type_is(result, type->yp_type);
        assert_sequence(result, items[0], items[1], x_items[0], x_items[1]);
        assert_sequence(s, items[0], items[1]);  // s unchanged.
        yp_decrefN(N(s, x, result));
    }

    // "s is empty", "x is empty", "both are empty", and "x is s" are tested in test_sequence.

    // Duplicates: items[0] is duplicated in s, x_items[1] in x.
    {
        ypObject *s = type->newN(N(items[0], items[1], items[0]));
        ypObject *x = x_type->newN(N(x_items[0], x_items[1], x_items[1]));
        ypObject *result = yp_concat(s, x);
        assert_sequence(result, items[0], items[1], items[0], x_items[0], x_items[1], x_items[1]);
        yp_decrefN(N(s, x, result));
    }

    // "Failing iterators", "x is not an iterable", and "exception passthrough" are in
    // test_sequence.

    obj_array_decref(x_items);
    obj_array_decref(items);
    uniqueness_dealloc(uq);
}

static MunitResult test_concat(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    peer_type_t    *peer;

    for (peer = type->peers; peer->type != NULL; peer++) {
        // Concatenation with non-string peers is tested in test_sequence.
        if (!peer->type->is_string) continue;
        _test_concat(type, peer->type);
    }

    return MUNIT_OK;
}

// FIXME More string-specific getslice tests...and also setslice and the other methods where we
// need to convert between character widths.
// FIXME getslice, particularly testing the downconvert (1from2, 1from4, and 2from4 are not covered
// currently.)
static MunitResult test_getslice(const MunitParameter params[], fixture_t *fixture)
{
    // fixture_type_t *type = fixture->type;
    // ypObject       *s = type->fromordsCN(N(0x101, 0xF0, 0x11010));
    // ypObject       *slice;
    // assert_not_raises(slice = yp_getsliceC4(s, 0, 2, 2));
    // yp_decrefN(N(s, slice));
    return MUNIT_OK;
}

// Shared tests for yp_findC5, yp_indexC5, yp_rfindC5, yp_rindexC5, etc. The _test_findC in
// test_sequence checks for the behaviour shared amongst all sequences; this _test_findC considers
// the behaviour unique to strings, namely substring matching.
static void _test_findC(fixture_type_t *type,
        yp_ssize_t (*any_findC)(ypObject *, ypObject *, ypObject **),
        yp_ssize_t (*any_findC5)(ypObject *, ypObject *, yp_ssize_t, yp_ssize_t, ypObject **),
        int forward, int raises)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[3];
    ypObject     *s;
    ypObject     *x_0_1;
    ypObject     *x_1_2;
    ypObject     *x_0_2;
    ypObject     *x_1_0;
    ypObject     *empty = type->newN(0);
    // FIXME Update for cross-encoding tests.
    // s contains only items; x contains either items or x_items. (See note at top of file.)
    obj_array_fill(items, uq, type->rand_elems->items);
    s = type->newN(N(items[0], items[1], items[2]));
    // FIXME Test against different "other" types (the other pair, really)
    x_0_1 = type->newN(N(items[0], items[1]));
    x_1_2 = type->newN(N(items[1], items[2]));
    x_0_2 = type->newN(N(items[0], items[2]));
    x_1_0 = type->newN(N(items[1], items[0]));

#define assert_not_found_exc(expression)                    \
    do {                                                    \
        ypObject *exc = yp_None;                            \
        assert_ssizeC(expression, ==, -1);                  \
        if (raises) assert_isexception(exc, yp_ValueError); \
    } while (0)

    assert_ssizeC_exc(any_findC(s, x_0_1, &exc), ==, 0);                // Sub-string.
    assert_ssizeC_exc(any_findC(s, x_1_2, &exc), ==, 1);                // Sub-string.
    assert_not_found_exc(any_findC(s, x_0_2, &exc));                    // Out-of-order.
    assert_not_found_exc(any_findC(s, x_1_0, &exc));                    // Out-of-order.
    assert_ssizeC_exc(any_findC(s, empty, &exc), ==, forward ? 0 : 3);  // Empty.
    assert_ssizeC_exc(any_findC(s, s, &exc), ==, 0);                    // Self.

    assert_ssizeC_exc(any_findC5(s, x_0_1, 0, 3, &exc), ==, 0);  // Total slice.
    assert_ssizeC_exc(any_findC5(s, x_0_1, 0, 2, &exc), ==, 0);  // Exact slice.
    assert_not_found_exc(any_findC5(s, x_0_1, 0, 1, &exc));      // Too-small slice.
    assert_not_found_exc(any_findC5(s, x_0_1, 0, 0, &exc));      // Empty slice.

    assert_ssizeC_exc(any_findC5(s, x_1_2, 1, 3, &exc), ==, 1);  // Exact slice.
    assert_not_found_exc(any_findC5(s, x_1_2, 1, 2, &exc));      // Too-small slice.
    assert_not_found_exc(any_findC5(s, x_1_2, 1, 1, &exc));      // Empty slice.

    assert_ssizeC_exc(any_findC5(s, empty, 0, 3, &exc), ==, forward ? 0 : 3);  // Empty, total.
    assert_ssizeC_exc(any_findC5(s, empty, 1, 2, &exc), ==, forward ? 1 : 2);  // Empty, partial.
    assert_ssizeC_exc(any_findC5(s, empty, 2, 2, &exc), ==, 2);                // Empty, empty.

    assert_ssizeC_exc(any_findC5(s, s, 0, 3, &exc), ==, 0);  // Self, exact.
    assert_not_found_exc(any_findC5(s, s, 1, 2, &exc));      // Self, too-small.
    assert_not_found_exc(any_findC5(s, s, 1, 1, &exc));      // Self, empty.

    // FIXME That empty slice bug thing.
    // FIXME !forward substrings?
    // FIXME Anything else to add here?

#undef assert_not_found_exc

    obj_array_decref(items);
    yp_decrefN(N(s, empty, x_0_1, x_1_2, x_0_2, x_1_0));
    uniqueness_dealloc(uq);
}

static MunitResult test_findC(const MunitParameter params[], fixture_t *fixture)
{
    _test_findC(fixture->type, yp_findC, yp_findC5, /*forward=*/TRUE, /*raises=*/FALSE);
    return MUNIT_OK;
}

static MunitResult test_indexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_findC(fixture->type, yp_indexC, yp_indexC5, /*forward=*/TRUE, /*raises=*/TRUE);
    return MUNIT_OK;
}

static MunitResult test_rfindC(const MunitParameter params[], fixture_t *fixture)
{
    _test_findC(fixture->type, yp_rfindC, yp_rfindC5, /*forward=*/FALSE, /*raises=*/FALSE);
    return MUNIT_OK;
}

static MunitResult test_rindexC(const MunitParameter params[], fixture_t *fixture)
{
    _test_findC(fixture->type, yp_rindexC, yp_rindexC5, /*forward=*/FALSE, /*raises=*/TRUE);
    return MUNIT_OK;
}

// FIXME test_countC, for non-overlapping substrings.

// String-specific tests not covered by test_sequence. In particular, this tests peers of differing
// encodings, for example setting a slice of str_1byte to a str_4bytes.
static void _test_setsliceC(fixture_type_t *type, fixture_type_t *x_type)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[6];
    ypObject     *x_items[32];
    // s contains only items; x contains only x_items. (See note at top of file.)
    obj_array_fill(items, uq, type->rand_elems->items);
    obj_array_fill(x_items, uq, x_type->rand_elems->items);

    // Immutables don't support setslice.
    if (!type->is_mutable) {
        ypObject *s = type->newN(N(items[0], items[1]));
        ypObject *two = x_type->newN(N(x_items[2]));
        assert_raises_exc(yp_setsliceC6(s, 0, 1, 1, two, &exc), yp_TypeError);
        assert_sequence(s, items[0], items[1]);
        yp_decrefN(N(s, two));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic slice.
    {
        ypObject *s = type->newN(N(items[0], items[1]));
        ypObject *two = x_type->newN(N(x_items[2]));
        ypObject *three = x_type->newN(N(x_items[3]));
        assert_not_raises_exc(yp_setsliceC6(s, 0, 1, 1, two, &exc));
        assert_sequence(s, x_items[2], items[1]);
        assert_not_raises_exc(yp_setsliceC6(s, 1, 2, 1, three, &exc));
        assert_sequence(s, x_items[2], x_items[3]);
        yp_decrefN(N(s, two, three));
    }

    // Negative step.
    {
        ypObject *s = type->newN(N(items[0], items[1]));
        ypObject *two = x_type->newN(N(x_items[2]));
        ypObject *three = x_type->newN(N(x_items[3]));
        assert_not_raises_exc(yp_setsliceC6(s, -1, -2, -1, two, &exc));
        assert_sequence(s, items[0], x_items[2]);
        assert_not_raises_exc(yp_setsliceC6(s, -2, -3, -1, three, &exc));
        assert_sequence(s, x_items[3], x_items[2]);
        yp_decrefN(N(s, two, three));
    }

    // Total slice, forward and backward.
    {
        ypObject *s = type->newN(N(items[0], items[1]));
        ypObject *four_five = x_type->newN(N(x_items[4], x_items[5]));
        ypObject *six_seven = x_type->newN(N(x_items[6], x_items[7]));
        assert_not_raises_exc(yp_setsliceC6(s, 0, 2, 1, four_five, &exc));
        assert_sequence(s, x_items[4], x_items[5]);
        assert_not_raises_exc(yp_setsliceC6(s, -1, -3, -1, six_seven, &exc));
        assert_sequence(s, x_items[7], x_items[6]);
        yp_decrefN(N(s, four_five, six_seven));
    }

    // Step of 2, -2.
    {
        ypObject *s = type->newN(N(items[0], items[1], items[2], items[3], items[4]));
        ypObject *five_six_seven = x_type->newN(N(x_items[5], x_items[6], x_items[7]));
        ypObject *eight_nine_ten = x_type->newN(N(x_items[8], x_items[9], x_items[10]));
        assert_not_raises_exc(yp_setsliceC6(s, 0, 5, 2, five_six_seven, &exc));
        assert_sequence(s, x_items[5], items[1], x_items[6], items[3], x_items[7]);
        assert_not_raises_exc(yp_setsliceC6(s, -1, -6, -2, eight_nine_ten, &exc));
        assert_sequence(s, x_items[10], items[1], x_items[9], items[3], x_items[8]);
        yp_decrefN(N(s, five_six_seven, eight_nine_ten));
    }

    // "Empty slices" is tested in test_sequence.

    // yp_SLICE_DEFAULT.
    {
        ypObject *s = type->newN(N(items[0], items[1]));
        ypObject *two = x_type->newN(N(x_items[2]));
        ypObject *three = x_type->newN(N(x_items[3]));
        ypObject *four_five = x_type->newN(N(x_items[4], x_items[5]));
        ypObject *six = x_type->newN(N(x_items[6]));
        ypObject *seven = x_type->newN(N(x_items[7]));
        ypObject *eight_nine = x_type->newN(N(x_items[8], x_items[9]));
        assert_not_raises_exc(yp_setsliceC6(s, yp_SLICE_DEFAULT, 1, 1, two, &exc));
        assert_sequence(s, x_items[2], items[1]);
        assert_not_raises_exc(yp_setsliceC6(s, 1, yp_SLICE_DEFAULT, 1, three, &exc));
        assert_sequence(s, x_items[2], x_items[3]);
        assert_not_raises_exc(
                yp_setsliceC6(s, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, 1, four_five, &exc));
        assert_sequence(s, x_items[4], x_items[5]);
        assert_not_raises_exc(yp_setsliceC6(s, yp_SLICE_DEFAULT, -2, -1, six, &exc));
        assert_sequence(s, x_items[4], x_items[6]);
        assert_not_raises_exc(yp_setsliceC6(s, -2, yp_SLICE_DEFAULT, -1, seven, &exc));
        assert_sequence(s, x_items[7], x_items[6]);
        assert_not_raises_exc(
                yp_setsliceC6(s, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT, -1, eight_nine, &exc));
        assert_sequence(s, x_items[9], x_items[8]);
        yp_decrefN(N(s, two, three, four_five, six, seven, eight_nine));
    }

    // yp_SLICE_LAST.
    {
        ypObject *s = type->newN(N(items[0], items[1]));
        ypObject *empty = x_type->newN(0);
        ypObject *two = x_type->newN(N(x_items[2]));
        ypObject *three_four = x_type->newN(N(x_items[3], x_items[4]));
        assert_not_raises_exc(yp_setsliceC6(s, yp_SLICE_LAST, 2, 1, empty, &exc));
        assert_sequence(s, items[0], items[1]);
        assert_not_raises_exc(yp_setsliceC6(s, 1, yp_SLICE_LAST, 1, two, &exc));
        assert_sequence(s, items[0], x_items[2]);
        assert_not_raises_exc(yp_setsliceC6(s, yp_SLICE_LAST, yp_SLICE_LAST, 1, empty, &exc));
        assert_sequence(s, items[0], x_items[2]);
        assert_not_raises_exc(yp_setsliceC6(s, yp_SLICE_LAST, -3, -1, three_four, &exc));
        assert_sequence(s, x_items[4], x_items[3]);
        assert_not_raises_exc(yp_setsliceC6(s, -1, yp_SLICE_LAST, -1, empty, &exc));
        assert_sequence(s, x_items[4], x_items[3]);
        assert_not_raises_exc(yp_setsliceC6(s, yp_SLICE_LAST, yp_SLICE_LAST, -1, empty, &exc));
        assert_sequence(s, x_items[4], x_items[3]);
        yp_decrefN(N(s, empty, two, three_four));
    }

    // "Invalid slices" is tested in test_sequence.

    // Regular slices (step==1) can grow and shrink the sequence.
    {
        ypObject *s = type->newN(0);
        ypObject *empty = x_type->newN(0);
        ypObject *zero_one = x_type->newN(N(x_items[0], x_items[1]));
        ypObject *two = x_type->newN(N(x_items[2]));
        ypObject *three = x_type->newN(N(x_items[3]));
        ypObject *four_five = x_type->newN(N(x_items[4], x_items[5]));
        ypObject *six_seven_eight = x_type->newN(N(x_items[6], x_items[7], x_items[8]));
        ypObject *nine = x_type->newN(N(x_items[9]));
        assert_not_raises_exc(yp_setsliceC6(s, 0, 0, 1, zero_one, &exc));
        assert_sequence(s, x_items[0], x_items[1]);
        assert_not_raises_exc(yp_setsliceC6(s, 0, 0, 1, empty, &exc));
        assert_sequence(s, x_items[0], x_items[1]);
        assert_not_raises_exc(yp_setsliceC6(s, 0, 0, 1, two, &exc));
        assert_sequence(s, x_items[2], x_items[0], x_items[1]);
        assert_not_raises_exc(yp_setsliceC6(s, 1, 2, 1, empty, &exc));
        assert_sequence(s, x_items[2], x_items[1]);
        assert_not_raises_exc(yp_setsliceC6(s, 1, 2, 1, three, &exc));
        assert_sequence(s, x_items[2], x_items[3]);
        assert_not_raises_exc(yp_setsliceC6(s, 1, 2, 1, four_five, &exc));
        assert_sequence(s, x_items[2], x_items[4], x_items[5]);
        assert_not_raises_exc(yp_setsliceC6(s, 0, 3, 1, six_seven_eight, &exc));
        assert_sequence(s, x_items[6], x_items[7], x_items[8]);
        assert_not_raises_exc(yp_setsliceC6(s, 0, 3, 1, nine, &exc));
        assert_sequence(s, x_items[9]);
        assert_not_raises_exc(yp_setsliceC6(s, 0, 1, 1, empty, &exc));
        assert_len(s, 0);
        yp_decrefN(N(s, empty, zero_one, two, three, four_five, six_seven_eight, nine));
    }

    // "Extended slices cannot grow/shrink" and "x is s" is tested in test_sequence.

    // x is large, growing the sequence (likely triggering a resize).
    {
        ypObject *s = type->newN(N(items[0], items[1]));
        ypObject *x = x_type->newN(N(x_items[0], x_items[1], x_items[2], x_items[3], x_items[4],
                x_items[5], x_items[6], x_items[7], x_items[8], x_items[9], x_items[10],
                x_items[11], x_items[12], x_items[13], x_items[14], x_items[15], x_items[16],
                x_items[17], x_items[18], x_items[19], x_items[20], x_items[21], x_items[22],
                x_items[23], x_items[24], x_items[25], x_items[26], x_items[27], x_items[28],
                x_items[29], x_items[30], x_items[31]));
        assert_not_raises_exc(yp_setsliceC6(s, 1, 2, 1, x, &exc));
        assert_sequence(s, items[0], x_items[0], x_items[1], x_items[2], x_items[3], x_items[4],
                x_items[5], x_items[6], x_items[7], x_items[8], x_items[9], x_items[10],
                x_items[11], x_items[12], x_items[13], x_items[14], x_items[15], x_items[16],
                x_items[17], x_items[18], x_items[19], x_items[20], x_items[21], x_items[22],
                x_items[23], x_items[24], x_items[25], x_items[26], x_items[27], x_items[28],
                x_items[29], x_items[30], x_items[31]);
        yp_decrefN(N(s, x));
    }

    // Duplicates: items[0] is duplicated in s, x_items[1] in x.
    if (!x_type->is_patterned) {
        ypObject *s = type->newN(N(items[0], items[2], items[0]));
        ypObject *x = x_type->newN(N(x_items[2], x_items[1], x_items[1]));
        assert_not_raises_exc(yp_setsliceC6(s, 1, 1, 1, x, &exc));
        assert_sequence(s, items[0], x_items[2], x_items[1], x_items[1], items[2], items[0]);
        yp_decrefN(N(s, x));
    }

    // "Failing iterators", "x is not an iterable", and "exception passthrough" are tested in
    // test_sequence.

tear_down:
    obj_array_decref(x_items);
    obj_array_decref(items);
    uniqueness_dealloc(uq);
}

static MunitResult test_setsliceC(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    peer_type_t    *peer;

    for (peer = type->peers; peer->type != NULL; peer++) {
        // Set slice with non-string peers is tested in test_sequence.
        if (!peer->type->is_string) continue;
        _test_setsliceC(type, peer->type);
    }

    return MUNIT_OK;
}

// FIXME delslice, particularly testing the downconvert (1from2, 1from4, and 2from4 are not covered
// currently.)

// String-specific tests not covered by test_sequence. In particular, this tests peers of differing
// encodings, for example extending a str_1byte with a str_4bytes.
static void _test_extend(fixture_type_t *type, fixture_type_t *x_type)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[4];
    ypObject     *x_items[32];
    // s contains only items; x contains only x_items. (See note at top of file.)
    obj_array_fill(items, uq, type->rand_elems->items);
    obj_array_fill(x_items, uq, x_type->rand_elems->items);

    // Immutables don't support extend.
    if (!type->is_mutable) {
        ypObject *s = type->newN(N(items[0], items[1]));
        ypObject *two = x_type->newN(N(x_items[2]));
        assert_raises_exc(yp_extend(s, two, &exc), yp_MethodError);
        assert_sequence(s, items[0], items[1]);
        yp_decrefN(N(s, two));
        goto tear_down;  // Skip remaining tests.
    }

    // Basic extend.
    {
        ypObject *s = type->newN(N(items[0], items[1]));
        ypObject *x = x_type->newN(N(x_items[2], x_items[3]));
        assert_not_raises_exc(yp_extend(s, x, &exc));
        assert_sequence(s, items[0], items[1], x_items[2], x_items[3]);
        yp_decrefN(N(s, x));
    }

    // "s is empty", "x is empty", "both are empty", "x is s", and "x contains s" are tested in
    // test_sequence.

    // x is large (likely triggering a resize).
    {
        ypObject *s = type->newN(N(items[0], items[1]));
        ypObject *x = x_type->newN(N(x_items[0], x_items[1], x_items[2], x_items[3], x_items[4],
                x_items[5], x_items[6], x_items[7], x_items[8], x_items[9], x_items[10],
                x_items[11], x_items[12], x_items[13], x_items[14], x_items[15], x_items[16],
                x_items[17], x_items[18], x_items[19], x_items[20], x_items[21], x_items[22],
                x_items[23], x_items[24], x_items[25], x_items[26], x_items[27], x_items[28],
                x_items[29], x_items[30], x_items[31]));
        assert_not_raises_exc(yp_extend(s, x, &exc));
        assert_sequence(s, items[0], items[1], x_items[0], x_items[1], x_items[2], x_items[3],
                x_items[4], x_items[5], x_items[6], x_items[7], x_items[8], x_items[9], x_items[10],
                x_items[11], x_items[12], x_items[13], x_items[14], x_items[15], x_items[16],
                x_items[17], x_items[18], x_items[19], x_items[20], x_items[21], x_items[22],
                x_items[23], x_items[24], x_items[25], x_items[26], x_items[27], x_items[28],
                x_items[29], x_items[30], x_items[31]);
        yp_decrefN(N(s, x));
    }

    // Duplicates: items[0] is duplicated in s, x_items[1] in x.
    if (!x_type->is_patterned) {
        ypObject *s = type->newN(N(items[0], items[2], items[0]));
        ypObject *x = x_type->newN(N(x_items[2], x_items[1], x_items[1]));
        assert_not_raises_exc(yp_extend(s, x, &exc));
        assert_sequence(s, items[0], items[2], items[0], x_items[2], x_items[1], x_items[1]);
        yp_decrefN(N(s, x));
    }

    // "Failing iterators", "x is not an iterable", and "exception passthrough" are tested in
    // test_sequence.

tear_down:
    obj_array_decref(x_items);
    obj_array_decref(items);
    uniqueness_dealloc(uq);
}

static MunitResult test_extend(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    peer_type_t    *peer;

    for (peer = type->peers; peer->type != NULL; peer++) {
        // Extend with non-string peers is tested in test_sequence.
        if (!peer->type->is_string) continue;
        _test_extend(type, peer->type);
    }

    return MUNIT_OK;
}

// FIXME test_remove and test_discard, for substrings.

// Additional tests for specific characters are in test_string_classifier.
static MunitResult test_isalnum(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isalnum.
    // FIXME This raises yp_SystemLimitationError on *_2bytes and *_4bytes. Fix.
    // ead(s, rand_obj(NULL, type), assert_not_raises(yp_isalnum(s)));

    // Empty s.
    ead(s, type->newN(0), assert_obj(yp_isalnum(s), is, yp_False));

    return MUNIT_OK;
}

// Additional tests for specific characters are in test_string_classifier.
static MunitResult test_isalpha(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isalpha.
    // FIXME This raises yp_SystemLimitationError on *_2bytes and *_4bytes. Fix.
    // ead(s, rand_obj(NULL, type), assert_not_raises(yp_isalpha(s)));

    // Empty s.
    ead(s, type->newN(0), assert_obj(yp_isalpha(s), is, yp_False));

    return MUNIT_OK;
}

// Additional tests for specific characters are in test_string_classifier.
static MunitResult test_isascii(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isascii.
    ead(s, rand_obj(NULL, type), assert_not_raises(yp_isascii(s)));

    // Empty s.
    ead(s, type->newN(0), assert_obj(yp_isascii(s), is, yp_True));

    return MUNIT_OK;
}

// Additional tests for specific characters are in test_string_classifier.
static MunitResult test_isdecimal(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Binary strings don't support isdecimal.
    if (isbinary(type)) {
        ead(s, rand_obj(NULL, type), assert_raises(yp_isdecimal(s), yp_MethodError));
        goto tear_down;
    }

    // Basic isdecimal.
    // FIXME This raises yp_SystemLimitationError on *_2bytes and *_4bytes. Fix.
    // ead(s, rand_obj(NULL, type), assert_not_raises(yp_isdecimal(s)));

    // Empty s.
    ead(s, type->newN(0), assert_obj(yp_isdecimal(s), is, yp_False));

tear_down:
    return MUNIT_OK;
}

// Additional tests for specific characters are in test_string_classifier.
static MunitResult test_isdigit(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isdigit.
    // FIXME This raises yp_SystemLimitationError on *_2bytes and *_4bytes. Fix.
    // ead(s, rand_obj(NULL, type), assert_not_raises(yp_isdigit(s)));

    // Empty s.
    ead(s, type->newN(0), assert_obj(yp_isdigit(s), is, yp_False));

    return MUNIT_OK;
}

// Additional tests for specific characters are in test_string_classifier.
static MunitResult test_isidentifier(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Binary strings don't support isidentifier.
    if (isbinary(type)) {
        ead(s, rand_obj(NULL, type), assert_raises(yp_isidentifier(s), yp_MethodError));
        goto tear_down;
    }

    // Basic isidentifier.
    // FIXME This raises yp_SystemLimitationError on *_2bytes and *_4bytes. Fix.
    // ead(s, rand_obj(NULL, type), assert_not_raises(yp_isidentifier(s)));

    // Empty s.
    ead(s, type->newN(0), assert_obj(yp_isidentifier(s), is, yp_False));

tear_down:
    return MUNIT_OK;
}

// Additional tests for specific characters are in test_string_classifier.
static MunitResult test_islower(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic islower.
    // FIXME This raises yp_SystemLimitationError on *_2bytes and *_4bytes. Fix.
    // ead(s, rand_obj(NULL, type), assert_not_raises(yp_islower(s)));

    // Empty s.
    ead(s, type->newN(0), assert_obj(yp_islower(s), is, yp_False));

    return MUNIT_OK;
}

// Additional tests for specific characters are in test_string_classifier.
static MunitResult test_isnumeric(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Binary strings don't support isnumeric.
    if (isbinary(type)) {
        ead(s, rand_obj(NULL, type), assert_raises(yp_isnumeric(s), yp_MethodError));
        goto tear_down;
    }

    // Basic isnumeric.
    // FIXME This raises yp_SystemLimitationError on *_2bytes and *_4bytes. Fix.
    // ead(s, rand_obj(NULL, type), assert_not_raises(yp_isnumeric(s)));

    // Empty s.
    ead(s, type->newN(0), assert_obj(yp_isnumeric(s), is, yp_False));

tear_down:
    return MUNIT_OK;
}

// Additional tests for specific characters are in test_string_classifier.
static MunitResult test_isprintable(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Binary strings don't support isprintable.
    if (isbinary(type)) {
        ead(s, rand_obj(NULL, type), assert_raises(yp_isprintable(s), yp_MethodError));
        goto tear_down;
    }

    // Basic isprintable.
    // FIXME This raises yp_SystemLimitationError on *_2bytes and *_4bytes. Fix.
    // ead(s, rand_obj(NULL, type), assert_not_raises(yp_isprintable(s)));

    // Empty s.
    ead(s, type->newN(0), assert_obj(yp_isprintable(s), is, yp_True));

tear_down:
    return MUNIT_OK;
}

// Additional tests for specific characters are in test_string_classifier.
static MunitResult test_isspace(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isspace.
    // FIXME This raises yp_SystemLimitationError on *_2bytes and *_4bytes. Fix.
    // ead(s, rand_obj(NULL, type), assert_not_raises(yp_isspace(s)));

    // Empty s.
    ead(s, type->newN(0), assert_obj(yp_isspace(s), is, yp_False));

    return MUNIT_OK;
}

// Additional tests for specific characters are in test_string_classifier.
static MunitResult test_isupper(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isupper.
    // FIXME This raises yp_SystemLimitationError on *_2bytes and *_4bytes. Fix.
    // ead(s, rand_obj(NULL, type), assert_not_raises(yp_isupper(s)));

    // No cased characters. Non-ascii characters are non-cased in binary strings.
    ead(s, type->newN(0), assert_obj(yp_isupper(s), is, yp_False));

    return MUNIT_OK;
}

// For startswith and endswith.
static void _test_tailmatch_not_supported(fixture_type_t *type, fixture_type_t *x_type,
        ypObject *(*any_tailmatch)(ypObject *, ypObject *),
        ypObject *(*any_tailmatchC4)(ypObject *, ypObject *, yp_ssize_t, yp_ssize_t))
{
    ypObject  *items[2];
    ypObject  *s;
    ypObject  *empty = type->newN(0);
    ypObject  *x_values[] = {rand_obj(NULL, x_type), x_type->newN(0), NULL};
    ypObject **x;
    obj_array_fill(items, NULL, type->rand_elems->items);
    s = type->newN(N(items[0], items[1]));

    for (x = x_values; (*x) != NULL; x++) {
        assert_raises(any_tailmatch(s, *x), yp_TypeError);
        assert_raises(any_tailmatch(empty, *x), yp_TypeError);
        assert_raises(any_tailmatchC4(s, *x, 0, 2), yp_TypeError);
        assert_raises(any_tailmatchC4(s, *x, 0, 0), yp_TypeError);
        assert_raises(any_tailmatchC4(empty, *x, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT), yp_TypeError);
        ead(x_tuple, yp_tupleN(N(*x)), assert_raises(any_tailmatch(s, x_tuple), yp_TypeError));
        ead(x_tuple, yp_tupleN(N(*x, s)), assert_raises(any_tailmatch(s, x_tuple), yp_TypeError));

        // Optimization: early exit if x is a tuple with a match, even if x contains bad types.
        // FIXME Python 3.13 catches this error. We should too.
        ead(x_tuple, yp_tupleN(N(s, x)), assert_obj(any_tailmatch(s, x_tuple), is, yp_True));
    }

    obj_array_decref(x_values);
    obj_array_decref(items);
    yp_decrefN(N(s, empty));
}

static void _test_startswith(fixture_type_t *type, fixture_type_t *x_type)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *items[1];
    ypObject     *x_items[5];
    ypObject     *s;
    ypObject     *empty = type->newN(0);
    ypObject     *x_empty = x_type->newN(0);
    ypObject     *x_0;
    ypObject     *x_1;
    ypObject     *x_1_1;
    ypObject     *x_1_2;
    ypObject     *x_1_4;
    ypObject     *x_2;
    ypObject     *x_2_1;
    ypObject     *x_3;
    // s contains both items and x_items; x contains only x_items. (See note at top of file.)
    obj_array_fill(items, uq, type->rand_elems->items);
    obj_array_fill(x_items, uq, x_type->rand_elems->items);
    s = type->newN(N(x_items[1], x_items[2], items[0], x_items[3]));
    x_0 = x_type->newN(N(x_items[0]));
    x_1 = x_type->newN(N(x_items[1]));
    x_1_1 = x_type->newN(N(x_items[1], x_items[1]));
    x_1_2 = x_type->newN(N(x_items[1], x_items[2]));
    x_1_4 = x_type->newN(N(x_items[0], x_items[4]));
    x_2 = x_type->newN(N(x_items[2]));
    x_2_1 = x_type->newN(N(x_items[2], x_items[1]));
    x_3 = x_type->newN(N(x_items[3]));

    // Basic startswith.
    assert_obj(yp_startswith(s, x_1), is, yp_True);
    assert_obj(yp_startswith(s, x_1_2), is, yp_True);

    // Characters not in string.
    assert_obj(yp_startswith(s, x_0), is, yp_False);
    assert_obj(yp_startswith(s, x_1_4), is, yp_False);

    // Characters not at start.
    assert_obj(yp_startswith(s, x_2), is, yp_False);
    assert_obj(yp_startswith(s, x_3), is, yp_False);
    ead(x, x_type->newN(N(items[0], x_items[3])), assert_obj(yp_startswith(s, x), is, yp_False));

    // Characters out-of-order or duplicated.
    assert_obj(yp_startswith(s, x_2_1), is, yp_False);
    assert_obj(yp_startswith(s, x_1_1), is, yp_False);

    // Empty x.
    assert_obj(yp_startswith(s, x_empty), is, yp_True);
    assert_obj(yp_startswith(empty, x_empty), is, yp_True);

    // Empty s.
    assert_obj(yp_startswith(empty, x_1), is, yp_False);
    assert_obj(yp_startswith(empty, x_1_2), is, yp_False);

    // Basic slice.
    assert_obj(yp_startswithC4(s, x_1, 0, 1), is, yp_True);
    assert_obj(yp_startswithC4(s, x_1, 1, 2), is, yp_False);
    assert_obj(yp_startswithC4(s, x_1_2, 0, 1), is, yp_False);
    assert_obj(yp_startswithC4(s, x_1_2, 1, 2), is, yp_False);

    // Negative indicies.
    assert_obj(yp_startswithC4(s, x_1_2, -4, -1), is, yp_True);
    assert_obj(yp_startswithC4(s, x_1_2, -1, 4), is, yp_False);
    assert_obj(yp_startswithC4(s, x_3, -4, -1), is, yp_False);
    assert_obj(yp_startswithC4(s, x_3, -1, 4), is, yp_True);

    // Total slice.
    assert_obj(yp_startswithC4(s, x_0, 0, 4), is, yp_False);
    assert_obj(yp_startswithC4(s, x_1, 0, 4), is, yp_True);
    assert_obj(yp_startswithC4(s, x_1_2, 0, 4), is, yp_True);

    // Total slice, negative indicies.
    assert_obj(yp_startswithC4(s, x_0, -4, 4), is, yp_False);
    assert_obj(yp_startswithC4(s, x_1, -4, 4), is, yp_True);
    assert_obj(yp_startswithC4(s, x_1_2, -4, 4), is, yp_True);

    // Empty slices.
    {
        slice_args_t slices[] = {
                // recall step is always 1 for startswith
                {0, 0, 1},     // typical empty slice
                {4, 99, 1},    // i>=len(s) (regardless of j)
                {-99, -5, 1},  // j<-len(s) (regardless of i)
                {2, 2, 1},     // i=j (regardless of k)
                {1, 0, 1},     // i>j
                {-1, -4, 1},   // reverse total slice...but k is always 1
        };
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(slices); i++) {
            slice_args_t args = slices[i];
            assert_obj(yp_startswithC4(s, x_1, args.start, args.stop), is, yp_False);
            assert_obj(yp_startswithC4(s, x_1_2, args.start, args.stop), is, yp_False);
            // XXX nohtyP _always_ treats start as in slice: https://bugs.python.org/issue24243
            assert_obj(yp_startswithC4(s, x_empty, args.start, args.stop), is, yp_True);
            assert_obj(yp_startswithC4(empty, x_empty, args.start, args.stop), is, yp_True);
        }
    }

    // yp_SLICE_DEFAULT.
    assert_obj(yp_startswithC4(s, x_1, yp_SLICE_DEFAULT, 1), is, yp_True);
    assert_obj(yp_startswithC4(s, x_1, 0, yp_SLICE_DEFAULT), is, yp_True);
    assert_obj(yp_startswithC4(s, x_1, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT), is, yp_True);
    assert_obj(yp_startswithC4(s, x_2, yp_SLICE_DEFAULT, 2), is, yp_False);
    assert_obj(yp_startswithC4(s, x_2, 1, yp_SLICE_DEFAULT), is, yp_True);
    assert_obj(yp_startswithC4(s, x_2, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT), is, yp_False);
    assert_obj(yp_startswithC4(s, x_0, yp_SLICE_DEFAULT, 2), is, yp_False);
    assert_obj(yp_startswithC4(s, x_0, 0, yp_SLICE_DEFAULT), is, yp_False);
    assert_obj(yp_startswithC4(s, x_0, yp_SLICE_DEFAULT, yp_SLICE_DEFAULT), is, yp_False);

    // yp_SLICE_LAST.
    assert_obj(yp_startswithC4(s, x_1, yp_SLICE_LAST, 2), is, yp_False);
    assert_obj(yp_startswithC4(s, x_1, 0, yp_SLICE_LAST), is, yp_True);
    assert_obj(yp_startswithC4(s, x_1, yp_SLICE_LAST, yp_SLICE_LAST), is, yp_False);
    assert_obj(yp_startswithC4(s, x_2, yp_SLICE_LAST, 2), is, yp_False);
    assert_obj(yp_startswithC4(s, x_2, 1, yp_SLICE_LAST), is, yp_True);
    assert_obj(yp_startswithC4(s, x_2, yp_SLICE_LAST, yp_SLICE_LAST), is, yp_False);
    assert_obj(yp_startswithC4(s, x_0, yp_SLICE_LAST, 2), is, yp_False);
    assert_obj(yp_startswithC4(s, x_0, 0, yp_SLICE_LAST), is, yp_False);
    assert_obj(yp_startswithC4(s, x_0, yp_SLICE_LAST, yp_SLICE_LAST), is, yp_False);

    // x equals s.
    {
        ypObject *x = x_type->newN(N(x_items[1], x_items[2], items[0], x_items[3]));
        assert_obj(yp_startswith(s, x), is, yp_True);
        assert_obj(yp_startswithC4(s, x, 1, 4), is, yp_False);
        assert_obj(yp_startswithC4(s, x, 0, 4), is, yp_True);
        yp_decrefN(N(x));
    }

    // x is s.
    assert_obj(yp_startswith(s, s), is, yp_True);
    assert_obj(yp_startswithC4(s, s, 0, 3), is, yp_False);
    assert_obj(yp_startswithC4(s, s, 0, 4), is, yp_True);

    // x is a tuple of strings.
    ead(x_tuple, yp_tupleN(N(x_0, x_1)), assert_obj(yp_startswith(s, x_tuple), is, yp_True));
    ead(x_tuple, yp_tupleN(N(x_0, x_1_4)), assert_obj(yp_startswith(s, x_tuple), is, yp_False));
    ead(x_tuple, yp_tupleN(N(x_1)), assert_obj(yp_startswithC4(s, x_tuple, 0, 4), is, yp_True));
    ead(x_tuple, yp_tupleN(N(x_1)), assert_obj(yp_startswithC4(s, x_tuple, 1, 4), is, yp_False));

    // x is an empty tuple.
    ead(x_tuple, yp_tupleN(0), assert_obj(yp_startswith(s, x_tuple), is, yp_False));
    ead(x_tuple, yp_tupleN(0), assert_obj(yp_startswith(empty, x_tuple), is, yp_False));
    ead(x_tuple, yp_tupleN(0), assert_obj(yp_startswithC4(s, x_tuple, 0, 0), is, yp_False));

    // x is an item. Supported on text as their x_items are strings.
    if (isbinary(type)) {
        assert_raises(yp_startswith(s, x_items[1]), yp_TypeError);
    } else {
        assert_obj(yp_startswith(s, x_items[1]), is, yp_True);
    }

    // x is a list, which is not supported. FIXME Should it be?
    ead(x, yp_listN(N(x_1)), assert_raises(yp_startswith(s, x), yp_TypeError));

    // x is not an iterable.
    assert_raises(yp_startswith(s, not_iterable), yp_TypeError);

    // Optimization: early exit if x is a tuple with a match, even if x contains bad types.
    // FIXME Python 3.13 catches this error. We should too.
    // FIXME yp_isdisjoint has early exit. We should standardize.
    ead(x_tuple, yp_tupleN(N(x_0, not_iterable)),
            assert_raises(yp_startswith(s, x_tuple), yp_TypeError));
    ead(x_tuple, yp_tupleN(N(x_1, not_iterable)),
            assert_obj(yp_startswith(s, x_tuple), is, yp_True));

    // Exception passthrough.
    assert_raises(yp_startswith(s, yp_SyntaxError), yp_SyntaxError);
    assert_raises(yp_startswithC4(s, yp_SyntaxError, 0, 1), yp_SyntaxError);
    assert_raises(yp_startswithC4(s, yp_SyntaxError, 0, 0), yp_SyntaxError);
    assert_raises(yp_startswith(empty, yp_SyntaxError), yp_SyntaxError);
    assert_raises(yp_startswithC4(empty, yp_SyntaxError, 0, 1), yp_SyntaxError);
    assert_raises(yp_startswithC4(empty, yp_SyntaxError, 0, 0), yp_SyntaxError);

    assert_sequence(s, x_items[1], x_items[2], items[0], x_items[3]);  // s unchanged.

    obj_array_decref(x_items);
    obj_array_decref(items);
    uniqueness_dealloc(uq);
    yp_decrefN(N(not_iterable, s, empty, x_empty, x_0, x_1, x_1_1, x_1_2, x_1_4, x_2, x_2_1, x_3));
}

static MunitResult test_startswith(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t  *type = fixture->type;
    peer_type_t     *peer;
    fixture_type_t **x_type;

    for (peer = type->peers; peer->type != NULL; peer++) {
        if (peer->type->is_string) {
            _test_startswith(type, peer->type);
        } else if (peer->type != fixture_type_tuple) {
            // Calling with a tuple is tested in _test_startswith and _test_tailmatch_not_supported.
            _test_tailmatch_not_supported(type, peer->type, yp_startswith, yp_startswithC4);
        }
    }

    // Binary strings cannot be compared with text strings.
    for (x_type = fixture_types_string->types; (*x_type) != NULL; x_type++) {
        if (isbinary(type) == isbinary(*x_type)) continue;
        _test_tailmatch_not_supported(type, *x_type, yp_startswith, yp_startswithC4);
    }

    return MUNIT_OK;
}

static MunitResult test_endswith(const MunitParameter params[], fixture_t *fixture)
{
    // FIXME
    return MUNIT_OK;
}


static MunitParameterEnum test_string_params[] = {
        {param_key_type, param_values_types_string}, {NULL}};

MunitTest test_string_tests[] = {TEST(test_lt, test_string_params),
        TEST(test_le, test_string_params), TEST(test_eq, test_string_params),
        TEST(test_ne, test_string_params), TEST(test_ge, test_string_params),
        TEST(test_gt, test_string_params), TEST(test_concat, test_string_params),
        TEST(test_getslice, test_string_params), TEST(test_findC, test_string_params),
        TEST(test_indexC, test_string_params), TEST(test_rfindC, test_string_params),
        TEST(test_rindexC, test_string_params), TEST(test_setsliceC, test_string_params),
        TEST(test_extend, test_string_params), TEST(test_isalnum, test_string_params),
        TEST(test_isalpha, test_string_params), TEST(test_isascii, test_string_params),
        TEST(test_isdecimal, test_string_params), TEST(test_isdigit, test_string_params),
        TEST(test_isidentifier, test_string_params), TEST(test_islower, test_string_params),
        TEST(test_isnumeric, test_string_params), TEST(test_isprintable, test_string_params),
        TEST(test_isspace, test_string_params), TEST(test_isupper, test_string_params),
        TEST(test_startswith, test_string_params), TEST(test_endswith, test_string_params), {NULL}};


extern void test_string_initialize(void) {}
