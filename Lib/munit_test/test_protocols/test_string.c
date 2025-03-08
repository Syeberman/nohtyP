
#include "munit_test/unittest.h"

// test_latin_1_classifiers is a bit too complex for GCC.
#if defined(__GNUC__)
#pragma GCC optimize("no-var-tracking")
#endif

// TODO Ensure yp_startswithC4/yp_endswithC4/yp_replaceC4/yp_lstrip2/yp_splitlines2/yp_encode3/etc
// properly handles exception passthrough, even in cases where one of the arguments would be ignored
// (e.g. empty str, empty slice).
// TODO This (exception passthrough) even includes yp_formatN/etc where the argument is never
// referenced in the format string.

// FIXME Strings are either "binary" or "text".
static int isbinary(fixture_type_t *type)
{
    return type == fixture_type_bytes || type == fixture_type_bytearray;
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
    ypObject     *string;
    ypObject     *other_0_1;
    ypObject     *other_1_2;
    ypObject     *other_0_2;
    ypObject     *other_1_0;
    ypObject     *empty = type->newN(0);

    obj_array_fill(items, uq, type->rand_items);
    string = type->newN(N(items[0], items[1], items[2]));
    // TODO Test against different "other" types (the other pair, really)
    other_0_1 = type->newN(N(items[0], items[1]));
    other_1_2 = type->newN(N(items[1], items[2]));
    other_0_2 = type->newN(N(items[0], items[2]));
    other_1_0 = type->newN(N(items[1], items[0]));

#define assert_not_found_exc(expression)                    \
    do {                                                    \
        ypObject *exc = yp_None;                            \
        assert_ssizeC(expression, ==, -1);                  \
        if (raises) assert_isexception(exc, yp_ValueError); \
    } while (0)

    assert_ssizeC_exc(any_findC(string, other_0_1, &exc), ==, 0);            // Sub-string.
    assert_ssizeC_exc(any_findC(string, other_1_2, &exc), ==, 1);            // Sub-string.
    assert_not_found_exc(any_findC(string, other_0_2, &exc));                // Out-of-order.
    assert_not_found_exc(any_findC(string, other_1_0, &exc));                // Out-of-order.
    assert_ssizeC_exc(any_findC(string, empty, &exc), ==, forward ? 0 : 3);  // Empty.
    assert_ssizeC_exc(any_findC(string, string, &exc), ==, 0);               // Self.

    assert_ssizeC_exc(any_findC5(string, other_0_1, 0, 3, &exc), ==, 0);  // Total slice.
    assert_ssizeC_exc(any_findC5(string, other_0_1, 0, 2, &exc), ==, 0);  // Exact slice.
    assert_not_found_exc(any_findC5(string, other_0_1, 0, 1, &exc));      // Too-small slice.
    assert_not_found_exc(any_findC5(string, other_0_1, 0, 0, &exc));      // Empty slice.

    assert_ssizeC_exc(any_findC5(string, other_1_2, 1, 3, &exc), ==, 1);  // Exact slice.
    assert_not_found_exc(any_findC5(string, other_1_2, 1, 2, &exc));      // Too-small slice.
    assert_not_found_exc(any_findC5(string, other_1_2, 1, 1, &exc));      // Empty slice.

    assert_ssizeC_exc(any_findC5(string, empty, 0, 3, &exc), ==, forward ? 0 : 3);  // Empty, total.
    assert_ssizeC_exc(
            any_findC5(string, empty, 1, 2, &exc), ==, forward ? 1 : 2);  // Empty, partial.
    assert_ssizeC_exc(any_findC5(string, empty, 2, 2, &exc), ==, 2);      // Empty, empty.

    assert_ssizeC_exc(any_findC5(string, string, 0, 3, &exc), ==, 0);  // Self, exact.
    assert_not_found_exc(any_findC5(string, string, 1, 2, &exc));      // Self, too-small.
    assert_not_found_exc(any_findC5(string, string, 1, 1, &exc));      // Self, empty.

    // TODO That empty slice bug thing.
    // TODO !forward substrings?
    // TODO Anything else to add here?

#undef assert_not_found_exc

    obj_array_decref(items);
    yp_decrefN(N(string, empty, other_0_1, other_1_2, other_0_2, other_1_0));
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

// TODO test_countC, for non-overlapping substrings.

// TODO test_remove and test_discard, for substrings.

// Tests for the string classifiers for the latin-1 characters. The full Unicode Character Database
// is an optional feature of nohtyP, but the latin-1 characters are always supported, and mostly
// share the same classifications between bytes and str.
static MunitResult test_latin_1_classifiers(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;
    ypObject       *expected[] = {yp_False, yp_True, isbinary(type) ? yp_False : yp_True};

    // FIXME Not on bytes: "isdecimal", "isidentifier", "isnumeric", "isprintable"

#define assert_char(ord, alpha, decimal, digit, lower, numeric, printable, space, upper) \
    do {                                                                                 \
        ypObject *x = type->fromordsCN(1, ord);                                          \
        assert_obj(yp_isalnum(x), is, expected[alpha == 0 ? numeric : alpha]);           \
        assert_obj(yp_isalpha(x), is, expected[alpha]);                                  \
        assert_obj(yp_isascii(x), is, expected[ord < 128 ? 1 : 0]);                      \
        assert_obj(yp_isdigit(x), is, expected[digit]);                                  \
        assert_obj(yp_islower(x), is, expected[lower]);                                  \
        assert_obj(yp_isspace(x), is, expected[space]);                                  \
        assert_obj(yp_isupper(x), is, expected[upper]);                                  \
        /* FIXME if (!isbinary(type)) {                                                  \
            assert_obj(yp_isdecimal(x), is, expected[decimal]);                          \
            assert_obj(yp_isidentifier(x), is, expected[alpha || ord == 95]);            \
            assert_obj(yp_isnumeric(x), is, expected[numeric]);                          \
            assert_obj(yp_isprintable(x), is, expected[printable]);                      \
        } */                                                                             \
        yp_decref(x);                                                                    \
    } while (0)

    /*
    methods = "alpha, decimal, digit, lower, numeric, printable, space, upper"
    def getexpected(x, m):
        if not getattr(x, f"is{m}")(): return "0"
        if m == "space" and ("\x1c" <= x < "\x20"): return "2"
        return "1" if x < "\x7f" else "2"
    for i in range(256):
        x = chr(i)
        expected = [getexpected(x, m) for m in methods.split(", ")]
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


static MunitParameterEnum test_string_params[] = {
        {param_key_type, param_values_types_string}, {NULL}};

MunitTest test_string_tests[] = {TEST(test_findC, test_string_params),
        TEST(test_indexC, test_string_params), TEST(test_rfindC, test_string_params),
        TEST(test_rindexC, test_string_params), TEST(test_latin_1_classifiers, test_string_params),
        {NULL}};


extern void test_string_initialize(void) {}
