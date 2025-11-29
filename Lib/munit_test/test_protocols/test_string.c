
#include "munit_test/unittest.h"

// FIXME replace copy/paste sq with s.


#define O_a_GRAVE 0xE0        // 'à', "a with grave", a non-ascii latin-1 lowercase
#define O_A_GRAVE 0xC0        // 'À', "A with grave", a non-ascii latin-1 uppercase
#define O_SUPER1 0xb9         // '¹', "superscript 1", a non-ascii latin-1 non-decimal digit
#define O_1OVER4 0xbc         // '¼', "fraction 1/4", a non-ascii latin-1 non-digit numeric
#define O_BIG_LOWER 0x101     // 'ā', "a with macron", a non-latin-1 lowercase
#define O_BIG_TITLE 0x1C5     // 'ǅ', "D with z with caron", a non-latin-1 titlecase
#define O_BIG_UPPER 0x100     // 'Ā', "A with macron", a non-latin-1 uppercase
#define O_BIG_DECIMAL 0x661   // '١', "Arabic-Indic 1", a non-latin-1 decimal
#define O_BIG_DIGIT 0x2081    // '₁', "subscript 1", a non-latin-1 non-decimal digit
#define O_BIG_NUMERIC 0x2153  // '⅓', "fraction 1/3", a non-latin-1 non-digit numeric

// An array of all our test non-latin-1 characters, as ordinals.
static int ords_non_latin_1[] = {
        O_BIG_LOWER, O_BIG_TITLE, O_BIG_UPPER, O_BIG_DECIMAL, O_BIG_DIGIT, O_BIG_NUMERIC};

// test_latin_1_classifiers is a bit too complex for GCC.
#if defined(__GNUC__)
#pragma GCC optimize("no-var-tracking")
#endif

// FIXME Ensure yp_startswithC4/yp_endswithC4/yp_replaceC4/yp_lstrip2/yp_splitlines2/yp_encode3/etc
// properly handles exception passthrough, even in cases where one of the arguments would be ignored
// (e.g. empty str, empty slice).
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

// String-specific tests not covered by test_sequence. In particular, this tests peers of differing
// encodings, for example extending a str_1byte with a str_4bytes.
static void _test_extend(fixture_type_t *type, fixture_type_t *x_type)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *items[4];
    ypObject     *x_items[32];
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

