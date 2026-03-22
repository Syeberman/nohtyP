// Tests for the string classifier and converter methods (yp_isalnum, yp_lower, et al.). These rely
// on the properties of specific characters, so are run on the non-variant string types (i.e.
// fixture_type_str, but not fixture_type_str_1byte).

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

// FIXME Strings are either "binary" or "text".
static int isbinary(fixture_type_t *type)
{
    return type == fixture_type_bytes || type == fixture_type_bytearray;
}


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

// Used by test_latin_1_classifiers, called on each latin-1 character. For the arguments after ord,
// 0 means to expect false, 1 means to expect true, and 2 means to expect false for binary and true
// otherwise.
static void char_class_test(fixture_type_t *type, int ord, int alpha, int decimal, int digit,
        int lower, int numeric, int printable, int space, int upper)
{
    int       int2bool[] = {FALSE, TRUE, isbinary(type) ? FALSE : TRUE};
    ypObject *s = type->fromordsCN(1, ord);
    ypObject *_s = type->fromordsCN(2, '_', ord);

#define assert_class(expression, expected)                                                     \
    do {                                                                                       \
        ypObject *_ypmt_CLASS_expression = (expression);                                       \
        int       _ypmt_CLASS_expected = (expected);                                           \
        _assert_bool(_ypmt_CLASS_expression, (_ypmt_CLASS_expected ? yp_True : yp_False),      \
                "%s /*ord %d*/", (_ypmt_CLASS_expected ? "yp_True" : "yp_False"), #expression, \
                ord);                                                                          \
    } while (0)

    assert_class(yp_isalnum(s), int2bool[alpha == 0 ? numeric : alpha]);
    assert_class(yp_isalpha(s), int2bool[alpha]);
    assert_class(yp_isascii(s), ord < 0x80);
    assert_class(yp_isdigit(s), int2bool[digit]);
    assert_class(yp_islower(s), int2bool[lower]);
    assert_class(yp_isspace(s), int2bool[space]);
    assert_class(yp_isupper(s), int2bool[upper]);
    if (!isbinary(type)) {
        assert_class(yp_isdecimal(s), int2bool[decimal]);
        assert_class(yp_isidentifier(s), (alpha != 0 || ord == '_'));
        assert_class(
                yp_isidentifier(_s), (alpha != 0 || decimal != 0 || ord == '_' || ord == 0xb7));
        assert_class(yp_isnumeric(s), int2bool[numeric]);
        assert_class(yp_isprintable(s), int2bool[printable]);
    }

#undef assert_class
    yp_decrefN(N(_s, s));
}

