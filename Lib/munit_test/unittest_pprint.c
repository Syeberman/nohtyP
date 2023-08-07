
#include "munit_test/unittest.h"


static void pprint_any(FILE *f, int indent, ypObject *obj);


static void fprintf_indent(FILE *f, int indent)
{
    while (indent > 0) {
        fprintf(f, "  ");
        indent--;
    }
}

static void pprint_sized_iterable(
        FILE *f, int indent, char *open, char *close, char *empty, ypObject *obj)
{
    yp_uint64_t mi_state;
    ypObject   *mi;
    yp_ssize_t  len;

    assert_not_raises_exc(len = yp_lenC(obj, &exc));
    if (len < 1) {
        // set opens with { and closes with }, but the empty set is set().
        fprintf(f, "%s", empty);
        return;
    }

    assert_not_raises(mi = yp_miniiter(obj, &mi_state));  // new ref

    fprintf(f, "%s", open);
    if (len > 1) fprintf(f, "\n");

    while (1) {
        ypObject *item = yp_miniiter_next(mi, &mi_state);
        if (yp_isexceptionC2(item, yp_StopIteration)) break;
        assert_not_exception(item);

        if (len > 1) fprintf_indent(f, indent + 1);
        pprint_any(f, indent + 1, item);
        fprintf(f, ",");
        if (len > 1) fprintf(f, "\n");

        yp_decrefN(N(item));
    }

    if (len > 1) fprintf_indent(f, indent);
    fprintf(f, "%s", close);

    yp_decrefN(N(mi));
}

static void pprint_mapping(FILE *f, int indent, char *open, char *close, char *empty, ypObject *obj)
{
    yp_uint64_t mi_state;
    ypObject   *mi;
    yp_ssize_t  len;

    assert_not_raises_exc(len = yp_lenC(obj, &exc));
    if (len < 1) {
        // set opens with { and closes with }, but the empty set is set().
        fprintf(f, "%s", empty);
        return;
    }

    assert_not_raises(mi = yp_miniiter_items(obj, &mi_state));  // new ref

    fprintf(f, "%s", open);
    if (len > 1) fprintf(f, "\n");

    while (1) {
        ypObject *key;
        ypObject *value;
        yp_miniiter_items_next(mi, &mi_state, &key, &value);
        if (yp_isexceptionC2(key, yp_StopIteration)) break;
        assert_not_exception(key);

        if (len > 1) fprintf_indent(f, indent + 1);
        pprint_any(f, indent + 1, key);
        fprintf(f, ": ");
        pprint_any(f, indent + 1, value);
        fprintf(f, ",");
        if (len > 1) fprintf(f, "\n");

        yp_decrefN(N(key, value));
    }

    if (len > 1) fprintf_indent(f, indent);
    fprintf(f, "%s", close);

    yp_decrefN(N(mi));
}

static void pprint_any(FILE *f, int indent, ypObject *obj)
{
    ypObject *type = yp_type(obj);

    if (type == yp_t_invalidated) {
        fprintf(f, "<invalidated object at 0x%p>", obj);

    } else if (type == yp_t_exception) {
        fprintf(f, "<exception object at 0x%p>", obj);

    } else if (type == yp_t_type) {
        fprintf(f, "<type object at 0x%p>", obj);

    } else if (type == yp_t_NoneType) {
        fprintf(f, "None");

    } else if (type == yp_t_bool) {
        fprintf(f, obj == yp_True ? "True" : (obj == yp_False ? "False" : "<unexpected bool>"));

    } else if (type == yp_t_int) {
        assert_not_raises_exc(fprintf(f, "%" PRIint, yp_asintC(obj, &exc)) );

    } else if (type == yp_t_intstore) {
        assert_not_raises_exc(fprintf(f, "intstore(%" PRIint ")", yp_asintC(obj, &exc)));

    } else if (type == yp_t_float) {
        assert_not_raises_exc(fprintf(f, "%f", yp_asfloatC(obj, &exc)));

    } else if (type == yp_t_floatstore) {
        assert_not_raises_exc(fprintf(f, "floatstore(%f)", yp_asfloatC(obj, &exc)));

    } else if (type == yp_t_iter) {
        fprintf(f, "<iter object at 0x%p>", obj);

    } else if (type == yp_t_bytes) {
        fprintf(f, "<bytes object at 0x%p>", obj);

    } else if (type == yp_t_bytearray) {
        fprintf(f, "<bytearray object at 0x%p>", obj);

    } else if (type == yp_t_str) {
        fprintf(f, "<str object at 0x%p>", obj);

    } else if (type == yp_t_chrarray) {
        fprintf(f, "<chrarray object at 0x%p>", obj);

    } else if (type == yp_t_tuple) {
        pprint_sized_iterable(f, indent, "(", ")", "()", obj);

    } else if (type == yp_t_list) {
        pprint_sized_iterable(f, indent, "[", "]", "[]", obj);

    } else if (type == yp_t_frozenset) {
        pprint_sized_iterable(f, indent, "frozenset({", "})", "frozenset()", obj);

    } else if (type == yp_t_set) {
        pprint_sized_iterable(f, indent, "{", "}", "set()", obj);

    } else if (type == yp_t_frozendict) {
        pprint_mapping(f, indent, "frozendict({", "})", "frozendict()", obj);

    } else if (type == yp_t_dict) {
        pprint_mapping(f, indent, "{", "}", "{}", obj);

    } else if (type == yp_t_range) {
        fprintf(f, "<range object at 0x%p>", obj);

    } else if (type == yp_t_function) {
        fprintf(f, "<function object at 0x%p>", obj);

    } else {
        fprintf(f, "<!unknown type! object at 0x%p>", obj);
    }

    yp_decrefN(N(type));
}

void pprint(FILE *f, ypObject *obj)
{
    pprint_any(f, 0, obj);
    fprintf(f, "\n");
}