static MunitResult test_isalnum(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isalnum.
    ead(s, type->fromordsCN(N('A', 'a', '1')), assert_obj(yp_isalnum(s), is, yp_True));
    ead(s, type->fromordsCN(N('A', 'a', '1', O_A_GRAVE, O_a_GRAVE, O_SUPER1, O_1OVER4)),
            assert_obj(yp_isalnum(s), is, isbinary(type) ? yp_False : yp_True));

    // Non-alphanumeric.
    ead(s, type->fromordsCN(N(' ', 'a')), assert_obj(yp_isalnum(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', '\t')), assert_obj(yp_isalnum(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', 'a', '!')), assert_obj(yp_isalnum(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', 'a', '\0', '1')), assert_obj(yp_isalnum(s), is, yp_False));

    // Empty s.
    ead(s, type->fromordsCN(0), assert_obj(yp_isalnum(s), is, yp_False));

    // Non-latin-1.
    if (!isbinary(type)) {
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(ords_non_latin_1); i++) {
            ead(s, type->fromordsCN(N(ords_non_latin_1[i])),
                    assert_raises(yp_isalnum(s), yp_SystemLimitationError));
        }
    }

    return MUNIT_OK;
}

static MunitResult test_isalpha(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isalpha.
    ead(s, type->fromordsCN(N('A', 'a')), assert_obj(yp_isalpha(s), is, yp_True));
    ead(s, type->fromordsCN(N('A', 'a', O_A_GRAVE, O_a_GRAVE)),
            assert_obj(yp_isalpha(s), is, isbinary(type) ? yp_False : yp_True));

    // Non-letter.
    ead(s, type->fromordsCN(N(' ', 'a')), assert_obj(yp_isalpha(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', '\t')), assert_obj(yp_isalpha(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', 'a', '!')), assert_obj(yp_isalpha(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', 'a', '\0', 'b')), assert_obj(yp_isalpha(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', 'a')), assert_obj(yp_isalpha(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', O_SUPER1)), assert_obj(yp_isalpha(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', 'a', O_1OVER4)), assert_obj(yp_isalpha(s), is, yp_False));

    // Empty s.
    ead(s, type->fromordsCN(0), assert_obj(yp_isalpha(s), is, yp_False));

    // Non-latin-1.
    if (!isbinary(type)) {
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(ords_non_latin_1); i++) {
            ead(s, type->fromordsCN(N(ords_non_latin_1[i])),
                    assert_raises(yp_isalpha(s), yp_SystemLimitationError));
        }
    }

    return MUNIT_OK;
}

static MunitResult test_isascii(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isascii.
    ead(s, type->fromordsCN(N('A', 'a', '1', ' ', '\t', '!', '\0', 0x7f)),
            assert_obj(yp_isascii(s), is, yp_True));

    // Non-ascii.
    ead(s, type->fromordsCN(N(0x80, 'a')), assert_obj(yp_isascii(s), is, yp_False));
    ead(s, type->fromordsCN(N(O_A_GRAVE, 'a')), assert_obj(yp_isascii(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', O_a_GRAVE)), assert_obj(yp_isascii(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', O_SUPER1)), assert_obj(yp_isascii(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', '\0', O_1OVER4)), assert_obj(yp_isascii(s), is, yp_False));

    // Empty s.
    ead(s, type->fromordsCN(0), assert_obj(yp_isascii(s), is, yp_True));

    // Non-latin-1.
    if (!isbinary(type)) {
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(ords_non_latin_1); i++) {
            ead(s, type->fromordsCN(N(ords_non_latin_1[i])),
                    assert_obj(yp_isascii(s), is, yp_False));
        }
    }

    // Optimization: isascii can check 8 bytes at once.
    {
        yp_ssize_t i;
        for (i = 0; i < 9; i++) {
            ypObject *ascii = type->fromordsCN(N(' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 0x7f, ' ',
                    ' ', ' ', ' ', ' ', ' ', ' ', ' '));
            ypObject *non_ascii = type->fromordsCN(N(' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', 0x80,
                    ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '));
            ead(s, yp_getsliceC4(ascii, i, 9, 1), assert_obj(yp_isascii(s), is, yp_True));
            ead(s, yp_getsliceC4(non_ascii, i, 9, 1), assert_obj(yp_isascii(s), is, yp_False));
            ead(s, yp_getsliceC4(ascii, i, 9 + 8, 1), assert_obj(yp_isascii(s), is, yp_True));
            ead(s, yp_getsliceC4(non_ascii, i, 9 + 8, 1), assert_obj(yp_isascii(s), is, yp_False));
            yp_decrefN(N(non_ascii, ascii));
        }
    }

    return MUNIT_OK;
}

static MunitResult test_isdecimal(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Binary strings don't support isdecimal.
    if (isbinary(type)) {
        ead(s, type->fromordsCN(N('1')), assert_raises(yp_isdecimal(s), yp_MethodError));
        ead(s, type->fromordsCN(N(munit_rand_int_range(0x0, 0xff))),
                assert_raises(yp_isdecimal(s), yp_MethodError));
        goto tear_down;
    }

    // Basic isdecimal.
    ead(s, type->fromordsCN(N('1')), assert_obj(yp_isdecimal(s), is, yp_True));

    // Non-decimal.
    ead(s, type->fromordsCN(N('A', '2')), assert_obj(yp_isdecimal(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', 'a')), assert_obj(yp_isdecimal(s), is, yp_False));
    ead(s, type->fromordsCN(N(O_A_GRAVE, '2')), assert_obj(yp_isdecimal(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', O_a_GRAVE)), assert_obj(yp_isdecimal(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', O_SUPER1, '3')), assert_obj(yp_isdecimal(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', '2', O_1OVER4)), assert_obj(yp_isdecimal(s), is, yp_False));
    ead(s, type->fromordsCN(N(' ', '2')), assert_obj(yp_isdecimal(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', '\t')), assert_obj(yp_isdecimal(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', '2', '!')), assert_obj(yp_isdecimal(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', '2', '\0', '3')), assert_obj(yp_isdecimal(s), is, yp_False));

    // Empty s.
    ead(s, type->fromordsCN(0), assert_obj(yp_isdecimal(s), is, yp_False));

    // Non-latin-1.
    {
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(ords_non_latin_1); i++) {
            ead(s, type->fromordsCN(N(ords_non_latin_1[i])),
                    assert_raises(yp_isdecimal(s), yp_SystemLimitationError));
        }
    }

tear_down:
    return MUNIT_OK;
}

static MunitResult test_isdigit(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isdigit.
    ead(s, type->fromordsCN(N('1')), assert_obj(yp_isdigit(s), is, yp_True));
    ead(s, type->fromordsCN(N('1', O_SUPER1)),
            assert_obj(yp_isdigit(s), is, isbinary(type) ? yp_False : yp_True));

    // Non-digit.
    ead(s, type->fromordsCN(N('A', '2')), assert_obj(yp_isdigit(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', 'a')), assert_obj(yp_isdigit(s), is, yp_False));
    ead(s, type->fromordsCN(N(O_A_GRAVE, '2')), assert_obj(yp_isdigit(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', O_a_GRAVE)), assert_obj(yp_isdigit(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', '2', O_1OVER4)), assert_obj(yp_isdigit(s), is, yp_False));
    ead(s, type->fromordsCN(N(' ', '2')), assert_obj(yp_isdigit(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', '\t')), assert_obj(yp_isdigit(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', '2', '!')), assert_obj(yp_isdigit(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', '2', '\0', '3')), assert_obj(yp_isdigit(s), is, yp_False));

    // Empty s.
    ead(s, type->fromordsCN(0), assert_obj(yp_isdigit(s), is, yp_False));

    // Non-latin-1.
    if (!isbinary(type)) {
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(ords_non_latin_1); i++) {
            ead(s, type->fromordsCN(N(ords_non_latin_1[i])),
                    assert_raises(yp_isdigit(s), yp_SystemLimitationError));
        }
    }

    return MUNIT_OK;
}

static MunitResult test_isidentifier(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Binary strings don't support isidentifier.
    if (isbinary(type)) {
        ead(s, type->fromordsCN(N('A')), assert_raises(yp_isidentifier(s), yp_MethodError));
        ead(s, type->fromordsCN(N(munit_rand_int_range(0x0, 0xff))),
                assert_raises(yp_isidentifier(s), yp_MethodError));
        goto tear_down;
    }

    // Basic isidentifier.
    ead(s, type->fromordsCN(N('A', 'a', '1', '_', O_A_GRAVE, O_a_GRAVE, 0xb7)),
            assert_obj(yp_isidentifier(s), is, yp_True));

    // Unicode identifier rules apply, except underscore is allowed as the first character.
    ead(s, type->fromordsCN(N('_', 'A', 'a', '1', O_A_GRAVE, O_a_GRAVE, 0xb7)),
            assert_obj(yp_isidentifier(s), is, yp_True));

    // Characters that are invalid at the start.
    ead(s, type->fromordsCN(N('1', 'a')), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N(0xb7, 'a')), assert_obj(yp_isidentifier(s), is, yp_False));

    // Characters that are invalid anywhere.
    ead(s, type->fromordsCN(N(' ', 'a')), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', ' ', 'a')), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N('\t', 'a')), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', '\t')), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N('!', 'A', 'a')), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', 'a', '!')), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N('\0', 'A', 'a')), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', 'a', '\0', 'b')), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N(O_SUPER1, 'a')), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', O_SUPER1)), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N(O_1OVER4, 'A', 'a')), assert_obj(yp_isidentifier(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', 'a', O_1OVER4)), assert_obj(yp_isidentifier(s), is, yp_False));

    // Empty s.
    ead(s, type->fromordsCN(0), assert_obj(yp_isidentifier(s), is, yp_False));

    // Non-latin-1.
    {
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(ords_non_latin_1); i++) {
            ead(s, type->fromordsCN(N(ords_non_latin_1[i])),
                    assert_raises(yp_isidentifier(s), yp_SystemLimitationError));
            ead(s, type->fromordsCN(N('A', ords_non_latin_1[i])),
                    assert_raises(yp_isidentifier(s), yp_SystemLimitationError));
        }
    }

tear_down:
    return MUNIT_OK;
}

static MunitResult test_islower(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic islower.
    ead(s, type->fromordsCN(N('a', 'b')), assert_obj(yp_islower(s), is, yp_True));
    ead(s, type->fromordsCN(N('a', O_a_GRAVE)), assert_obj(yp_islower(s), is, yp_True));

    // Non-lowercase cased characters. Non-ascii characters are non-cased in binary strings.
    ead(s, type->fromordsCN(N('A', 'b')), assert_obj(yp_islower(s), is, yp_False));
    ead(s, type->fromordsCN(N('a', O_A_GRAVE)),
            assert_obj(yp_islower(s), is, isbinary(type) ? yp_True : yp_False));

    // Non-cased characters are ignored.
    ead(s, type->fromordsCN(N('a', '1', ' ', '\t', '!', '\0')),
            assert_obj(yp_islower(s), is, yp_True));
    ead(s, type->fromordsCN(N('1', ' ', '\t', '!', '\0', O_a_GRAVE, O_SUPER1, O_1OVER4, 'b')),
            assert_obj(yp_islower(s), is, yp_True));

    // No cased characters. Non-ascii characters are non-cased in binary strings.
    ead(s, type->fromordsCN(0), assert_obj(yp_islower(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', ' ', '\t', '!', '\0')), assert_obj(yp_islower(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', ' ', '\t', '!', '\0', O_SUPER1, O_1OVER4)),
            assert_obj(yp_islower(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', O_a_GRAVE)),
            assert_obj(yp_islower(s), is, isbinary(type) ? yp_False : yp_True));

    // Non-latin-1.
    if (!isbinary(type)) {
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(ords_non_latin_1); i++) {
            ead(s, type->fromordsCN(N(ords_non_latin_1[i])),
                    assert_raises(yp_islower(s), yp_SystemLimitationError));
        }
    }

    return MUNIT_OK;
}

static MunitResult test_isnumeric(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Binary strings don't support isnumeric.
    if (isbinary(type)) {
        ead(s, type->fromordsCN(N('1')), assert_raises(yp_isnumeric(s), yp_MethodError));
        ead(s, type->fromordsCN(N(munit_rand_int_range(0x0, 0xff))),
                assert_raises(yp_isnumeric(s), yp_MethodError));
        goto tear_down;
    }

    // Basic isnumeric.
    ead(s, type->fromordsCN(N('1', O_SUPER1, O_1OVER4)), assert_obj(yp_isnumeric(s), is, yp_True));

    // Non-numeric.
    ead(s, type->fromordsCN(N('A', '2')), assert_obj(yp_isnumeric(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', 'a')), assert_obj(yp_isnumeric(s), is, yp_False));
    ead(s, type->fromordsCN(N(' ', '2')), assert_obj(yp_isnumeric(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', '\t')), assert_obj(yp_isnumeric(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', '2', '!')), assert_obj(yp_isnumeric(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', '2', '\0', '3')), assert_obj(yp_isnumeric(s), is, yp_False));

    // Empty s.
    ead(s, type->fromordsCN(0), assert_obj(yp_isnumeric(s), is, yp_False));

    // Non-latin-1.
    {
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(ords_non_latin_1); i++) {
            ead(s, type->fromordsCN(N(ords_non_latin_1[i])),
                    assert_raises(yp_isnumeric(s), yp_SystemLimitationError));
        }
    }

tear_down:
    return MUNIT_OK;
}

static MunitResult test_isprintable(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Binary strings don't support isprintable.
    if (isbinary(type)) {
        ead(s, type->fromordsCN(N('A')), assert_raises(yp_isprintable(s), yp_MethodError));
        ead(s, type->fromordsCN(N(munit_rand_int_range(0x0, 0xff))),
                assert_raises(yp_isprintable(s), yp_MethodError));
        goto tear_down;
    }

    // Basic isprintable.
    ead(s, type->fromordsCN(N('A', 'a', '1', ' ', '!', O_A_GRAVE, O_a_GRAVE, O_SUPER1, O_1OVER4)),
            assert_obj(yp_isprintable(s), is, yp_True));

    // Non-printable.
    ead(s, type->fromordsCN(N('A', '\t')), assert_obj(yp_isprintable(s), is, yp_False));
    ead(s, type->fromordsCN(N(0x80, 'a')), assert_obj(yp_isprintable(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', 'a', '\0')), assert_obj(yp_isprintable(s), is, yp_False));

    // Empty s.
    ead(s, type->fromordsCN(0), assert_obj(yp_isprintable(s), is, yp_True));

    // Non-latin-1.
    {
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(ords_non_latin_1); i++) {
            ead(s, type->fromordsCN(N(ords_non_latin_1[i])),
                    assert_raises(yp_isprintable(s), yp_SystemLimitationError));
        }
    }

tear_down:
    return MUNIT_OK;
}

static MunitResult test_isspace(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isspace.
    ead(s, type->fromordsCN(N('\t', '\n', 0x0b, 0x0c, '\r', ' ')),
            assert_obj(yp_isspace(s), is, yp_True));
    ead(s,
            type->fromordsCN(
                    N('\t', '\n', 0x0b, 0x0c, '\r', ' ', 0x1c, 0x1d, 0x1e, 0x1f, 0x85, 0xa0)),
            assert_obj(yp_isspace(s), is, isbinary(type) ? yp_False : yp_True));

    // Non-space.
    ead(s, type->fromordsCN(N('A', '\n')), assert_obj(yp_isspace(s), is, yp_False));
    ead(s, type->fromordsCN(N('\t', 'a')), assert_obj(yp_isspace(s), is, yp_False));
    ead(s, type->fromordsCN(N('\t', '1', 0x0b)), assert_obj(yp_isspace(s), is, yp_False));
    ead(s, type->fromordsCN(N(O_A_GRAVE, '\n')), assert_obj(yp_isspace(s), is, yp_False));
    ead(s, type->fromordsCN(N('\t', O_a_GRAVE)), assert_obj(yp_isspace(s), is, yp_False));
    ead(s, type->fromordsCN(N('\t', O_SUPER1, 0x0b)), assert_obj(yp_isspace(s), is, yp_False));
    ead(s, type->fromordsCN(N('\t', '\n', O_1OVER4)), assert_obj(yp_isspace(s), is, yp_False));
    ead(s, type->fromordsCN(N('\t', '\n', '!')), assert_obj(yp_isspace(s), is, yp_False));
    ead(s, type->fromordsCN(N('\t', '\n', '\0', 0x0c)), assert_obj(yp_isspace(s), is, yp_False));

    // Empty s.
    ead(s, type->fromordsCN(0), assert_obj(yp_isspace(s), is, yp_False));

    // Non-latin-1.
    if (!isbinary(type)) {
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(ords_non_latin_1); i++) {
            ead(s, type->fromordsCN(N(ords_non_latin_1[i])),
                    assert_raises(yp_isspace(s), yp_SystemLimitationError));
        }
    }

    return MUNIT_OK;
}

static MunitResult test_isupper(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // Basic isupper.
    ead(s, type->fromordsCN(N('A', 'B')), assert_obj(yp_isupper(s), is, yp_True));
    ead(s, type->fromordsCN(N('A', O_A_GRAVE)), assert_obj(yp_isupper(s), is, yp_True));

    // Non-uppercase cased characters. Non-ascii characters are non-cased in binary strings.
    ead(s, type->fromordsCN(N('a', 'B')), assert_obj(yp_isupper(s), is, yp_False));
    ead(s, type->fromordsCN(N('A', O_a_GRAVE)),
            assert_obj(yp_isupper(s), is, isbinary(type) ? yp_True : yp_False));

    // Non-cased characters are ignored.
    ead(s, type->fromordsCN(N('A', '1', ' ', '\t', '!', '\0')),
            assert_obj(yp_isupper(s), is, yp_True));
    ead(s, type->fromordsCN(N('1', ' ', '\t', '!', '\0', O_A_GRAVE, O_SUPER1, O_1OVER4, 'B')),
            assert_obj(yp_isupper(s), is, yp_True));

    // No cased characters. Non-ascii characters are non-cased in binary strings.
    ead(s, type->fromordsCN(0), assert_obj(yp_isupper(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', ' ', '\t', '!', '\0')), assert_obj(yp_isupper(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', ' ', '\t', '!', '\0', O_SUPER1, O_1OVER4)),
            assert_obj(yp_isupper(s), is, yp_False));
    ead(s, type->fromordsCN(N('1', O_A_GRAVE)),
            assert_obj(yp_isupper(s), is, isbinary(type) ? yp_False : yp_True));

    // Non-latin-1.
    if (!isbinary(type)) {
        yp_ssize_t i;
        for (i = 0; i < yp_lengthof_array(ords_non_latin_1); i++) {
            ead(s, type->fromordsCN(N(ords_non_latin_1[i])),
                    assert_raises(yp_isupper(s), yp_SystemLimitationError));
        }
    }

    return MUNIT_OK;
}

// Tests for the string classifiers for the latin-1 characters. The full Unicode Character Database
// is an optional feature of nohtyP, but the latin-1 characters are always supported, and mostly
// share the same classifications between bytes and str.
static MunitResult test_latin_1_classifiers(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *expected[] = {yp_False, yp_True, isbinary(type) ? yp_False : yp_True};

#define assert_char(ord, alpha, decimal, digit, lower, numeric, printable, space, upper)         \
    do {                                                                                         \
        ypObject *s = type->fromordsCN(1, ord);                                                  \
        ypObject *_s = type->fromordsCN(2, '_', ord);                                            \
        assert_obj(yp_isalnum(s), is, expected[alpha == 0 ? numeric : alpha]);                   \
        assert_obj(yp_isalpha(s), is, expected[alpha]);                                          \
        assert_obj(yp_isascii(s), is, ord < 0x80 ? yp_True : yp_False);                          \
        assert_obj(yp_isdigit(s), is, expected[digit]);                                          \
        assert_obj(yp_islower(s), is, expected[lower]);                                          \
        assert_obj(yp_isspace(s), is, expected[space]);                                          \
        assert_obj(yp_isupper(s), is, expected[upper]);                                          \
        if (!isbinary(type)) {                                                                   \
            assert_obj(yp_isdecimal(s), is, expected[decimal]);                                  \
            assert_obj(yp_isidentifier(s), is, (alpha != 0 || ord == '_') ? yp_True : yp_False); \
            assert_obj(yp_isidentifier(_s), is,                                                  \
                    (alpha != 0 || decimal != 0 || ord == '_' || ord == 0xb7) ? yp_True :        \
                                                                                yp_False);       \
            assert_obj(yp_isnumeric(s), is, expected[numeric]);                                  \
            assert_obj(yp_isprintable(s), is, expected[printable]);                              \
        }                                                                                        \
        yp_decrefN(N(_s, s));                                                                    \
    } while (0)

    /*
    methods = "alpha, decimal, digit, lower, numeric, printable, space, upper"
    def get_expected(x, m):
        if not getattr(x, f"is{m}")(): return "0"
        if m == "space" and ("\x1c" <= x < "\x20"): return "2"
        return "1" if x < "\x7f" else "2"
    for i in range(256):
        x = chr(i)
        expected = [get_expected(x, m) for m in methods.split(", ")]
        if i % 32 == 0: print(f"    // {methods}")
        print(f"    assert_char({i}, {", ".join(expected)});  // {x!r}")
    */

    // alpha, decimal, digit, lower, numeric, printable, space, upper
    assert_char(0, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x00'
    assert_char(1, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x01'
    assert_char(2, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x02'
    assert_char(3, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x03'
    assert_char(4, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x04'
    assert_char(5, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x05'
    assert_char(6, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x06'
    assert_char(7, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x07'
    assert_char(8, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x08'
    assert_char(9, 0, 0, 0, 0, 0, 0, 1, 0);   // '\t'
    assert_char(10, 0, 0, 0, 0, 0, 0, 1, 0);  // '\n'
    assert_char(11, 0, 0, 0, 0, 0, 0, 1, 0);  // '\x0b'
    assert_char(12, 0, 0, 0, 0, 0, 0, 1, 0);  // '\x0c'
    assert_char(13, 0, 0, 0, 0, 0, 0, 1, 0);  // '\r'
    assert_char(14, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x0e'
    assert_char(15, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x0f'
    assert_char(16, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x10'
    assert_char(17, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x11'
    assert_char(18, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x12'
    assert_char(19, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x13'
    assert_char(20, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x14'
    assert_char(21, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x15'
    assert_char(22, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x16'
    assert_char(23, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x17'
    assert_char(24, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x18'
    assert_char(25, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x19'
    assert_char(26, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x1a'
    assert_char(27, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x1b'
    assert_char(28, 0, 0, 0, 0, 0, 0, 2, 0);  // '\x1c'
    assert_char(29, 0, 0, 0, 0, 0, 0, 2, 0);  // '\x1d'
    assert_char(30, 0, 0, 0, 0, 0, 0, 2, 0);  // '\x1e'
    assert_char(31, 0, 0, 0, 0, 0, 0, 2, 0);  // '\x1f'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    assert_char(32, 0, 0, 0, 0, 0, 1, 1, 0);  // ' '
    assert_char(33, 0, 0, 0, 0, 0, 1, 0, 0);  // '!'
    assert_char(34, 0, 0, 0, 0, 0, 1, 0, 0);  // '"'
    assert_char(35, 0, 0, 0, 0, 0, 1, 0, 0);  // '#'
    assert_char(36, 0, 0, 0, 0, 0, 1, 0, 0);  // '$'
    assert_char(37, 0, 0, 0, 0, 0, 1, 0, 0);  // '%'
    assert_char(38, 0, 0, 0, 0, 0, 1, 0, 0);  // '&'
    assert_char(39, 0, 0, 0, 0, 0, 1, 0, 0);  // "'"
    assert_char(40, 0, 0, 0, 0, 0, 1, 0, 0);  // '('
    assert_char(41, 0, 0, 0, 0, 0, 1, 0, 0);  // ')'
    assert_char(42, 0, 0, 0, 0, 0, 1, 0, 0);  // '*'
    assert_char(43, 0, 0, 0, 0, 0, 1, 0, 0);  // '+'
    assert_char(44, 0, 0, 0, 0, 0, 1, 0, 0);  // ','
    assert_char(45, 0, 0, 0, 0, 0, 1, 0, 0);  // '-'
    assert_char(46, 0, 0, 0, 0, 0, 1, 0, 0);  // '.'
    assert_char(47, 0, 0, 0, 0, 0, 1, 0, 0);  // '/'
    assert_char(48, 0, 1, 1, 0, 1, 1, 0, 0);  // '0'
    assert_char(49, 0, 1, 1, 0, 1, 1, 0, 0);  // '1'
    assert_char(50, 0, 1, 1, 0, 1, 1, 0, 0);  // '2'
    assert_char(51, 0, 1, 1, 0, 1, 1, 0, 0);  // '3'
    assert_char(52, 0, 1, 1, 0, 1, 1, 0, 0);  // '4'
    assert_char(53, 0, 1, 1, 0, 1, 1, 0, 0);  // '5'
    assert_char(54, 0, 1, 1, 0, 1, 1, 0, 0);  // '6'
    assert_char(55, 0, 1, 1, 0, 1, 1, 0, 0);  // '7'
    assert_char(56, 0, 1, 1, 0, 1, 1, 0, 0);  // '8'
    assert_char(57, 0, 1, 1, 0, 1, 1, 0, 0);  // '9'
    assert_char(58, 0, 0, 0, 0, 0, 1, 0, 0);  // ':'
    assert_char(59, 0, 0, 0, 0, 0, 1, 0, 0);  // ';'
    assert_char(60, 0, 0, 0, 0, 0, 1, 0, 0);  // '<'
    assert_char(61, 0, 0, 0, 0, 0, 1, 0, 0);  // '='
    assert_char(62, 0, 0, 0, 0, 0, 1, 0, 0);  // '>'
    assert_char(63, 0, 0, 0, 0, 0, 1, 0, 0);  // '?'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    assert_char(64, 0, 0, 0, 0, 0, 1, 0, 0);  // '@'
    assert_char(65, 1, 0, 0, 0, 0, 1, 0, 1);  // 'A'
    assert_char(66, 1, 0, 0, 0, 0, 1, 0, 1);  // 'B'
    assert_char(67, 1, 0, 0, 0, 0, 1, 0, 1);  // 'C'
    assert_char(68, 1, 0, 0, 0, 0, 1, 0, 1);  // 'D'
    assert_char(69, 1, 0, 0, 0, 0, 1, 0, 1);  // 'E'
    assert_char(70, 1, 0, 0, 0, 0, 1, 0, 1);  // 'F'
    assert_char(71, 1, 0, 0, 0, 0, 1, 0, 1);  // 'G'
    assert_char(72, 1, 0, 0, 0, 0, 1, 0, 1);  // 'H'
    assert_char(73, 1, 0, 0, 0, 0, 1, 0, 1);  // 'I'
    assert_char(74, 1, 0, 0, 0, 0, 1, 0, 1);  // 'J'
    assert_char(75, 1, 0, 0, 0, 0, 1, 0, 1);  // 'K'
    assert_char(76, 1, 0, 0, 0, 0, 1, 0, 1);  // 'L'
    assert_char(77, 1, 0, 0, 0, 0, 1, 0, 1);  // 'M'
    assert_char(78, 1, 0, 0, 0, 0, 1, 0, 1);  // 'N'
    assert_char(79, 1, 0, 0, 0, 0, 1, 0, 1);  // 'O'
    assert_char(80, 1, 0, 0, 0, 0, 1, 0, 1);  // 'P'
    assert_char(81, 1, 0, 0, 0, 0, 1, 0, 1);  // 'Q'
    assert_char(82, 1, 0, 0, 0, 0, 1, 0, 1);  // 'R'
    assert_char(83, 1, 0, 0, 0, 0, 1, 0, 1);  // 'S'
    assert_char(84, 1, 0, 0, 0, 0, 1, 0, 1);  // 'T'
    assert_char(85, 1, 0, 0, 0, 0, 1, 0, 1);  // 'U'
    assert_char(86, 1, 0, 0, 0, 0, 1, 0, 1);  // 'V'
    assert_char(87, 1, 0, 0, 0, 0, 1, 0, 1);  // 'W'
    assert_char(88, 1, 0, 0, 0, 0, 1, 0, 1);  // 'X'
    assert_char(89, 1, 0, 0, 0, 0, 1, 0, 1);  // 'Y'
    assert_char(90, 1, 0, 0, 0, 0, 1, 0, 1);  // 'Z'
    assert_char(91, 0, 0, 0, 0, 0, 1, 0, 0);  // '['
    assert_char(92, 0, 0, 0, 0, 0, 1, 0, 0);  // '\\'
    assert_char(93, 0, 0, 0, 0, 0, 1, 0, 0);  // ']'
    assert_char(94, 0, 0, 0, 0, 0, 1, 0, 0);  // '^'
    assert_char(95, 0, 0, 0, 0, 0, 1, 0, 0);  // '_'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    assert_char(96, 0, 0, 0, 0, 0, 1, 0, 0);   // '`'
    assert_char(97, 1, 0, 0, 1, 0, 1, 0, 0);   // 'a'
    assert_char(98, 1, 0, 0, 1, 0, 1, 0, 0);   // 'b'
    assert_char(99, 1, 0, 0, 1, 0, 1, 0, 0);   // 'c'
    assert_char(100, 1, 0, 0, 1, 0, 1, 0, 0);  // 'd'
    assert_char(101, 1, 0, 0, 1, 0, 1, 0, 0);  // 'e'
    assert_char(102, 1, 0, 0, 1, 0, 1, 0, 0);  // 'f'
    assert_char(103, 1, 0, 0, 1, 0, 1, 0, 0);  // 'g'
    assert_char(104, 1, 0, 0, 1, 0, 1, 0, 0);  // 'h'
    assert_char(105, 1, 0, 0, 1, 0, 1, 0, 0);  // 'i'
    assert_char(106, 1, 0, 0, 1, 0, 1, 0, 0);  // 'j'
    assert_char(107, 1, 0, 0, 1, 0, 1, 0, 0);  // 'k'
    assert_char(108, 1, 0, 0, 1, 0, 1, 0, 0);  // 'l'
    assert_char(109, 1, 0, 0, 1, 0, 1, 0, 0);  // 'm'
    assert_char(110, 1, 0, 0, 1, 0, 1, 0, 0);  // 'n'
    assert_char(111, 1, 0, 0, 1, 0, 1, 0, 0);  // 'o'
    assert_char(112, 1, 0, 0, 1, 0, 1, 0, 0);  // 'p'
    assert_char(113, 1, 0, 0, 1, 0, 1, 0, 0);  // 'q'
    assert_char(114, 1, 0, 0, 1, 0, 1, 0, 0);  // 'r'
    assert_char(115, 1, 0, 0, 1, 0, 1, 0, 0);  // 's'
    assert_char(116, 1, 0, 0, 1, 0, 1, 0, 0);  // 't'
    assert_char(117, 1, 0, 0, 1, 0, 1, 0, 0);  // 'u'
    assert_char(118, 1, 0, 0, 1, 0, 1, 0, 0);  // 'v'
    assert_char(119, 1, 0, 0, 1, 0, 1, 0, 0);  // 'w'
    assert_char(120, 1, 0, 0, 1, 0, 1, 0, 0);  // 'x'
    assert_char(121, 1, 0, 0, 1, 0, 1, 0, 0);  // 'y'
    assert_char(122, 1, 0, 0, 1, 0, 1, 0, 0);  // 'z'
    assert_char(123, 0, 0, 0, 0, 0, 1, 0, 0);  // '{'
    assert_char(124, 0, 0, 0, 0, 0, 1, 0, 0);  // '|'
    assert_char(125, 0, 0, 0, 0, 0, 1, 0, 0);  // '}'
    assert_char(126, 0, 0, 0, 0, 0, 1, 0, 0);  // '~'
    assert_char(127, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x7f'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    assert_char(128, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x80'
    assert_char(129, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x81'
    assert_char(130, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x82'
    assert_char(131, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x83'
    assert_char(132, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x84'
    assert_char(133, 0, 0, 0, 0, 0, 0, 2, 0);  // '\x85'
    assert_char(134, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x86'
    assert_char(135, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x87'
    assert_char(136, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x88'
    assert_char(137, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x89'
    assert_char(138, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8a'
    assert_char(139, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8b'
    assert_char(140, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8c'
    assert_char(141, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8d'
    assert_char(142, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8e'
    assert_char(143, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8f'
    assert_char(144, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x90'
    assert_char(145, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x91'
    assert_char(146, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x92'
    assert_char(147, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x93'
    assert_char(148, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x94'
    assert_char(149, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x95'
    assert_char(150, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x96'
    assert_char(151, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x97'
    assert_char(152, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x98'
    assert_char(153, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x99'
    assert_char(154, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9a'
    assert_char(155, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9b'
    assert_char(156, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9c'
    assert_char(157, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9d'
    assert_char(158, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9e'
    assert_char(159, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9f'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    assert_char(160, 0, 0, 0, 0, 0, 0, 2, 0);  // '\xa0'
    assert_char(161, 0, 0, 0, 0, 0, 2, 0, 0);  // '¡'
    assert_char(162, 0, 0, 0, 0, 0, 2, 0, 0);  // '¢'
    assert_char(163, 0, 0, 0, 0, 0, 2, 0, 0);  // '£'
    assert_char(164, 0, 0, 0, 0, 0, 2, 0, 0);  // '¤'
    assert_char(165, 0, 0, 0, 0, 0, 2, 0, 0);  // '¥'
    assert_char(166, 0, 0, 0, 0, 0, 2, 0, 0);  // '¦'
    assert_char(167, 0, 0, 0, 0, 0, 2, 0, 0);  // '§'
    assert_char(168, 0, 0, 0, 0, 0, 2, 0, 0);  // '¨'
    assert_char(169, 0, 0, 0, 0, 0, 2, 0, 0);  // '©'
    assert_char(170, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ª'
    assert_char(171, 0, 0, 0, 0, 0, 2, 0, 0);  // '«'
    assert_char(172, 0, 0, 0, 0, 0, 2, 0, 0);  // '¬'
    assert_char(173, 0, 0, 0, 0, 0, 0, 0, 0);  // '\xad'
    assert_char(174, 0, 0, 0, 0, 0, 2, 0, 0);  // '®'
    assert_char(175, 0, 0, 0, 0, 0, 2, 0, 0);  // '¯'
    assert_char(176, 0, 0, 0, 0, 0, 2, 0, 0);  // '°'
    assert_char(177, 0, 0, 0, 0, 0, 2, 0, 0);  // '±'
    assert_char(178, 0, 0, 2, 0, 2, 2, 0, 0);  // '²'
    assert_char(179, 0, 0, 2, 0, 2, 2, 0, 0);  // '³'
    assert_char(180, 0, 0, 0, 0, 0, 2, 0, 0);  // '´'
    assert_char(181, 2, 0, 0, 2, 0, 2, 0, 0);  // 'µ'
    assert_char(182, 0, 0, 0, 0, 0, 2, 0, 0);  // '¶'
    assert_char(183, 0, 0, 0, 0, 0, 2, 0, 0);  // '·'
    assert_char(184, 0, 0, 0, 0, 0, 2, 0, 0);  // '¸'
    assert_char(185, 0, 0, 2, 0, 2, 2, 0, 0);  // '¹'
    assert_char(186, 2, 0, 0, 2, 0, 2, 0, 0);  // 'º'
    assert_char(187, 0, 0, 0, 0, 0, 2, 0, 0);  // '»'
    assert_char(188, 0, 0, 0, 0, 2, 2, 0, 0);  // '¼'
    assert_char(189, 0, 0, 0, 0, 2, 2, 0, 0);  // '½'
    assert_char(190, 0, 0, 0, 0, 2, 2, 0, 0);  // '¾'
    assert_char(191, 0, 0, 0, 0, 0, 2, 0, 0);  // '¿'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    assert_char(192, 2, 0, 0, 0, 0, 2, 0, 2);  // 'À'
    assert_char(193, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Á'
    assert_char(194, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Â'
    assert_char(195, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ã'
    assert_char(196, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ä'
    assert_char(197, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Å'
    assert_char(198, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Æ'
    assert_char(199, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ç'
    assert_char(200, 2, 0, 0, 0, 0, 2, 0, 2);  // 'È'
    assert_char(201, 2, 0, 0, 0, 0, 2, 0, 2);  // 'É'
    assert_char(202, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ê'
    assert_char(203, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ë'
    assert_char(204, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ì'
    assert_char(205, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Í'
    assert_char(206, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Î'
    assert_char(207, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ï'
    assert_char(208, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ð'
    assert_char(209, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ñ'
    assert_char(210, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ò'
    assert_char(211, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ó'
    assert_char(212, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ô'
    assert_char(213, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Õ'
    assert_char(214, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ö'
    assert_char(215, 0, 0, 0, 0, 0, 2, 0, 0);  // '×'
    assert_char(216, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ø'
    assert_char(217, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ù'
    assert_char(218, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ú'
    assert_char(219, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Û'
    assert_char(220, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ü'
    assert_char(221, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ý'
    assert_char(222, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Þ'
    assert_char(223, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ß'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    assert_char(224, 2, 0, 0, 2, 0, 2, 0, 0);  // 'à'
    assert_char(225, 2, 0, 0, 2, 0, 2, 0, 0);  // 'á'
    assert_char(226, 2, 0, 0, 2, 0, 2, 0, 0);  // 'â'
    assert_char(227, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ã'
    assert_char(228, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ä'
    assert_char(229, 2, 0, 0, 2, 0, 2, 0, 0);  // 'å'
    assert_char(230, 2, 0, 0, 2, 0, 2, 0, 0);  // 'æ'
    assert_char(231, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ç'
    assert_char(232, 2, 0, 0, 2, 0, 2, 0, 0);  // 'è'
    assert_char(233, 2, 0, 0, 2, 0, 2, 0, 0);  // 'é'
    assert_char(234, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ê'
    assert_char(235, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ë'
    assert_char(236, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ì'
    assert_char(237, 2, 0, 0, 2, 0, 2, 0, 0);  // 'í'
    assert_char(238, 2, 0, 0, 2, 0, 2, 0, 0);  // 'î'
    assert_char(239, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ï'
    assert_char(240, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ð'
    assert_char(241, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ñ'
    assert_char(242, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ò'
    assert_char(243, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ó'
    assert_char(244, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ô'
    assert_char(245, 2, 0, 0, 2, 0, 2, 0, 0);  // 'õ'
    assert_char(246, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ö'
    assert_char(247, 0, 0, 0, 0, 0, 2, 0, 0);  // '÷'
    assert_char(248, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ø'
    assert_char(249, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ù'
    assert_char(250, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ú'
    assert_char(251, 2, 0, 0, 2, 0, 2, 0, 0);  // 'û'
    assert_char(252, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ü'
    assert_char(253, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ý'
    assert_char(254, 2, 0, 0, 2, 0, 2, 0, 0);  // 'þ'
    assert_char(255, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ÿ'

#undef assert_char
    return MUNIT_OK;
}

static void _test_startswith(fixture_type_t *type, fixture_type_t *x_type)
{
    uniqueness_t *uq = uniqueness_new();
    ypObject     *not_iterable = rand_obj_any_not_iterable(uq);
    ypObject     *items[6];
    ypObject     *s;
    ypObject     *empty = type->newN(0);
    ypObject     *x_empty = x_type->newN(0);
    ypObject     *x_0;
    ypObject     *x_1;
    ypObject     *x_1_2;
    ypObject     *x_1_4;
    ypObject     *x_2;
    ypObject     *x_2_1;
    ypObject     *x_2_3;
    ypObject     *x_3;
    obj_array_fill(items, uq, type->rand_elems->items);  // FIXME Update this test.
    s = type->newN(N(items[1], items[2], items[3]));
    x_0 = x_type->newN(N(items[0]));
    x_1 = x_type->newN(N(items[1]));
    x_1_2 = x_type->newN(N(items[1], items[2]));
    x_1_4 = x_type->newN(N(items[0], items[4]));
    x_2 = x_type->newN(N(items[2]));
    x_2_1 = x_type->newN(N(items[2], items[1]));
    x_2_3 = x_type->newN(N(items[2], items[3]));
    x_3 = x_type->newN(N(items[3]));

    // FIXME Differing str encodings.
    // FIXME Substring is duplicated in string? Maybe x_1_1?

    // Basic startswith.
    assert_obj(yp_startswith(s, x_1), is, yp_True);
    assert_obj(yp_startswith(s, x_1_2), is, yp_True);

    // Characters not in string.
    assert_obj(yp_startswith(s, x_0), is, yp_False);
    assert_obj(yp_startswith(s, x_1_4), is, yp_False);

    // Characters not at start.
    assert_obj(yp_startswith(s, x_2), is, yp_False);
    assert_obj(yp_startswith(s, x_2_3), is, yp_False);

    // Characters out-of-order.
    assert_obj(yp_startswith(s, x_2_1), is, yp_False);

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
    assert_obj(yp_startswithC4(s, x_1_2, -3, -1), is, yp_True);
    assert_obj(yp_startswithC4(s, x_1_2, -1, 3), is, yp_False);
    assert_obj(yp_startswithC4(s, x_3, -3, -1), is, yp_False);
    assert_obj(yp_startswithC4(s, x_3, -1, 3), is, yp_True);

    // Total slice.
    assert_obj(yp_startswithC4(s, x_0, 0, 3), is, yp_False);
    assert_obj(yp_startswithC4(s, x_1, 0, 3), is, yp_True);
    assert_obj(yp_startswithC4(s, x_1_2, 0, 3), is, yp_True);

    // Total slice, negative indicies.
    assert_obj(yp_startswithC4(s, x_0, -3, 3), is, yp_False);
    assert_obj(yp_startswithC4(s, x_1, -3, 3), is, yp_True);
    assert_obj(yp_startswithC4(s, x_1_2, -3, 3), is, yp_True);

    // Empty slices.
    {
        slice_args_t slices[] = {
                // recall step is always 1 for startswith
                {0, 0, 1},     // typical empty slice
                {3, 99, 1},    // i>=len(s) (regardless of j)
                {-99, -4, 1},  // j<-len(s) (regardless of i)
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

    // x is s.
    assert_obj(yp_startswith(s, s), is, yp_True);
    assert_obj(yp_startswithC4(s, s, 0, 1), is, yp_False);
    assert_obj(yp_startswithC4(s, s, 0, 3), is, yp_True);

    // x is a tuple of strings.
    ead(x_tuple, yp_tupleN(N(x_0, x_1)), assert_obj(yp_startswith(s, x_tuple), is, yp_True));
    ead(x_tuple, yp_tupleN(N(x_0, x_1_4)), assert_obj(yp_startswith(s, x_tuple), is, yp_False));
    ead(x_tuple, yp_tupleN(N(x_1)), assert_obj(yp_startswithC4(s, x_tuple, 0, 3), is, yp_True));
    ead(x_tuple, yp_tupleN(N(x_1)), assert_obj(yp_startswithC4(s, x_tuple, 1, 3), is, yp_False));

    // x is an empty tuple.
    ead(x_tuple, yp_tupleN(0), assert_obj(yp_startswith(s, x_tuple), is, yp_False));
    ead(x_tuple, yp_tupleN(0), assert_obj(yp_startswith(empty, x_tuple), is, yp_False));
    ead(x_tuple, yp_tupleN(0), assert_obj(yp_startswithC4(s, x_tuple, 0, 0), is, yp_False));

    // Binary and text types cannot be mixed. FIXME Parameterize?
    if (isbinary(type)) {
        ead(x, rand_obj(uq, fixture_type_str), assert_raises(yp_startswith(s, x), yp_TypeError));
        ead(x, rand_obj(uq, fixture_type_chrarray),
                assert_raises(yp_startswith(s, x), yp_TypeError));
    } else {
        ead(x, rand_obj(uq, fixture_type_bytes), assert_raises(yp_startswith(s, x), yp_TypeError));
        ead(x, rand_obj(uq, fixture_type_bytearray),
                assert_raises(yp_startswith(s, x), yp_TypeError));
    }

    // x is an item. Supported on text as their items are strings.
    if (isbinary(type)) {
        assert_raises(yp_startswith(s, items[1]), yp_TypeError);
    } else {
        assert_obj(yp_startswith(s, items[1]), is, yp_True);
    }

    // x is a list, which is not supported. TODO Should it be?
    ead(x, yp_listN(N(x_1)), assert_raises(yp_startswith(s, x), yp_TypeError));

    // x is not an iterable.
    assert_raises(yp_startswith(s, not_iterable), yp_TypeError);

    // Optimization: early exit if x is a tuple with a match, even if x contains bad types.
    // FIXME This is how Python behaves, but it can hide errors. Should we check remaining items?
    // FIXME yp_isdisjoint has early exit; is there a counterexample? We should standardize.
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

    assert_sequence(s, items[1], items[2], items[3]);  // s unchanged.

    obj_array_decref(items);
    uniqueness_dealloc(uq);
    yp_decrefN(N(not_iterable, s, empty, x_empty, x_0, x_1, x_1_2, x_1_4, x_2, x_2_1, x_2_3, x_3));
}

static MunitResult test_startswith(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    peer_type_t    *peer;

    for (peer = type->peers; peer->type != NULL; peer++) {
        // FIXME Test that an error is raised on bad type.
        if (!peer->type->is_string) continue;  // Skip peers that are not strings.
        _test_startswith(type, peer->type);
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
        TEST(test_latin_1_classifiers, test_string_params),
        TEST(test_startswith, test_string_params), TEST(test_endswith, test_string_params), {NULL}};


extern void test_string_initialize(void) {}