// Tests for the string classifiers for the latin-1 characters. The full Unicode Character Database
// is an optional feature of nohtyP, but the latin-1 characters are always supported, and mostly
// share the same classifications between bytes and str.
static MunitResult test_latin_1_classifiers(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

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
        print(f"    char_class_test(type, {i}, {", ".join(expected)});  // {x!r}")
    */

    // alpha, decimal, digit, lower, numeric, printable, space, upper
    char_class_test(type, 0, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x00'
    char_class_test(type, 1, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x01'
    char_class_test(type, 2, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x02'
    char_class_test(type, 3, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x03'
    char_class_test(type, 4, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x04'
    char_class_test(type, 5, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x05'
    char_class_test(type, 6, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x06'
    char_class_test(type, 7, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x07'
    char_class_test(type, 8, 0, 0, 0, 0, 0, 0, 0, 0);   // '\x08'
    char_class_test(type, 9, 0, 0, 0, 0, 0, 0, 1, 0);   // '\t'
    char_class_test(type, 10, 0, 0, 0, 0, 0, 0, 1, 0);  // '\n'
    char_class_test(type, 11, 0, 0, 0, 0, 0, 0, 1, 0);  // '\x0b'
    char_class_test(type, 12, 0, 0, 0, 0, 0, 0, 1, 0);  // '\x0c'
    char_class_test(type, 13, 0, 0, 0, 0, 0, 0, 1, 0);  // '\r'
    char_class_test(type, 14, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x0e'
    char_class_test(type, 15, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x0f'
    char_class_test(type, 16, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x10'
    char_class_test(type, 17, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x11'
    char_class_test(type, 18, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x12'
    char_class_test(type, 19, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x13'
    char_class_test(type, 20, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x14'
    char_class_test(type, 21, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x15'
    char_class_test(type, 22, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x16'
    char_class_test(type, 23, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x17'
    char_class_test(type, 24, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x18'
    char_class_test(type, 25, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x19'
    char_class_test(type, 26, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x1a'
    char_class_test(type, 27, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x1b'
    char_class_test(type, 28, 0, 0, 0, 0, 0, 0, 2, 0);  // '\x1c'
    char_class_test(type, 29, 0, 0, 0, 0, 0, 0, 2, 0);  // '\x1d'
    char_class_test(type, 30, 0, 0, 0, 0, 0, 0, 2, 0);  // '\x1e'
    char_class_test(type, 31, 0, 0, 0, 0, 0, 0, 2, 0);  // '\x1f'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    char_class_test(type, 32, 0, 0, 0, 0, 0, 1, 1, 0);  // ' '
    char_class_test(type, 33, 0, 0, 0, 0, 0, 1, 0, 0);  // '!'
    char_class_test(type, 34, 0, 0, 0, 0, 0, 1, 0, 0);  // '"'
    char_class_test(type, 35, 0, 0, 0, 0, 0, 1, 0, 0);  // '#'
    char_class_test(type, 36, 0, 0, 0, 0, 0, 1, 0, 0);  // '$'
    char_class_test(type, 37, 0, 0, 0, 0, 0, 1, 0, 0);  // '%'
    char_class_test(type, 38, 0, 0, 0, 0, 0, 1, 0, 0);  // '&'
    char_class_test(type, 39, 0, 0, 0, 0, 0, 1, 0, 0);  // "'"
    char_class_test(type, 40, 0, 0, 0, 0, 0, 1, 0, 0);  // '('
    char_class_test(type, 41, 0, 0, 0, 0, 0, 1, 0, 0);  // ')'
    char_class_test(type, 42, 0, 0, 0, 0, 0, 1, 0, 0);  // '*'
    char_class_test(type, 43, 0, 0, 0, 0, 0, 1, 0, 0);  // '+'
    char_class_test(type, 44, 0, 0, 0, 0, 0, 1, 0, 0);  // ','
    char_class_test(type, 45, 0, 0, 0, 0, 0, 1, 0, 0);  // '-'
    char_class_test(type, 46, 0, 0, 0, 0, 0, 1, 0, 0);  // '.'
    char_class_test(type, 47, 0, 0, 0, 0, 0, 1, 0, 0);  // '/'
    char_class_test(type, 48, 0, 1, 1, 0, 1, 1, 0, 0);  // '0'
    char_class_test(type, 49, 0, 1, 1, 0, 1, 1, 0, 0);  // '1'
    char_class_test(type, 50, 0, 1, 1, 0, 1, 1, 0, 0);  // '2'
    char_class_test(type, 51, 0, 1, 1, 0, 1, 1, 0, 0);  // '3'
    char_class_test(type, 52, 0, 1, 1, 0, 1, 1, 0, 0);  // '4'
    char_class_test(type, 53, 0, 1, 1, 0, 1, 1, 0, 0);  // '5'
    char_class_test(type, 54, 0, 1, 1, 0, 1, 1, 0, 0);  // '6'
    char_class_test(type, 55, 0, 1, 1, 0, 1, 1, 0, 0);  // '7'
    char_class_test(type, 56, 0, 1, 1, 0, 1, 1, 0, 0);  // '8'
    char_class_test(type, 57, 0, 1, 1, 0, 1, 1, 0, 0);  // '9'
    char_class_test(type, 58, 0, 0, 0, 0, 0, 1, 0, 0);  // ':'
    char_class_test(type, 59, 0, 0, 0, 0, 0, 1, 0, 0);  // ';'
    char_class_test(type, 60, 0, 0, 0, 0, 0, 1, 0, 0);  // '<'
    char_class_test(type, 61, 0, 0, 0, 0, 0, 1, 0, 0);  // '='
    char_class_test(type, 62, 0, 0, 0, 0, 0, 1, 0, 0);  // '>'
    char_class_test(type, 63, 0, 0, 0, 0, 0, 1, 0, 0);  // '?'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    char_class_test(type, 64, 0, 0, 0, 0, 0, 1, 0, 0);  // '@'
    char_class_test(type, 65, 1, 0, 0, 0, 0, 1, 0, 1);  // 'A'
    char_class_test(type, 66, 1, 0, 0, 0, 0, 1, 0, 1);  // 'B'
    char_class_test(type, 67, 1, 0, 0, 0, 0, 1, 0, 1);  // 'C'
    char_class_test(type, 68, 1, 0, 0, 0, 0, 1, 0, 1);  // 'D'
    char_class_test(type, 69, 1, 0, 0, 0, 0, 1, 0, 1);  // 'E'
    char_class_test(type, 70, 1, 0, 0, 0, 0, 1, 0, 1);  // 'F'
    char_class_test(type, 71, 1, 0, 0, 0, 0, 1, 0, 1);  // 'G'
    char_class_test(type, 72, 1, 0, 0, 0, 0, 1, 0, 1);  // 'H'
    char_class_test(type, 73, 1, 0, 0, 0, 0, 1, 0, 1);  // 'I'
    char_class_test(type, 74, 1, 0, 0, 0, 0, 1, 0, 1);  // 'J'
    char_class_test(type, 75, 1, 0, 0, 0, 0, 1, 0, 1);  // 'K'
    char_class_test(type, 76, 1, 0, 0, 0, 0, 1, 0, 1);  // 'L'
    char_class_test(type, 77, 1, 0, 0, 0, 0, 1, 0, 1);  // 'M'
    char_class_test(type, 78, 1, 0, 0, 0, 0, 1, 0, 1);  // 'N'
    char_class_test(type, 79, 1, 0, 0, 0, 0, 1, 0, 1);  // 'O'
    char_class_test(type, 80, 1, 0, 0, 0, 0, 1, 0, 1);  // 'P'
    char_class_test(type, 81, 1, 0, 0, 0, 0, 1, 0, 1);  // 'Q'
    char_class_test(type, 82, 1, 0, 0, 0, 0, 1, 0, 1);  // 'R'
    char_class_test(type, 83, 1, 0, 0, 0, 0, 1, 0, 1);  // 'S'
    char_class_test(type, 84, 1, 0, 0, 0, 0, 1, 0, 1);  // 'T'
    char_class_test(type, 85, 1, 0, 0, 0, 0, 1, 0, 1);  // 'U'
    char_class_test(type, 86, 1, 0, 0, 0, 0, 1, 0, 1);  // 'V'
    char_class_test(type, 87, 1, 0, 0, 0, 0, 1, 0, 1);  // 'W'
    char_class_test(type, 88, 1, 0, 0, 0, 0, 1, 0, 1);  // 'X'
    char_class_test(type, 89, 1, 0, 0, 0, 0, 1, 0, 1);  // 'Y'
    char_class_test(type, 90, 1, 0, 0, 0, 0, 1, 0, 1);  // 'Z'
    char_class_test(type, 91, 0, 0, 0, 0, 0, 1, 0, 0);  // '['
    char_class_test(type, 92, 0, 0, 0, 0, 0, 1, 0, 0);  // '\\'
    char_class_test(type, 93, 0, 0, 0, 0, 0, 1, 0, 0);  // ']'
    char_class_test(type, 94, 0, 0, 0, 0, 0, 1, 0, 0);  // '^'
    char_class_test(type, 95, 0, 0, 0, 0, 0, 1, 0, 0);  // '_'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    char_class_test(type, 96, 0, 0, 0, 0, 0, 1, 0, 0);   // '`'
    char_class_test(type, 97, 1, 0, 0, 1, 0, 1, 0, 0);   // 'a'
    char_class_test(type, 98, 1, 0, 0, 1, 0, 1, 0, 0);   // 'b'
    char_class_test(type, 99, 1, 0, 0, 1, 0, 1, 0, 0);   // 'c'
    char_class_test(type, 100, 1, 0, 0, 1, 0, 1, 0, 0);  // 'd'
    char_class_test(type, 101, 1, 0, 0, 1, 0, 1, 0, 0);  // 'e'
    char_class_test(type, 102, 1, 0, 0, 1, 0, 1, 0, 0);  // 'f'
    char_class_test(type, 103, 1, 0, 0, 1, 0, 1, 0, 0);  // 'g'
    char_class_test(type, 104, 1, 0, 0, 1, 0, 1, 0, 0);  // 'h'
    char_class_test(type, 105, 1, 0, 0, 1, 0, 1, 0, 0);  // 'i'
    char_class_test(type, 106, 1, 0, 0, 1, 0, 1, 0, 0);  // 'j'
    char_class_test(type, 107, 1, 0, 0, 1, 0, 1, 0, 0);  // 'k'
    char_class_test(type, 108, 1, 0, 0, 1, 0, 1, 0, 0);  // 'l'
    char_class_test(type, 109, 1, 0, 0, 1, 0, 1, 0, 0);  // 'm'
    char_class_test(type, 110, 1, 0, 0, 1, 0, 1, 0, 0);  // 'n'
    char_class_test(type, 111, 1, 0, 0, 1, 0, 1, 0, 0);  // 'o'
    char_class_test(type, 112, 1, 0, 0, 1, 0, 1, 0, 0);  // 'p'
    char_class_test(type, 113, 1, 0, 0, 1, 0, 1, 0, 0);  // 'q'
    char_class_test(type, 114, 1, 0, 0, 1, 0, 1, 0, 0);  // 'r'
    char_class_test(type, 115, 1, 0, 0, 1, 0, 1, 0, 0);  // 's'
    char_class_test(type, 116, 1, 0, 0, 1, 0, 1, 0, 0);  // 't'
    char_class_test(type, 117, 1, 0, 0, 1, 0, 1, 0, 0);  // 'u'
    char_class_test(type, 118, 1, 0, 0, 1, 0, 1, 0, 0);  // 'v'
    char_class_test(type, 119, 1, 0, 0, 1, 0, 1, 0, 0);  // 'w'
    char_class_test(type, 120, 1, 0, 0, 1, 0, 1, 0, 0);  // 'x'
    char_class_test(type, 121, 1, 0, 0, 1, 0, 1, 0, 0);  // 'y'
    char_class_test(type, 122, 1, 0, 0, 1, 0, 1, 0, 0);  // 'z'
    char_class_test(type, 123, 0, 0, 0, 0, 0, 1, 0, 0);  // '{'
    char_class_test(type, 124, 0, 0, 0, 0, 0, 1, 0, 0);  // '|'
    char_class_test(type, 125, 0, 0, 0, 0, 0, 1, 0, 0);  // '}'
    char_class_test(type, 126, 0, 0, 0, 0, 0, 1, 0, 0);  // '~'
    char_class_test(type, 127, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x7f'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    char_class_test(type, 128, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x80'
    char_class_test(type, 129, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x81'
    char_class_test(type, 130, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x82'
    char_class_test(type, 131, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x83'
    char_class_test(type, 132, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x84'
    char_class_test(type, 133, 0, 0, 0, 0, 0, 0, 2, 0);  // '\x85'
    char_class_test(type, 134, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x86'
    char_class_test(type, 135, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x87'
    char_class_test(type, 136, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x88'
    char_class_test(type, 137, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x89'
    char_class_test(type, 138, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8a'
    char_class_test(type, 139, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8b'
    char_class_test(type, 140, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8c'
    char_class_test(type, 141, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8d'
    char_class_test(type, 142, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8e'
    char_class_test(type, 143, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x8f'
    char_class_test(type, 144, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x90'
    char_class_test(type, 145, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x91'
    char_class_test(type, 146, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x92'
    char_class_test(type, 147, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x93'
    char_class_test(type, 148, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x94'
    char_class_test(type, 149, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x95'
    char_class_test(type, 150, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x96'
    char_class_test(type, 151, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x97'
    char_class_test(type, 152, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x98'
    char_class_test(type, 153, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x99'
    char_class_test(type, 154, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9a'
    char_class_test(type, 155, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9b'
    char_class_test(type, 156, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9c'
    char_class_test(type, 157, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9d'
    char_class_test(type, 158, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9e'
    char_class_test(type, 159, 0, 0, 0, 0, 0, 0, 0, 0);  // '\x9f'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    char_class_test(type, 160, 0, 0, 0, 0, 0, 0, 2, 0);  // '\xa0'
    char_class_test(type, 161, 0, 0, 0, 0, 0, 2, 0, 0);  // '¡'
    char_class_test(type, 162, 0, 0, 0, 0, 0, 2, 0, 0);  // '¢'
    char_class_test(type, 163, 0, 0, 0, 0, 0, 2, 0, 0);  // '£'
    char_class_test(type, 164, 0, 0, 0, 0, 0, 2, 0, 0);  // '¤'
    char_class_test(type, 165, 0, 0, 0, 0, 0, 2, 0, 0);  // '¥'
    char_class_test(type, 166, 0, 0, 0, 0, 0, 2, 0, 0);  // '¦'
    char_class_test(type, 167, 0, 0, 0, 0, 0, 2, 0, 0);  // '§'
    char_class_test(type, 168, 0, 0, 0, 0, 0, 2, 0, 0);  // '¨'
    char_class_test(type, 169, 0, 0, 0, 0, 0, 2, 0, 0);  // '©'
    char_class_test(type, 170, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ª'
    char_class_test(type, 171, 0, 0, 0, 0, 0, 2, 0, 0);  // '«'
    char_class_test(type, 172, 0, 0, 0, 0, 0, 2, 0, 0);  // '¬'
    char_class_test(type, 173, 0, 0, 0, 0, 0, 0, 0, 0);  // '\xad'
    char_class_test(type, 174, 0, 0, 0, 0, 0, 2, 0, 0);  // '®'
    char_class_test(type, 175, 0, 0, 0, 0, 0, 2, 0, 0);  // '¯'
    char_class_test(type, 176, 0, 0, 0, 0, 0, 2, 0, 0);  // '°'
    char_class_test(type, 177, 0, 0, 0, 0, 0, 2, 0, 0);  // '±'
    char_class_test(type, 178, 0, 0, 2, 0, 2, 2, 0, 0);  // '²'
    char_class_test(type, 179, 0, 0, 2, 0, 2, 2, 0, 0);  // '³'
    char_class_test(type, 180, 0, 0, 0, 0, 0, 2, 0, 0);  // '´'
    char_class_test(type, 181, 2, 0, 0, 2, 0, 2, 0, 0);  // 'µ'
    char_class_test(type, 182, 0, 0, 0, 0, 0, 2, 0, 0);  // '¶'
    char_class_test(type, 183, 0, 0, 0, 0, 0, 2, 0, 0);  // '·'
    char_class_test(type, 184, 0, 0, 0, 0, 0, 2, 0, 0);  // '¸'
    char_class_test(type, 185, 0, 0, 2, 0, 2, 2, 0, 0);  // '¹'
    char_class_test(type, 186, 2, 0, 0, 2, 0, 2, 0, 0);  // 'º'
    char_class_test(type, 187, 0, 0, 0, 0, 0, 2, 0, 0);  // '»'
    char_class_test(type, 188, 0, 0, 0, 0, 2, 2, 0, 0);  // '¼'
    char_class_test(type, 189, 0, 0, 0, 0, 2, 2, 0, 0);  // '½'
    char_class_test(type, 190, 0, 0, 0, 0, 2, 2, 0, 0);  // '¾'
    char_class_test(type, 191, 0, 0, 0, 0, 0, 2, 0, 0);  // '¿'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    char_class_test(type, 192, 2, 0, 0, 0, 0, 2, 0, 2);  // 'À'
    char_class_test(type, 193, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Á'
    char_class_test(type, 194, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Â'
    char_class_test(type, 195, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ã'
    char_class_test(type, 196, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ä'
    char_class_test(type, 197, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Å'
    char_class_test(type, 198, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Æ'
    char_class_test(type, 199, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ç'
    char_class_test(type, 200, 2, 0, 0, 0, 0, 2, 0, 2);  // 'È'
    char_class_test(type, 201, 2, 0, 0, 0, 0, 2, 0, 2);  // 'É'
    char_class_test(type, 202, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ê'
    char_class_test(type, 203, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ë'
    char_class_test(type, 204, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ì'
    char_class_test(type, 205, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Í'
    char_class_test(type, 206, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Î'
    char_class_test(type, 207, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ï'
    char_class_test(type, 208, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ð'
    char_class_test(type, 209, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ñ'
    char_class_test(type, 210, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ò'
    char_class_test(type, 211, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ó'
    char_class_test(type, 212, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ô'
    char_class_test(type, 213, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Õ'
    char_class_test(type, 214, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ö'
    char_class_test(type, 215, 0, 0, 0, 0, 0, 2, 0, 0);  // '×'
    char_class_test(type, 216, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ø'
    char_class_test(type, 217, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ù'
    char_class_test(type, 218, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ú'
    char_class_test(type, 219, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Û'
    char_class_test(type, 220, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ü'
    char_class_test(type, 221, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Ý'
    char_class_test(type, 222, 2, 0, 0, 0, 0, 2, 0, 2);  // 'Þ'
    char_class_test(type, 223, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ß'
    // alpha, decimal, digit, lower, numeric, printable, space, upper
    char_class_test(type, 224, 2, 0, 0, 2, 0, 2, 0, 0);  // 'à'
    char_class_test(type, 225, 2, 0, 0, 2, 0, 2, 0, 0);  // 'á'
    char_class_test(type, 226, 2, 0, 0, 2, 0, 2, 0, 0);  // 'â'
    char_class_test(type, 227, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ã'
    char_class_test(type, 228, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ä'
    char_class_test(type, 229, 2, 0, 0, 2, 0, 2, 0, 0);  // 'å'
    char_class_test(type, 230, 2, 0, 0, 2, 0, 2, 0, 0);  // 'æ'
    char_class_test(type, 231, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ç'
    char_class_test(type, 232, 2, 0, 0, 2, 0, 2, 0, 0);  // 'è'
    char_class_test(type, 233, 2, 0, 0, 2, 0, 2, 0, 0);  // 'é'
    char_class_test(type, 234, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ê'
    char_class_test(type, 235, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ë'
    char_class_test(type, 236, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ì'
    char_class_test(type, 237, 2, 0, 0, 2, 0, 2, 0, 0);  // 'í'
    char_class_test(type, 238, 2, 0, 0, 2, 0, 2, 0, 0);  // 'î'
    char_class_test(type, 239, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ï'
    char_class_test(type, 240, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ð'
    char_class_test(type, 241, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ñ'
    char_class_test(type, 242, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ò'
    char_class_test(type, 243, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ó'
    char_class_test(type, 244, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ô'
    char_class_test(type, 245, 2, 0, 0, 2, 0, 2, 0, 0);  // 'õ'
    char_class_test(type, 246, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ö'
    char_class_test(type, 247, 0, 0, 0, 0, 0, 2, 0, 0);  // '÷'
    char_class_test(type, 248, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ø'
    char_class_test(type, 249, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ù'
    char_class_test(type, 250, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ú'
    char_class_test(type, 251, 2, 0, 0, 2, 0, 2, 0, 0);  // 'û'
    char_class_test(type, 252, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ü'
    char_class_test(type, 253, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ý'
    char_class_test(type, 254, 2, 0, 0, 2, 0, 2, 0, 0);  // 'þ'
    char_class_test(type, 255, 2, 0, 0, 2, 0, 2, 0, 0);  // 'ÿ'

    return MUNIT_OK;
}

// Used by test_latin_1_converters, called on each latin-1 character.
static void char_conv_test(fixture_type_t *type, int ord, int lower0, int lower1, int upper0,
        int upper1, int casefold0, int casefold1, int swapcase0, int swapcase1, int capitalize0,
        int capitalize1)
{
    ypObject *s = type->fromordsCN(1, ord);

#define assert_conv(expression, expected0, expected1)                                           \
    do {                                                                                        \
        ypObject *_ypmt_CONV_expression = (expression);                                         \
        ypObject *_ypmt_CONV_expected;                                                          \
        if (isbinary(type) && ord > 0x7F) {                                                     \
            _ypmt_CONV_expected = yp_incref(_ypmt_CONV_expression);                             \
        } else {                                                                                \
            _ypmt_CONV_expected =                                                               \
                    type->fromordsCN((expected1) < 0 ? 1 : 2, (expected0), (expected1));        \
        }                                                                                       \
        _assert_obj(_ypmt_CONV_expression, eq, _ypmt_CONV_expected, "%s /*ord %d*/",            \
                "<ords %d, %d>", #expression, ord, (expected0), (expected1));                   \
        _assert_type_is(_ypmt_CONV_expression, type->yp_type, "%s /*ord %d*/", "type->yp_type", \
                #expression, ord);                                                              \
        yp_decrefN(N(_ypmt_CONV_expected, _ypmt_CONV_expression));                              \
    } while (0)

    assert_conv(yp_lower(s), lower0, lower1);
    assert_conv(yp_upper(s), upper0, upper1);
    assert_conv(yp_swapcase(s), swapcase0, swapcase1);
    // assert_conv(yp_capitalize(s), capitalize0, capitalize1);
    // if (!isbinary(type)) {
    //     assert_conv(yp_casefold(s), casefold0, casefold1);
    // }

#undef assert_conv
    yp_decrefN(N(s));
}

// Tests for the string converters for the latin-1 characters. The full Unicode Character Database
// is an optional feature of nohtyP, but the latin-1 characters are always supported, and mostly
// share the same conversions between bytes and str.
static MunitResult test_latin_1_converters(const MunitParameter params[], fixture_t *fixture)
{
    fixture_type_t *type = fixture->type;

    // FIXME In Python, compare bytes to strs for same conversions?
    // FIXME bytes only operates on the ASCII characters
    /*
    methods = "lower, upper, casefold, swapcase, capitalize"
    def get_expected(x, m):
        ords = [str(ord(c)) for c in getattr(x, m)()]
        if len(ords) == 1: ords.append(-1)
        assert len(ords) == 2
        return f"{ords[0]}, {ords[1]}"
    for i in range(256):
        x = chr(i)
        expected = [get_expected(x, m) for m in methods.split(", ")]
        if i % 32 == 0: print(f"    // {methods}")
        print(f"    char_conv_test(type, {i}, {", ".join(expected)});  // {x!r}")
    */

    // lower, upper, casefold, swapcase, capitalize
    char_conv_test(type, 0, 0, -1, 0, -1, 0, -1, 0, -1, 0, -1);        // '\x00'
    char_conv_test(type, 1, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1);        // '\x01'
    char_conv_test(type, 2, 2, -1, 2, -1, 2, -1, 2, -1, 2, -1);        // '\x02'
    char_conv_test(type, 3, 3, -1, 3, -1, 3, -1, 3, -1, 3, -1);        // '\x03'
    char_conv_test(type, 4, 4, -1, 4, -1, 4, -1, 4, -1, 4, -1);        // '\x04'
    char_conv_test(type, 5, 5, -1, 5, -1, 5, -1, 5, -1, 5, -1);        // '\x05'
    char_conv_test(type, 6, 6, -1, 6, -1, 6, -1, 6, -1, 6, -1);        // '\x06'
    char_conv_test(type, 7, 7, -1, 7, -1, 7, -1, 7, -1, 7, -1);        // '\x07'
    char_conv_test(type, 8, 8, -1, 8, -1, 8, -1, 8, -1, 8, -1);        // '\x08'
    char_conv_test(type, 9, 9, -1, 9, -1, 9, -1, 9, -1, 9, -1);        // '\t'
    char_conv_test(type, 10, 10, -1, 10, -1, 10, -1, 10, -1, 10, -1);  // '\n'
    char_conv_test(type, 11, 11, -1, 11, -1, 11, -1, 11, -1, 11, -1);  // '\x0b'
    char_conv_test(type, 12, 12, -1, 12, -1, 12, -1, 12, -1, 12, -1);  // '\x0c'
    char_conv_test(type, 13, 13, -1, 13, -1, 13, -1, 13, -1, 13, -1);  // '\r'
    char_conv_test(type, 14, 14, -1, 14, -1, 14, -1, 14, -1, 14, -1);  // '\x0e'
    char_conv_test(type, 15, 15, -1, 15, -1, 15, -1, 15, -1, 15, -1);  // '\x0f'
    char_conv_test(type, 16, 16, -1, 16, -1, 16, -1, 16, -1, 16, -1);  // '\x10'
    char_conv_test(type, 17, 17, -1, 17, -1, 17, -1, 17, -1, 17, -1);  // '\x11'
    char_conv_test(type, 18, 18, -1, 18, -1, 18, -1, 18, -1, 18, -1);  // '\x12'
    char_conv_test(type, 19, 19, -1, 19, -1, 19, -1, 19, -1, 19, -1);  // '\x13'
    char_conv_test(type, 20, 20, -1, 20, -1, 20, -1, 20, -1, 20, -1);  // '\x14'
    char_conv_test(type, 21, 21, -1, 21, -1, 21, -1, 21, -1, 21, -1);  // '\x15'
    char_conv_test(type, 22, 22, -1, 22, -1, 22, -1, 22, -1, 22, -1);  // '\x16'
    char_conv_test(type, 23, 23, -1, 23, -1, 23, -1, 23, -1, 23, -1);  // '\x17'
    char_conv_test(type, 24, 24, -1, 24, -1, 24, -1, 24, -1, 24, -1);  // '\x18'
    char_conv_test(type, 25, 25, -1, 25, -1, 25, -1, 25, -1, 25, -1);  // '\x19'
    char_conv_test(type, 26, 26, -1, 26, -1, 26, -1, 26, -1, 26, -1);  // '\x1a'
    char_conv_test(type, 27, 27, -1, 27, -1, 27, -1, 27, -1, 27, -1);  // '\x1b'
    char_conv_test(type, 28, 28, -1, 28, -1, 28, -1, 28, -1, 28, -1);  // '\x1c'
    char_conv_test(type, 29, 29, -1, 29, -1, 29, -1, 29, -1, 29, -1);  // '\x1d'
    char_conv_test(type, 30, 30, -1, 30, -1, 30, -1, 30, -1, 30, -1);  // '\x1e'
    char_conv_test(type, 31, 31, -1, 31, -1, 31, -1, 31, -1, 31, -1);  // '\x1f'
    // lower, upper, casefold, swapcase, capitalize
    char_conv_test(type, 32, 32, -1, 32, -1, 32, -1, 32, -1, 32, -1);  // ' '
    char_conv_test(type, 33, 33, -1, 33, -1, 33, -1, 33, -1, 33, -1);  // '!'
    char_conv_test(type, 34, 34, -1, 34, -1, 34, -1, 34, -1, 34, -1);  // '"'
    char_conv_test(type, 35, 35, -1, 35, -1, 35, -1, 35, -1, 35, -1);  // '#'
    char_conv_test(type, 36, 36, -1, 36, -1, 36, -1, 36, -1, 36, -1);  // '$'
    char_conv_test(type, 37, 37, -1, 37, -1, 37, -1, 37, -1, 37, -1);  // '%'
    char_conv_test(type, 38, 38, -1, 38, -1, 38, -1, 38, -1, 38, -1);  // '&'
    char_conv_test(type, 39, 39, -1, 39, -1, 39, -1, 39, -1, 39, -1);  // "'"
    char_conv_test(type, 40, 40, -1, 40, -1, 40, -1, 40, -1, 40, -1);  // '('
    char_conv_test(type, 41, 41, -1, 41, -1, 41, -1, 41, -1, 41, -1);  // ')'
    char_conv_test(type, 42, 42, -1, 42, -1, 42, -1, 42, -1, 42, -1);  // '*'
    char_conv_test(type, 43, 43, -1, 43, -1, 43, -1, 43, -1, 43, -1);  // '+'
    char_conv_test(type, 44, 44, -1, 44, -1, 44, -1, 44, -1, 44, -1);  // ','
    char_conv_test(type, 45, 45, -1, 45, -1, 45, -1, 45, -1, 45, -1);  // '-'
    char_conv_test(type, 46, 46, -1, 46, -1, 46, -1, 46, -1, 46, -1);  // '.'
    char_conv_test(type, 47, 47, -1, 47, -1, 47, -1, 47, -1, 47, -1);  // '/'
    char_conv_test(type, 48, 48, -1, 48, -1, 48, -1, 48, -1, 48, -1);  // '0'
    char_conv_test(type, 49, 49, -1, 49, -1, 49, -1, 49, -1, 49, -1);  // '1'
    char_conv_test(type, 50, 50, -1, 50, -1, 50, -1, 50, -1, 50, -1);  // '2'
    char_conv_test(type, 51, 51, -1, 51, -1, 51, -1, 51, -1, 51, -1);  // '3'
    char_conv_test(type, 52, 52, -1, 52, -1, 52, -1, 52, -1, 52, -1);  // '4'
    char_conv_test(type, 53, 53, -1, 53, -1, 53, -1, 53, -1, 53, -1);  // '5'
    char_conv_test(type, 54, 54, -1, 54, -1, 54, -1, 54, -1, 54, -1);  // '6'
    char_conv_test(type, 55, 55, -1, 55, -1, 55, -1, 55, -1, 55, -1);  // '7'
    char_conv_test(type, 56, 56, -1, 56, -1, 56, -1, 56, -1, 56, -1);  // '8'
    char_conv_test(type, 57, 57, -1, 57, -1, 57, -1, 57, -1, 57, -1);  // '9'
    char_conv_test(type, 58, 58, -1, 58, -1, 58, -1, 58, -1, 58, -1);  // ':'
    char_conv_test(type, 59, 59, -1, 59, -1, 59, -1, 59, -1, 59, -1);  // ';'
    char_conv_test(type, 60, 60, -1, 60, -1, 60, -1, 60, -1, 60, -1);  // '<'
    char_conv_test(type, 61, 61, -1, 61, -1, 61, -1, 61, -1, 61, -1);  // '='
    char_conv_test(type, 62, 62, -1, 62, -1, 62, -1, 62, -1, 62, -1);  // '>'
    char_conv_test(type, 63, 63, -1, 63, -1, 63, -1, 63, -1, 63, -1);  // '?'
    // lower, upper, casefold, swapcase, capitalize
    char_conv_test(type, 64, 64, -1, 64, -1, 64, -1, 64, -1, 64, -1);     // '@'
    char_conv_test(type, 65, 97, -1, 65, -1, 97, -1, 97, -1, 65, -1);     // 'A'
    char_conv_test(type, 66, 98, -1, 66, -1, 98, -1, 98, -1, 66, -1);     // 'B'
    char_conv_test(type, 67, 99, -1, 67, -1, 99, -1, 99, -1, 67, -1);     // 'C'
    char_conv_test(type, 68, 100, -1, 68, -1, 100, -1, 100, -1, 68, -1);  // 'D'
    char_conv_test(type, 69, 101, -1, 69, -1, 101, -1, 101, -1, 69, -1);  // 'E'
    char_conv_test(type, 70, 102, -1, 70, -1, 102, -1, 102, -1, 70, -1);  // 'F'
    char_conv_test(type, 71, 103, -1, 71, -1, 103, -1, 103, -1, 71, -1);  // 'G'
    char_conv_test(type, 72, 104, -1, 72, -1, 104, -1, 104, -1, 72, -1);  // 'H'
    char_conv_test(type, 73, 105, -1, 73, -1, 105, -1, 105, -1, 73, -1);  // 'I'
    char_conv_test(type, 74, 106, -1, 74, -1, 106, -1, 106, -1, 74, -1);  // 'J'
    char_conv_test(type, 75, 107, -1, 75, -1, 107, -1, 107, -1, 75, -1);  // 'K'
    char_conv_test(type, 76, 108, -1, 76, -1, 108, -1, 108, -1, 76, -1);  // 'L'
    char_conv_test(type, 77, 109, -1, 77, -1, 109, -1, 109, -1, 77, -1);  // 'M'
    char_conv_test(type, 78, 110, -1, 78, -1, 110, -1, 110, -1, 78, -1);  // 'N'
    char_conv_test(type, 79, 111, -1, 79, -1, 111, -1, 111, -1, 79, -1);  // 'O'
    char_conv_test(type, 80, 112, -1, 80, -1, 112, -1, 112, -1, 80, -1);  // 'P'
    char_conv_test(type, 81, 113, -1, 81, -1, 113, -1, 113, -1, 81, -1);  // 'Q'
    char_conv_test(type, 82, 114, -1, 82, -1, 114, -1, 114, -1, 82, -1);  // 'R'
    char_conv_test(type, 83, 115, -1, 83, -1, 115, -1, 115, -1, 83, -1);  // 'S'
    char_conv_test(type, 84, 116, -1, 84, -1, 116, -1, 116, -1, 84, -1);  // 'T'
    char_conv_test(type, 85, 117, -1, 85, -1, 117, -1, 117, -1, 85, -1);  // 'U'
    char_conv_test(type, 86, 118, -1, 86, -1, 118, -1, 118, -1, 86, -1);  // 'V'
    char_conv_test(type, 87, 119, -1, 87, -1, 119, -1, 119, -1, 87, -1);  // 'W'
    char_conv_test(type, 88, 120, -1, 88, -1, 120, -1, 120, -1, 88, -1);  // 'X'
    char_conv_test(type, 89, 121, -1, 89, -1, 121, -1, 121, -1, 89, -1);  // 'Y'
    char_conv_test(type, 90, 122, -1, 90, -1, 122, -1, 122, -1, 90, -1);  // 'Z'
    char_conv_test(type, 91, 91, -1, 91, -1, 91, -1, 91, -1, 91, -1);     // '['
    char_conv_test(type, 92, 92, -1, 92, -1, 92, -1, 92, -1, 92, -1);     // '\\'
    char_conv_test(type, 93, 93, -1, 93, -1, 93, -1, 93, -1, 93, -1);     // ']'
    char_conv_test(type, 94, 94, -1, 94, -1, 94, -1, 94, -1, 94, -1);     // '^'
    char_conv_test(type, 95, 95, -1, 95, -1, 95, -1, 95, -1, 95, -1);     // '_'
    // lower, upper, casefold, swapcase, capitalize
    char_conv_test(type, 96, 96, -1, 96, -1, 96, -1, 96, -1, 96, -1);        // '`'
    char_conv_test(type, 97, 97, -1, 65, -1, 97, -1, 65, -1, 65, -1);        // 'a'
    char_conv_test(type, 98, 98, -1, 66, -1, 98, -1, 66, -1, 66, -1);        // 'b'
    char_conv_test(type, 99, 99, -1, 67, -1, 99, -1, 67, -1, 67, -1);        // 'c'
    char_conv_test(type, 100, 100, -1, 68, -1, 100, -1, 68, -1, 68, -1);     // 'd'
    char_conv_test(type, 101, 101, -1, 69, -1, 101, -1, 69, -1, 69, -1);     // 'e'
    char_conv_test(type, 102, 102, -1, 70, -1, 102, -1, 70, -1, 70, -1);     // 'f'
    char_conv_test(type, 103, 103, -1, 71, -1, 103, -1, 71, -1, 71, -1);     // 'g'
    char_conv_test(type, 104, 104, -1, 72, -1, 104, -1, 72, -1, 72, -1);     // 'h'
    char_conv_test(type, 105, 105, -1, 73, -1, 105, -1, 73, -1, 73, -1);     // 'i'
    char_conv_test(type, 106, 106, -1, 74, -1, 106, -1, 74, -1, 74, -1);     // 'j'
    char_conv_test(type, 107, 107, -1, 75, -1, 107, -1, 75, -1, 75, -1);     // 'k'
    char_conv_test(type, 108, 108, -1, 76, -1, 108, -1, 76, -1, 76, -1);     // 'l'
    char_conv_test(type, 109, 109, -1, 77, -1, 109, -1, 77, -1, 77, -1);     // 'm'
    char_conv_test(type, 110, 110, -1, 78, -1, 110, -1, 78, -1, 78, -1);     // 'n'
    char_conv_test(type, 111, 111, -1, 79, -1, 111, -1, 79, -1, 79, -1);     // 'o'
    char_conv_test(type, 112, 112, -1, 80, -1, 112, -1, 80, -1, 80, -1);     // 'p'
    char_conv_test(type, 113, 113, -1, 81, -1, 113, -1, 81, -1, 81, -1);     // 'q'
    char_conv_test(type, 114, 114, -1, 82, -1, 114, -1, 82, -1, 82, -1);     // 'r'
    char_conv_test(type, 115, 115, -1, 83, -1, 115, -1, 83, -1, 83, -1);     // 's'
    char_conv_test(type, 116, 116, -1, 84, -1, 116, -1, 84, -1, 84, -1);     // 't'
    char_conv_test(type, 117, 117, -1, 85, -1, 117, -1, 85, -1, 85, -1);     // 'u'
    char_conv_test(type, 118, 118, -1, 86, -1, 118, -1, 86, -1, 86, -1);     // 'v'
    char_conv_test(type, 119, 119, -1, 87, -1, 119, -1, 87, -1, 87, -1);     // 'w'
    char_conv_test(type, 120, 120, -1, 88, -1, 120, -1, 88, -1, 88, -1);     // 'x'
    char_conv_test(type, 121, 121, -1, 89, -1, 121, -1, 89, -1, 89, -1);     // 'y'
    char_conv_test(type, 122, 122, -1, 90, -1, 122, -1, 90, -1, 90, -1);     // 'z'
    char_conv_test(type, 123, 123, -1, 123, -1, 123, -1, 123, -1, 123, -1);  // '{'
    char_conv_test(type, 124, 124, -1, 124, -1, 124, -1, 124, -1, 124, -1);  // '|'
    char_conv_test(type, 125, 125, -1, 125, -1, 125, -1, 125, -1, 125, -1);  // '}'
    char_conv_test(type, 126, 126, -1, 126, -1, 126, -1, 126, -1, 126, -1);  // '~'
    char_conv_test(type, 127, 127, -1, 127, -1, 127, -1, 127, -1, 127, -1);  // '\x7f'
    // lower, upper, casefold, swapcase, capitalize
    char_conv_test(type, 128, 128, -1, 128, -1, 128, -1, 128, -1, 128, -1);  // '\x80'
    char_conv_test(type, 129, 129, -1, 129, -1, 129, -1, 129, -1, 129, -1);  // '\x81'
    char_conv_test(type, 130, 130, -1, 130, -1, 130, -1, 130, -1, 130, -1);  // '\x82'
    char_conv_test(type, 131, 131, -1, 131, -1, 131, -1, 131, -1, 131, -1);  // '\x83'
    char_conv_test(type, 132, 132, -1, 132, -1, 132, -1, 132, -1, 132, -1);  // '\x84'
    char_conv_test(type, 133, 133, -1, 133, -1, 133, -1, 133, -1, 133, -1);  // '\x85'
    char_conv_test(type, 134, 134, -1, 134, -1, 134, -1, 134, -1, 134, -1);  // '\x86'
    char_conv_test(type, 135, 135, -1, 135, -1, 135, -1, 135, -1, 135, -1);  // '\x87'
    char_conv_test(type, 136, 136, -1, 136, -1, 136, -1, 136, -1, 136, -1);  // '\x88'
    char_conv_test(type, 137, 137, -1, 137, -1, 137, -1, 137, -1, 137, -1);  // '\x89'
    char_conv_test(type, 138, 138, -1, 138, -1, 138, -1, 138, -1, 138, -1);  // '\x8a'
    char_conv_test(type, 139, 139, -1, 139, -1, 139, -1, 139, -1, 139, -1);  // '\x8b'
    char_conv_test(type, 140, 140, -1, 140, -1, 140, -1, 140, -1, 140, -1);  // '\x8c'
    char_conv_test(type, 141, 141, -1, 141, -1, 141, -1, 141, -1, 141, -1);  // '\x8d'
    char_conv_test(type, 142, 142, -1, 142, -1, 142, -1, 142, -1, 142, -1);  // '\x8e'
    char_conv_test(type, 143, 143, -1, 143, -1, 143, -1, 143, -1, 143, -1);  // '\x8f'
    char_conv_test(type, 144, 144, -1, 144, -1, 144, -1, 144, -1, 144, -1);  // '\x90'
    char_conv_test(type, 145, 145, -1, 145, -1, 145, -1, 145, -1, 145, -1);  // '\x91'
    char_conv_test(type, 146, 146, -1, 146, -1, 146, -1, 146, -1, 146, -1);  // '\x92'
    char_conv_test(type, 147, 147, -1, 147, -1, 147, -1, 147, -1, 147, -1);  // '\x93'
    char_conv_test(type, 148, 148, -1, 148, -1, 148, -1, 148, -1, 148, -1);  // '\x94'
    char_conv_test(type, 149, 149, -1, 149, -1, 149, -1, 149, -1, 149, -1);  // '\x95'
    char_conv_test(type, 150, 150, -1, 150, -1, 150, -1, 150, -1, 150, -1);  // '\x96'
    char_conv_test(type, 151, 151, -1, 151, -1, 151, -1, 151, -1, 151, -1);  // '\x97'
    char_conv_test(type, 152, 152, -1, 152, -1, 152, -1, 152, -1, 152, -1);  // '\x98'
    char_conv_test(type, 153, 153, -1, 153, -1, 153, -1, 153, -1, 153, -1);  // '\x99'
    char_conv_test(type, 154, 154, -1, 154, -1, 154, -1, 154, -1, 154, -1);  // '\x9a'
    char_conv_test(type, 155, 155, -1, 155, -1, 155, -1, 155, -1, 155, -1);  // '\x9b'
    char_conv_test(type, 156, 156, -1, 156, -1, 156, -1, 156, -1, 156, -1);  // '\x9c'
    char_conv_test(type, 157, 157, -1, 157, -1, 157, -1, 157, -1, 157, -1);  // '\x9d'
    char_conv_test(type, 158, 158, -1, 158, -1, 158, -1, 158, -1, 158, -1);  // '\x9e'
    char_conv_test(type, 159, 159, -1, 159, -1, 159, -1, 159, -1, 159, -1);  // '\x9f'
    // lower, upper, casefold, swapcase, capitalize
    char_conv_test(type, 160, 160, -1, 160, -1, 160, -1, 160, -1, 160, -1);  // '\xa0'
    char_conv_test(type, 161, 161, -1, 161, -1, 161, -1, 161, -1, 161, -1);  // '¡'
    char_conv_test(type, 162, 162, -1, 162, -1, 162, -1, 162, -1, 162, -1);  // '¢'
    char_conv_test(type, 163, 163, -1, 163, -1, 163, -1, 163, -1, 163, -1);  // '£'
    char_conv_test(type, 164, 164, -1, 164, -1, 164, -1, 164, -1, 164, -1);  // '¤'
    char_conv_test(type, 165, 165, -1, 165, -1, 165, -1, 165, -1, 165, -1);  // '¥'
    char_conv_test(type, 166, 166, -1, 166, -1, 166, -1, 166, -1, 166, -1);  // '¦'
    char_conv_test(type, 167, 167, -1, 167, -1, 167, -1, 167, -1, 167, -1);  // '§'
    char_conv_test(type, 168, 168, -1, 168, -1, 168, -1, 168, -1, 168, -1);  // '¨'
    char_conv_test(type, 169, 169, -1, 169, -1, 169, -1, 169, -1, 169, -1);  // '©'
    char_conv_test(type, 170, 170, -1, 170, -1, 170, -1, 170, -1, 170, -1);  // 'ª'
    char_conv_test(type, 171, 171, -1, 171, -1, 171, -1, 171, -1, 171, -1);  // '«'
    char_conv_test(type, 172, 172, -1, 172, -1, 172, -1, 172, -1, 172, -1);  // '¬'
    char_conv_test(type, 173, 173, -1, 173, -1, 173, -1, 173, -1, 173, -1);  // '\xad'
    char_conv_test(type, 174, 174, -1, 174, -1, 174, -1, 174, -1, 174, -1);  // '®'
    char_conv_test(type, 175, 175, -1, 175, -1, 175, -1, 175, -1, 175, -1);  // '¯'
    char_conv_test(type, 176, 176, -1, 176, -1, 176, -1, 176, -1, 176, -1);  // '°'
    char_conv_test(type, 177, 177, -1, 177, -1, 177, -1, 177, -1, 177, -1);  // '±'
    char_conv_test(type, 178, 178, -1, 178, -1, 178, -1, 178, -1, 178, -1);  // '²'
    char_conv_test(type, 179, 179, -1, 179, -1, 179, -1, 179, -1, 179, -1);  // '³'
    char_conv_test(type, 180, 180, -1, 180, -1, 180, -1, 180, -1, 180, -1);  // '´'
    char_conv_test(type, 181, 181, -1, 924, -1, 956, -1, 924, -1, 924, -1);  // 'µ'
    char_conv_test(type, 182, 182, -1, 182, -1, 182, -1, 182, -1, 182, -1);  // '¶'
    char_conv_test(type, 183, 183, -1, 183, -1, 183, -1, 183, -1, 183, -1);  // '·'
    char_conv_test(type, 184, 184, -1, 184, -1, 184, -1, 184, -1, 184, -1);  // '¸'
    char_conv_test(type, 185, 185, -1, 185, -1, 185, -1, 185, -1, 185, -1);  // '¹'
    char_conv_test(type, 186, 186, -1, 186, -1, 186, -1, 186, -1, 186, -1);  // 'º'
    char_conv_test(type, 187, 187, -1, 187, -1, 187, -1, 187, -1, 187, -1);  // '»'
    char_conv_test(type, 188, 188, -1, 188, -1, 188, -1, 188, -1, 188, -1);  // '¼'
    char_conv_test(type, 189, 189, -1, 189, -1, 189, -1, 189, -1, 189, -1);  // '½'
    char_conv_test(type, 190, 190, -1, 190, -1, 190, -1, 190, -1, 190, -1);  // '¾'
    char_conv_test(type, 191, 191, -1, 191, -1, 191, -1, 191, -1, 191, -1);  // '¿'
    // lower, upper, casefold, swapcase, capitalize
    char_conv_test(type, 192, 224, -1, 192, -1, 224, -1, 224, -1, 192, -1);  // 'À'
    char_conv_test(type, 193, 225, -1, 193, -1, 225, -1, 225, -1, 193, -1);  // 'Á'
    char_conv_test(type, 194, 226, -1, 194, -1, 226, -1, 226, -1, 194, -1);  // 'Â'
    char_conv_test(type, 195, 227, -1, 195, -1, 227, -1, 227, -1, 195, -1);  // 'Ã'
    char_conv_test(type, 196, 228, -1, 196, -1, 228, -1, 228, -1, 196, -1);  // 'Ä'
    char_conv_test(type, 197, 229, -1, 197, -1, 229, -1, 229, -1, 197, -1);  // 'Å'
    char_conv_test(type, 198, 230, -1, 198, -1, 230, -1, 230, -1, 198, -1);  // 'Æ'
    char_conv_test(type, 199, 231, -1, 199, -1, 231, -1, 231, -1, 199, -1);  // 'Ç'
    char_conv_test(type, 200, 232, -1, 200, -1, 232, -1, 232, -1, 200, -1);  // 'È'
    char_conv_test(type, 201, 233, -1, 201, -1, 233, -1, 233, -1, 201, -1);  // 'É'
    char_conv_test(type, 202, 234, -1, 202, -1, 234, -1, 234, -1, 202, -1);  // 'Ê'
    char_conv_test(type, 203, 235, -1, 203, -1, 235, -1, 235, -1, 203, -1);  // 'Ë'
    char_conv_test(type, 204, 236, -1, 204, -1, 236, -1, 236, -1, 204, -1);  // 'Ì'
    char_conv_test(type, 205, 237, -1, 205, -1, 237, -1, 237, -1, 205, -1);  // 'Í'
    char_conv_test(type, 206, 238, -1, 206, -1, 238, -1, 238, -1, 206, -1);  // 'Î'
    char_conv_test(type, 207, 239, -1, 207, -1, 239, -1, 239, -1, 207, -1);  // 'Ï'
    char_conv_test(type, 208, 240, -1, 208, -1, 240, -1, 240, -1, 208, -1);  // 'Ð'
    char_conv_test(type, 209, 241, -1, 209, -1, 241, -1, 241, -1, 209, -1);  // 'Ñ'
    char_conv_test(type, 210, 242, -1, 210, -1, 242, -1, 242, -1, 210, -1);  // 'Ò'
    char_conv_test(type, 211, 243, -1, 211, -1, 243, -1, 243, -1, 211, -1);  // 'Ó'
    char_conv_test(type, 212, 244, -1, 212, -1, 244, -1, 244, -1, 212, -1);  // 'Ô'
    char_conv_test(type, 213, 245, -1, 213, -1, 245, -1, 245, -1, 213, -1);  // 'Õ'
    char_conv_test(type, 214, 246, -1, 214, -1, 246, -1, 246, -1, 214, -1);  // 'Ö'
    char_conv_test(type, 215, 215, -1, 215, -1, 215, -1, 215, -1, 215, -1);  // '×'
    char_conv_test(type, 216, 248, -1, 216, -1, 248, -1, 248, -1, 216, -1);  // 'Ø'
    char_conv_test(type, 217, 249, -1, 217, -1, 249, -1, 249, -1, 217, -1);  // 'Ù'
    char_conv_test(type, 218, 250, -1, 218, -1, 250, -1, 250, -1, 218, -1);  // 'Ú'
    char_conv_test(type, 219, 251, -1, 219, -1, 251, -1, 251, -1, 219, -1);  // 'Û'
    char_conv_test(type, 220, 252, -1, 220, -1, 252, -1, 252, -1, 220, -1);  // 'Ü'
    char_conv_test(type, 221, 253, -1, 221, -1, 253, -1, 253, -1, 221, -1);  // 'Ý'
    char_conv_test(type, 222, 254, -1, 222, -1, 254, -1, 254, -1, 222, -1);  // 'Þ'
    char_conv_test(type, 223, 223, -1, 83, 83, 115, 115, 83, 83, 83, 115);   // 'ß'
    // lower, upper, casefold, swapcase, capitalize
    char_conv_test(type, 224, 224, -1, 192, -1, 224, -1, 192, -1, 192, -1);  // 'à'
    char_conv_test(type, 225, 225, -1, 193, -1, 225, -1, 193, -1, 193, -1);  // 'á'
    char_conv_test(type, 226, 226, -1, 194, -1, 226, -1, 194, -1, 194, -1);  // 'â'
    char_conv_test(type, 227, 227, -1, 195, -1, 227, -1, 195, -1, 195, -1);  // 'ã'
    char_conv_test(type, 228, 228, -1, 196, -1, 228, -1, 196, -1, 196, -1);  // 'ä'
    char_conv_test(type, 229, 229, -1, 197, -1, 229, -1, 197, -1, 197, -1);  // 'å'
    char_conv_test(type, 230, 230, -1, 198, -1, 230, -1, 198, -1, 198, -1);  // 'æ'
    char_conv_test(type, 231, 231, -1, 199, -1, 231, -1, 199, -1, 199, -1);  // 'ç'
    char_conv_test(type, 232, 232, -1, 200, -1, 232, -1, 200, -1, 200, -1);  // 'è'
    char_conv_test(type, 233, 233, -1, 201, -1, 233, -1, 201, -1, 201, -1);  // 'é'
    char_conv_test(type, 234, 234, -1, 202, -1, 234, -1, 202, -1, 202, -1);  // 'ê'
    char_conv_test(type, 235, 235, -1, 203, -1, 235, -1, 203, -1, 203, -1);  // 'ë'
    char_conv_test(type, 236, 236, -1, 204, -1, 236, -1, 204, -1, 204, -1);  // 'ì'
    char_conv_test(type, 237, 237, -1, 205, -1, 237, -1, 205, -1, 205, -1);  // 'í'
    char_conv_test(type, 238, 238, -1, 206, -1, 238, -1, 206, -1, 206, -1);  // 'î'
    char_conv_test(type, 239, 239, -1, 207, -1, 239, -1, 207, -1, 207, -1);  // 'ï'
    char_conv_test(type, 240, 240, -1, 208, -1, 240, -1, 208, -1, 208, -1);  // 'ð'
    char_conv_test(type, 241, 241, -1, 209, -1, 241, -1, 209, -1, 209, -1);  // 'ñ'
    char_conv_test(type, 242, 242, -1, 210, -1, 242, -1, 210, -1, 210, -1);  // 'ò'
    char_conv_test(type, 243, 243, -1, 211, -1, 243, -1, 211, -1, 211, -1);  // 'ó'
    char_conv_test(type, 244, 244, -1, 212, -1, 244, -1, 212, -1, 212, -1);  // 'ô'
    char_conv_test(type, 245, 245, -1, 213, -1, 245, -1, 213, -1, 213, -1);  // 'õ'
    char_conv_test(type, 246, 246, -1, 214, -1, 246, -1, 214, -1, 214, -1);  // 'ö'
    char_conv_test(type, 247, 247, -1, 247, -1, 247, -1, 247, -1, 247, -1);  // '÷'
    char_conv_test(type, 248, 248, -1, 216, -1, 248, -1, 216, -1, 216, -1);  // 'ø'
    char_conv_test(type, 249, 249, -1, 217, -1, 249, -1, 217, -1, 217, -1);  // 'ù'
    char_conv_test(type, 250, 250, -1, 218, -1, 250, -1, 218, -1, 218, -1);  // 'ú'
    char_conv_test(type, 251, 251, -1, 219, -1, 251, -1, 219, -1, 219, -1);  // 'û'
    char_conv_test(type, 252, 252, -1, 220, -1, 252, -1, 220, -1, 220, -1);  // 'ü'
    char_conv_test(type, 253, 253, -1, 221, -1, 253, -1, 221, -1, 221, -1);  // 'ý'
    char_conv_test(type, 254, 254, -1, 222, -1, 254, -1, 222, -1, 222, -1);  // 'þ'
    char_conv_test(type, 255, 255, -1, 376, -1, 255, -1, 376, -1, 376, -1);  // 'ÿ'

    return MUNIT_OK;
}


static MunitParameterEnum test_string_char_db_params[] = {
        {param_key_type, param_values_types_string_not_variant}, {NULL}};

MunitTest test_string_char_db_tests[] = {TEST(test_isalnum, test_string_char_db_params),
        TEST(test_isalpha, test_string_char_db_params),
        TEST(test_isascii, test_string_char_db_params),
        TEST(test_isdecimal, test_string_char_db_params),
        TEST(test_isdigit, test_string_char_db_params),
        TEST(test_isidentifier, test_string_char_db_params),
        TEST(test_islower, test_string_char_db_params),
        TEST(test_isnumeric, test_string_char_db_params),
        TEST(test_isprintable, test_string_char_db_params),
        TEST(test_isspace, test_string_char_db_params),
        TEST(test_isupper, test_string_char_db_params),
        TEST(test_latin_1_classifiers, test_string_char_db_params),
        TEST(test_latin_1_converters, test_string_char_db_params), {NULL}};


extern void test_string_char_db_initialize(void) {}
