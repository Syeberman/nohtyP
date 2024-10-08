"""
yp.py - Python wrapper for nohtyP
    https://github.com/Syeberman/nohtyP   [v0.1.0 $Change$]
    Copyright (c) 2001-2020 Python Software Foundation; All Rights Reserved
    License: http://docs.python.org/3/license.html
"""

# TODO __all__, or underscores

from ctypes import *
from ctypes import _SimpleCData
import functools
import collections
import sys
import weakref
import operator
import os
import pickle
import reprlib
import traceback


def _load_dynamic_library():
    path = os.getenv("NOHTYP_LIBRARY")
    if not path:
        raise ValueError("set NOHTYP_LIBRARY to the path of the nohtyP dynamic library")
    return CDLL(path)

ypdll = _load_dynamic_library()


# From the ctypes documentation...
c_IN = 1
c_OUT = 2
c_INOUT = 3

# There's a limit to the number of args in a ctypes call. ctypes doesn't export this.
CTYPES_MAX_ARGCOUNT = 1024

# Some standard C param/return types
c_void = None  # only valid for return values
c_char_pp = POINTER(c_char_p)

# Used to signal that a particular arg has not been supplied; set to default value of such params
_yp_arg_missing = object()

# Ensures that at most one Python object exists per nohtyP object; make sure new ypObjects are
# given their own references
_yp_pyobj_cache = weakref.WeakValueDictionary()


def _yp_transmute_and_cache(obj, *, steal):
    """If steal is true, this function will steal the nohtyP reference to obj.
    """
    if obj.value is None:
        raise ValueError

    cached = _yp_pyobj_cache.get(obj.value)  # try to use an existing object
    if cached is None:
        if not steal: _yp_incref(obj)  # we need a ref while in the cache
        obj.__class__ = ypObject._yptype2yp[_yp_type(obj).value]
        _yp_pyobj_cache[obj.value] = obj
        return obj
    else:
        if steal: _yp_decref(obj)  # cached already has a ref: we don't need two
        return cached


class yp_param:
    def __init__(self, type, name, default=_yp_arg_missing, direction=c_IN):
        self.type = type
        if default is _yp_arg_missing:
            self.pflag = (direction, name)
        else:
            self.pflag = (direction, name, default)

    def preconvert(self, x):
        if issubclass(self.type, (c_ypObject_p, c_ypObject_pp)):
            return self.type.from_param(x)
        elif issubclass(self.type, _SimpleCData):
            if self.type._type_ in "PzZ":
                return x  # skip pointers
            if isinstance(x, yp_int):
                x = x._asint()
            converted = self.type(x).value
            if converted != x:
                raise OverflowError("overflow in ctypes argument (%r != %r)" % (converted, x))
            return x
        else:
            return x


def yp_func_errcheck(result, func, args):
    if isinstance(result, c_ypObject_p):
        # Returned references are always new; no need to incref
        result = _yp_transmute_and_cache(result, steal=True)
    getattr(result, "_yp_errcheck", int)()
    for arg in args:
        getattr(arg, "_yp_errcheck", int)()
    return result


def yp_func(retval, name, paramtuple, *, errcheck=True):
    """Defines a function in globals() that wraps the given C yp_* function."""
    # Gather all the information that ctypes needs
    params = tuple(yp_param(*x) for x in paramtuple)
    proto = CFUNCTYPE(retval, *(x.type for x in params))
    # XXX ctypes won't allow variable-length arguments if we pass in pflags
    c_func = proto((name, ypdll))
    c_func._yp_name = "_"+name
    if errcheck:
        c_func.errcheck = yp_func_errcheck

    # Create a wrapper function to convert arguments and check for errors (because the way ctypes
    # does it doesn't work well for us...yp_func_errcheck needs the objects _after_ from_param)
    if len(paramtuple) > 0 and paramtuple[-1] is c_multiN_ypObject_p:
        def c_func_wrapper(*args):
            fixed, extra = args[:len(params)-1], args[len(params)-1:]
            converted = list(params[i].preconvert(x) for (i, x) in enumerate(fixed))
            converted.append(len(extra))
            converted.extend(c_ypObject_p.from_param(x) for x in extra)
            return c_func(*converted)  # let c_func.errcheck check for errors

    elif len(paramtuple) > 0 and paramtuple[-1] is c_multiK_ypObject_p:
        def c_func_wrapper(*args):
            fixed, extra = args[:len(params)-1], args[len(params)-1:]
            assert len(extra) % 2 == 0
            converted = list(params[i].preconvert(x) for (i, x) in enumerate(fixed))
            converted.append(len(extra) // 2)
            converted.extend(c_ypObject_p.from_param(x) for x in extra)
            return c_func(*converted)  # let c_func.errcheck check for errors

    else:
        def c_func_wrapper(*args):
            converted = tuple(params[i].preconvert(x) for (i, x) in enumerate(args))
            return c_func(*converted)  # let c_func.errcheck check for errors
    c_func_wrapper.__name__ = c_func._yp_name
    globals()[c_func._yp_name] = c_func_wrapper


# typedef struct _ypObject ypObject;
class c_ypObject_p(c_void_p):
    @classmethod
    def from_param(cls, val):
        if isinstance(val, c_ypObject_p):
            return val
        return ypObject._from_python(val)

    def _yp_errcheck(self): pass

    def __reduce__(self): raise pickle.PicklingError("can't pickle nohtyP types (yet)")


def c_ypObject_p_value(name):
    value = c_ypObject_p.in_dll(ypdll, name)
    value = _yp_transmute_and_cache(value, steal=True)
    globals()[name] = value


class c_ypObject_pp(c_ypObject_p*1):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        _yp_incref(self._item)

    @classmethod
    def from_param(cls, val):
        if isinstance(val, c_ypObject_pp):
            return val
        obj = cls(c_ypObject_p.from_param(val))
        return obj

    @property
    def _item(self): return super().__getitem__(0)

    def _yp_errcheck(self):
        self[0]._yp_errcheck()  # using our __getitem__ here for the transmute

    def __del__(self):
        self[0] = yp_None
        # super().__del__(self)  # no __del__ method?!

    def __getitem__(self, key):
        if key != 0: raise IndexError
        # We own _item's reference, so the cache needs to make its own.
        return _yp_transmute_and_cache(self._item, steal=False)

    def __setitem__(self, key, o):
        if key != 0: raise IndexError
        olditem = self._item
        if olditem.value is not None: _yp_decref(olditem)
        super().__setitem__(0, o)
        _yp_incref(self._item)

    def __reduce__(self): raise pickle.PicklingError("can't pickle nohtyP types (yet)")

# Special-case arguments
c_ypObject_pp_exc = (c_ypObject_pp, "exc", None)
c_multiN_ypObject_p = (c_int, "n", 0)
c_multiK_ypObject_p = (c_int, "n", 0)
assert c_multiN_ypObject_p is not c_multiK_ypObject_p

# ypObject *yp_incref(ypObject *x);
yp_func(c_void_p, "yp_incref", ((c_ypObject_p, "x"), ), errcheck=False)

# void yp_decref(ypObject *x);
yp_func(c_void, "yp_decref", ((c_ypObject_p, "x"), ), errcheck=False)

# int yp_isexceptionC(ypObject *x);
yp_func(c_int, "yp_isexceptionC", ((c_ypObject_p, "x"), ), errcheck=False)

# ypObject *yp_copy(ypObject *x);
yp_func(c_ypObject_p, "yp_copy", ((c_ypObject_p, "x"), ))

# ypObject *yp_deepcopy(ypObject *x);
yp_func(c_ypObject_p, "yp_deepcopy", ((c_ypObject_p, "x"), ))

# ypObject *yp_bool(ypObject *x);
yp_func(c_ypObject_p, "yp_bool", ((c_ypObject_p, "x"), ))

# ypObject *yp_not(ypObject *x);
yp_func(c_ypObject_p, "yp_not", ((c_ypObject_p, "x"), ))

# ypObject *yp_lt(ypObject *x, ypObject *y);
# ypObject *yp_le(ypObject *x, ypObject *y);
# ypObject *yp_eq(ypObject *x, ypObject *y);
# ypObject *yp_ne(ypObject *x, ypObject *y);
# ypObject *yp_ge(ypObject *x, ypObject *y);
# ypObject *yp_gt(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_lt", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
yp_func(c_ypObject_p, "yp_le", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
yp_func(c_ypObject_p, "yp_eq", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
yp_func(c_ypObject_p, "yp_ne", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
yp_func(c_ypObject_p, "yp_ge", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
yp_func(c_ypObject_p, "yp_gt", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))

# typedef float               yp_float32_t;
c_yp_float32_t = c_float
# typedef double              yp_float64_t;
c_yp_float64_t = c_double
# if SIZE_MAX == 0xFFFFFFFFu
# typedef yp_int32_t          yp_ssize_t;
# else
# typedef yp_int64_t          yp_ssize_t;
# endif
c_yp_ssize_t = c_ssize_t
c_yp_ssize_t_p = POINTER(c_yp_ssize_t)
# typedef yp_ssize_t          yp_hash_t;
c_yp_hash_t = c_yp_ssize_t
# define yp_SSIZE_T_MAX ((yp_ssize_t) (SIZE_MAX / 2))
_yp_SSIZE_T_MAX = sys.maxsize
# define yp_SSIZE_T_MIN (-yp_SSIZE_T_MAX - 1)
_yp_SSIZE_T_MIN = -_yp_SSIZE_T_MAX - 1

# typedef yp_int64_t      yp_int_t;
c_yp_int_t = c_int64
# typedef yp_float64_t    yp_float_t;
c_yp_float_t = c_yp_float64_t

# typedef struct _yp_state_decl_t {...} yp_state_decl_t;
class c_yp_state_decl_t(Structure):
    _fields_ = [
        ("size", c_yp_ssize_t),
        ("offsets_len", c_yp_ssize_t),
        ("offsets", c_yp_ssize_t * 0),
    ]

# typedef struct _yp_generator_decl_t {...} yp_generator_decl_t;
# XXX The return needs to be c_void_p to prevent addresses-as-ints being converted to yp_ints
c_yp_generator_func_t = CFUNCTYPE(c_void_p, c_ypObject_p, c_ypObject_p)
class c_yp_generator_decl_t(Structure):
    _fields_ = [
        ("func", c_yp_generator_func_t),
        ("length_hint", c_yp_ssize_t),
        ("state", c_void_p),
        ("state_decl", POINTER(c_yp_state_decl_t)),
    ]

# typedef struct _yp_parameter_decl_t {...} yp_parameter_decl_t;
class c_yp_parameter_decl_t(Structure):
    _fields_ = [
        ("name", c_ypObject_p),
        ("default", c_ypObject_p),
    ]
# typedef struct _yp_function_decl_t {...} yp_function_decl_t;
# XXX The return needs to be c_void_p, else it gets converted to yp_int.
c_yp_function_code_t = CFUNCTYPE(c_void_p, c_ypObject_p, c_yp_ssize_t, POINTER(c_ypObject_p))
class c_yp_function_decl_t(Structure):
    _fields_ = [
        ("code", c_yp_function_code_t),
        ("flags", c_uint32),
        ("parameters_len", c_int32),
        ("parameters", POINTER(c_yp_parameter_decl_t)),
        ("state", c_void_p),
        ("state_decl", POINTER(c_yp_state_decl_t)),
    ]

# ypObject *yp_intC(yp_int_t value);
yp_func(c_ypObject_p, "yp_intC", ((c_yp_int_t, "value"), ))

# ypObject *yp_floatCF(yp_float_t value);
yp_func(c_ypObject_p, "yp_floatCF", ((c_yp_float_t, "value"), ))

# ypObject *yp_iter(ypObject *x);
yp_func(c_ypObject_p, "yp_iter", ((c_ypObject_p, "x"), ))

# ypObject *yp_generatorC(yp_generator_decl_t *declaration);
yp_func(c_ypObject_p, "yp_generatorC", ((POINTER(c_yp_generator_decl_t), "declaration"), ))

# ypObject *yp_rangeC3(yp_int_t start, yp_int_t stop, yp_int_t step);
yp_func(c_ypObject_p, "yp_rangeC3",
        ((c_yp_int_t, "start"), (c_yp_int_t, "stop"), (c_yp_int_t, "step")))

# ypObject *yp_bytesC(yp_ssize_t len, const yp_uint8_t *source);
yp_func(c_ypObject_p, "yp_bytesC", ((c_yp_ssize_t, "len"), (c_char_p, "source")))
# ypObject *yp_bytearrayC(yp_ssize_t len, const yp_uint8_t *source);
yp_func(c_ypObject_p, "yp_bytearrayC", ((c_yp_ssize_t, "len"), (c_char_p, "source")))

# ypObject *yp_str_frombytesC4(yp_ssize_t len, const yp_uint8_t *source,
#         ypObject *encoding, ypObject *errors);
yp_func(c_ypObject_p, "yp_str_frombytesC4", ((c_yp_ssize_t, "len"), (c_char_p, "source"),
                                             (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")))

# ypObject *yp_str_frombytesC2(yp_ssize_t len, const yp_uint8_t *source);
yp_func(c_ypObject_p, "yp_str_frombytesC2", ((c_yp_ssize_t, "len"), (c_char_p, "source")))

# ypObject *yp_str3(ypObject *object, ypObject *encoding, ypObject *errors);
yp_func(c_ypObject_p, "yp_str3", ((c_ypObject_p, "object"),
                                  (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")))

# ypObject *yp_tupleN(int n, ...);
yp_func(c_ypObject_p, "yp_tupleN", (c_multiN_ypObject_p, ))

# ypObject *yp_listN(int n, ...);
yp_func(c_ypObject_p, "yp_listN", (c_multiN_ypObject_p, ))

# ypObject *yp_tuple(ypObject *iterable);
yp_func(c_ypObject_p, "yp_tuple", ((c_ypObject_p, "iterable"), ))
# ypObject *yp_list(ypObject *iterable);
yp_func(c_ypObject_p, "yp_list", ((c_ypObject_p, "iterable"), ))

# ypObject *yp_frozensetN(int n, ...);
yp_func(c_ypObject_p, "yp_frozensetN", (c_multiN_ypObject_p, ))
# ypObject *yp_setN(int n, ...);
yp_func(c_ypObject_p, "yp_setN", (c_multiN_ypObject_p, ))

# ypObject *yp_frozenset(ypObject *iterable);
yp_func(c_ypObject_p, "yp_frozenset", ((c_ypObject_p, "iterable"), ))
# ypObject *yp_set(ypObject *iterable);
yp_func(c_ypObject_p, "yp_set", ((c_ypObject_p, "iterable"), ))

# ypObject *yp_dictK(int n, ...);
yp_func(c_ypObject_p, "yp_dictK", (c_multiK_ypObject_p, ))

# ypObject *yp_frozendict_fromkeysN(ypObject *value, int n, ...);
yp_func(c_ypObject_p, "yp_frozendict_fromkeysN", ((c_ypObject_p, "value"), c_multiN_ypObject_p))
# ypObject *yp_dict_fromkeysN(ypObject *value, int n, ...);
yp_func(c_ypObject_p, "yp_dict_fromkeysN", ((c_ypObject_p, "value"), c_multiN_ypObject_p))

# ypObject *yp_frozendict(ypObject *x);
yp_func(c_ypObject_p, "yp_frozendict", ((c_ypObject_p, "x"), ))
# ypObject *yp_dict(ypObject *x);
yp_func(c_ypObject_p, "yp_dict", ((c_ypObject_p, "x"), ))

# ypObject *yp_functionC(yp_function_decl_t *declaration);
yp_func(c_ypObject_p, "yp_functionC", ((POINTER(c_yp_function_decl_t), "declaration"), ))

# yp_hash_t yp_hashC(ypObject *x, ypObject **exc);
yp_func(c_yp_hash_t, "yp_hashC", ((c_ypObject_p, "x"), c_ypObject_pp_exc))

# ypObject *yp_next(ypObject *iterator);
yp_func(c_ypObject_p, "yp_next", ((c_ypObject_p, "iterator"), ))

# yp_ssize_t yp_length_hintC(ypObject *iterator, ypObject **exc);
yp_func(c_yp_ssize_t, "yp_length_hintC", ((c_ypObject_p, "iterator"), c_ypObject_pp_exc))

# ypObject *yp_reversed(ypObject *seq);
yp_func(c_ypObject_p, "yp_reversed", ((c_ypObject_p, "seq"), ))

# ypObject *yp_contains(ypObject *container, ypObject *x);
yp_func(c_ypObject_p, "yp_contains", ((c_ypObject_p, "container"), (c_ypObject_p, "x")))

# yp_ssize_t yp_lenC(ypObject *container, ypObject **exc);
yp_func(c_yp_ssize_t, "yp_lenC", ((c_ypObject_p, "container"), c_ypObject_pp_exc),
        errcheck=False)

# void yp_push(ypObject *container, ypObject *x, ypObject **exc);
yp_func(c_void, "yp_push", ((c_ypObject_p, "container"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# void yp_clear(ypObject *container, ypObject **exc);
yp_func(c_void, "yp_clear", ((c_ypObject_p, "container"), c_ypObject_pp_exc))

# ypObject *yp_pop(ypObject *container);
yp_func(c_ypObject_p, "yp_pop", ((c_ypObject_p, "container"), ))

# ypObject *yp_concat(ypObject *sequence, ypObject *x);
yp_func(c_ypObject_p, "yp_concat", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x")))

# ypObject *yp_repeatC(ypObject *sequence, yp_ssize_t factor);
yp_func(c_ypObject_p, "yp_repeatC", ((c_ypObject_p, "sequence"), (c_yp_ssize_t, "factor")))

# ypObject *yp_getindexC(ypObject *sequence, yp_ssize_t i);
yp_func(c_ypObject_p, "yp_getindexC", ((c_ypObject_p, "sequence"), (c_yp_ssize_t, "i")))

# ypObject *yp_getsliceC4(ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k);
yp_func(c_ypObject_p, "yp_getsliceC4", ((c_ypObject_p, "sequence"),
                                        (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), (c_yp_ssize_t, "k")))

# yp_ssize_t yp_findC5(ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
#         ypObject **exc);
yp_func(c_yp_ssize_t, "yp_findC5", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
                                    (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), c_ypObject_pp_exc))

# yp_ssize_t yp_findC(ypObject *sequence, ypObject *x, ypObject **exc);
yp_func(c_yp_ssize_t, "yp_findC", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
                                   c_ypObject_pp_exc))

# yp_ssize_t yp_indexC5(ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
#         ypObject **exc);
yp_func(c_yp_ssize_t, "yp_indexC5", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
                                     (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), c_ypObject_pp_exc))
# yp_ssize_t yp_indexC(ypObject *sequence, ypObject *x, ypObject **exc);
yp_func(c_yp_ssize_t, "yp_indexC", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
                                    c_ypObject_pp_exc))

# yp_ssize_t yp_rfindC5(ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
#         ypObject **exc);
yp_func(c_yp_ssize_t, "yp_rfindC5", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
                                     (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), c_ypObject_pp_exc))
# yp_ssize_t yp_rfindC(ypObject *sequence, ypObject *x, ypObject **exc);
yp_func(c_yp_ssize_t, "yp_rfindC", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
                                    c_ypObject_pp_exc))
# yp_ssize_t yp_rindexC5(ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
#         ypObject **exc);
yp_func(c_yp_ssize_t, "yp_rindexC5", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
                                      (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), c_ypObject_pp_exc))
# yp_ssize_t yp_rindexC(ypObject *sequence, ypObject *x, ypObject **exc);
yp_func(c_yp_ssize_t, "yp_rindexC", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
                                     c_ypObject_pp_exc))

# yp_ssize_t yp_countC5(ypObject *sequence, ypObject *x, yp_ssize_t i, yp_ssize_t j,
#        ypObject **exc);
yp_func(c_yp_ssize_t, "yp_countC5", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
                                     (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), c_ypObject_pp_exc))

# yp_ssize_t yp_countC(ypObject *sequence, ypObject *x, ypObject **exc);
yp_func(c_yp_ssize_t, "yp_countC", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"),
                                    c_ypObject_pp_exc))

# void yp_setsliceC6(ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k, ypObject *x,
#         ypObject **exc);
yp_func(c_void, "yp_setsliceC6", ((c_ypObject_p, "sequence"),
                                  (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), (c_yp_ssize_t, "k"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# void yp_delsliceC5(
#         ypObject *sequence, yp_ssize_t i, yp_ssize_t j, yp_ssize_t k, ypObject **exc);
yp_func(c_void, "yp_delsliceC5", ((c_ypObject_p, "sequence"),
                                  (c_yp_ssize_t, "i"), (c_yp_ssize_t, "j"), (c_yp_ssize_t, "k"), c_ypObject_pp_exc))

# void yp_append(ypObject *sequence, ypObject *x, ypObject **exc);
yp_func(c_void, "yp_append", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# void yp_extend(ypObject *sequence, ypObject *t, ypObject **exc);
yp_func(c_void, "yp_extend", ((c_ypObject_p, "sequence"), (c_ypObject_p, "t"), c_ypObject_pp_exc))

# void yp_irepeatC(ypObject *sequence, yp_ssize_t factor, ypObject **exc);
yp_func(c_void, "yp_irepeatC", ((c_ypObject_p, "sequence"), (c_yp_ssize_t, "factor"), c_ypObject_pp_exc))

# void yp_insertC(ypObject *sequence, yp_ssize_t i, ypObject *x, ypObject **exc);
yp_func(c_void, "yp_insertC", ((c_ypObject_p, "sequence"),
                               (c_yp_ssize_t, "i"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# ypObject *yp_popindexC(ypObject *sequence, yp_ssize_t i);
yp_func(c_ypObject_p, "yp_popindexC", ((c_ypObject_p, "sequence"), (c_yp_ssize_t, "i")))

# void yp_remove(ypObject *sequence, ypObject *x, ypObject **exc);
yp_func(c_void, "yp_remove", ((c_ypObject_p, "sequence"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# void yp_reverse(ypObject *sequence, ypObject **exc);
yp_func(c_void, "yp_reverse", ((c_ypObject_p, "sequence"), c_ypObject_pp_exc))

# void yp_sort4(ypObject *sequence, ypObject *key, ypObject *reverse, ypObject **exc);
yp_func(
    c_void, "yp_sort4",
    ((c_ypObject_p, "sequence"), (c_ypObject_p, "key"), (c_ypObject_p, "reverse"), c_ypObject_pp_exc)
)

# define yp_SLICE_DEFAULT yp_SSIZE_T_MIN
_yp_SLICE_DEFAULT = _yp_SSIZE_T_MIN
# define yp_SLICE_LAST  yp_SSIZE_T_MAX
_yp_SLICE_LAST = _yp_SSIZE_T_MAX

# ypObject *yp_isdisjoint(ypObject *set, ypObject *x);
yp_func(c_ypObject_p, "yp_isdisjoint", ((c_ypObject_p, "set"), (c_ypObject_p, "x")))

# ypObject *yp_issubset(ypObject *set, ypObject *x);
yp_func(c_ypObject_p, "yp_issubset", ((c_ypObject_p, "set"), (c_ypObject_p, "x")))

# ypObject *yp_issuperset(ypObject *set, ypObject *x);
yp_func(c_ypObject_p, "yp_issuperset", ((c_ypObject_p, "set"), (c_ypObject_p, "x")))

# ypObject *yp_union(ypObject *set, ypObject *x);
yp_func(c_ypObject_p, "yp_union", ((c_ypObject_p, "set"), (c_ypObject_p, "x")))

# ypObject *yp_intersection(ypObject *set, ypObject *x);
yp_func(c_ypObject_p, "yp_intersection", ((c_ypObject_p, "set"), (c_ypObject_p, "x")))

# ypObject *yp_difference(ypObject *set, ypObject *x);
yp_func(c_ypObject_p, "yp_difference", ((c_ypObject_p, "set"), (c_ypObject_p, "x")))

# ypObject *yp_symmetric_difference(ypObject *set, ypObject *x);
yp_func(c_ypObject_p, "yp_symmetric_difference", ((c_ypObject_p, "set"), (c_ypObject_p, "x")))

# void yp_update(ypObject *set, ypObject *x, ypObject **exc);
yp_func(c_void, "yp_update", ((c_ypObject_p, "set"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# void yp_intersection_update(ypObject *set, ypObject *x, ypObject **exc);
yp_func(c_void, "yp_intersection_update", ((c_ypObject_p, "set"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# void yp_difference_update(ypObject *set, ypObject *x, ypObject **exc);
yp_func(c_void, "yp_difference_update", ((c_ypObject_p, "set"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# void yp_symmetric_difference_update(ypObject *set, ypObject *x, ypObject **exc);
yp_func(c_void, "yp_symmetric_difference_update", ((c_ypObject_p, "set"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# void yp_pushunique(ypObject *set, ypObject *x, ypObject **exc);
yp_func(c_void, "yp_pushunique", ((c_ypObject_p, "set"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# void yp_discard(ypObject *set, ypObject *x, ypObject **exc);
yp_func(c_void, "yp_discard", ((c_ypObject_p, "set"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# ypObject *yp_getitem(ypObject *mapping, ypObject *key);
yp_func(c_ypObject_p, "yp_getitem", ((c_ypObject_p, "mapping"), (c_ypObject_p, "key")))

# void yp_setitem(ypObject *mapping, ypObject *key, ypObject *x, ypObject **exc);
yp_func(c_void, "yp_setitem",
        ((c_ypObject_p, "mapping"), (c_ypObject_p, "key"), (c_ypObject_p, "x"), c_ypObject_pp_exc))

# void yp_delitem(ypObject *mapping, ypObject *key, ypObject **exc);
yp_func(c_void, "yp_delitem", ((c_ypObject_p, "mapping"), (c_ypObject_p, "key"), c_ypObject_pp_exc))

# ypObject *yp_getdefault(ypObject *mapping, ypObject *key, ypObject *default_);
yp_func(c_ypObject_p, "yp_getdefault",
        ((c_ypObject_p, "mapping"), (c_ypObject_p, "key"), (c_ypObject_p, "default_")))

# ypObject *yp_iter_items(ypObject *mapping);
yp_func(c_ypObject_p, "yp_iter_items", ((c_ypObject_p, "mapping"), ))

# ypObject *yp_iter_keys(ypObject *mapping);
yp_func(c_ypObject_p, "yp_iter_keys", ((c_ypObject_p, "mapping"), ))

# ypObject *yp_popvalue2(ypObject *mapping, ypObject *key);
yp_func(c_ypObject_p, "yp_popvalue2", ((c_ypObject_p, "mapping"), (c_ypObject_p, "key")))

# ypObject *yp_popvalue3(ypObject *mapping, ypObject *key, ypObject *default_);
yp_func(c_ypObject_p, "yp_popvalue3", ((c_ypObject_p, "mapping"), (c_ypObject_p, "key"),
                                       (c_ypObject_p, "default_")))

# void yp_popitem(ypObject *mapping, ypObject **key, ypObject **value);
yp_func(c_void, "yp_popitem", ((c_ypObject_p, "mapping"), (c_ypObject_pp, "key"),
                               (c_ypObject_pp, "value")))

# ypObject *yp_setdefault(ypObject *mapping, ypObject *key, ypObject *default_);
yp_func(c_ypObject_p, "yp_setdefault",
        ((c_ypObject_p, "mapping"), (c_ypObject_p, "key"), (c_ypObject_p, "default_")))

# void yp_updateK(ypObject *mapping, ypObject **exc, int n, ...);
yp_func(c_void, "yp_updateK", ((c_ypObject_p, "mapping"), c_ypObject_pp_exc, c_multiK_ypObject_p))

# ypObject *yp_iter_values(ypObject *mapping);
yp_func(c_ypObject_p, "yp_iter_values", ((c_ypObject_p, "mapping"), ))

# ypObject *yp_isalnum(ypObject *s);
yp_func(c_ypObject_p, "yp_isalnum", ((c_ypObject_p, "s"), ))

# ypObject *yp_isalpha(ypObject *s);
yp_func(c_ypObject_p, "yp_isalpha", ((c_ypObject_p, "s"), ))

# ypObject *yp_isdecimal(ypObject *s);
yp_func(c_ypObject_p, "yp_isdecimal", ((c_ypObject_p, "s"), ))

# ypObject *yp_isdigit(ypObject *s);
yp_func(c_ypObject_p, "yp_isdigit", ((c_ypObject_p, "s"), ))

# ypObject *yp_isidentifier(ypObject *s);
yp_func(c_ypObject_p, "yp_isidentifier", ((c_ypObject_p, "s"), ))

# ypObject *yp_islower(ypObject *s);
yp_func(c_ypObject_p, "yp_islower", ((c_ypObject_p, "s"), ))

# ypObject *yp_isnumeric(ypObject *s);
yp_func(c_ypObject_p, "yp_isnumeric", ((c_ypObject_p, "s"), ))

# ypObject *yp_isprintable(ypObject *s);
yp_func(c_ypObject_p, "yp_isprintable", ((c_ypObject_p, "s"), ))

# ypObject *yp_isspace(ypObject *s);
yp_func(c_ypObject_p, "yp_isspace", ((c_ypObject_p, "s"), ))

# ypObject *yp_isupper(ypObject *s);
yp_func(c_ypObject_p, "yp_isupper", ((c_ypObject_p, "s"), ))

# ypObject *yp_startswithC4(ypObject *s, ypObject *prefix, yp_ssize_t start, yp_ssize_t end);
# ypObject *yp_startswith(ypObject *s, ypObject *prefix);
yp_func(c_ypObject_p, "yp_startswithC4", ((c_ypObject_p, "s"), (c_ypObject_p, "prefix"),
                                          (c_yp_ssize_t, "start"), (c_yp_ssize_t, "end")))
yp_func(c_ypObject_p, "yp_startswith", ((c_ypObject_p, "s"), (c_ypObject_p, "prefix")))

# ypObject *yp_endswithC4(ypObject *s, ypObject *suffix, yp_ssize_t start, yp_ssize_t end);
# ypObject *yp_endswith(ypObject *s, ypObject *suffix);
yp_func(c_ypObject_p, "yp_endswithC4", ((c_ypObject_p, "s"), (c_ypObject_p, "suffix"),
                                        (c_yp_ssize_t, "start"), (c_yp_ssize_t, "end")))
yp_func(c_ypObject_p, "yp_endswith", ((c_ypObject_p, "s"), (c_ypObject_p, "suffix")))

# ypObject *yp_lower(ypObject *s);
yp_func(c_ypObject_p, "yp_lower", ((c_ypObject_p, "s"), ))

# ypObject *yp_upper(ypObject *s);
yp_func(c_ypObject_p, "yp_upper", ((c_ypObject_p, "s"), ))

# ypObject *yp_casefold(ypObject *s);
yp_func(c_ypObject_p, "yp_casefold", ((c_ypObject_p, "s"), ))

# ypObject *yp_swapcase(ypObject *s);
yp_func(c_ypObject_p, "yp_swapcase", ((c_ypObject_p, "s"), ))

# ypObject *yp_capitalize(ypObject *s);
yp_func(c_ypObject_p, "yp_capitalize", ((c_ypObject_p, "s"), ))

# ypObject *yp_ljustC3(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar);
# ypObject *yp_ljustC(ypObject *s, yp_ssize_t width);
yp_func(c_ypObject_p, "yp_ljustC3", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width"),
                                     (c_yp_int_t, "ord_fillchar")))
yp_func(c_ypObject_p, "yp_ljustC", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width")))

# ypObject *yp_rjustC3(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar);
# ypObject *yp_rjustC(ypObject *s, yp_ssize_t width);
yp_func(c_ypObject_p, "yp_rjustC3", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width"),
                                     (c_yp_int_t, "ord_fillchar")))
yp_func(c_ypObject_p, "yp_rjustC", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width")))

# ypObject *yp_centerC3(ypObject *s, yp_ssize_t width, yp_int_t ord_fillchar);
# ypObject *yp_centerC(ypObject *s, yp_ssize_t width);
yp_func(c_ypObject_p, "yp_centerC3", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width"),
                                      (c_yp_int_t, "ord_fillchar")))
yp_func(c_ypObject_p, "yp_centerC", ((c_ypObject_p, "s"), (c_yp_ssize_t, "width")))

# ypObject *yp_expandtabsC(ypObject *s, yp_ssize_t tabsize);
yp_func(c_ypObject_p, "yp_expandtabsC", ((c_ypObject_p, "s"), (c_yp_ssize_t, "tabsize")))

# ypObject *yp_replaceC4(ypObject *s, ypObject *oldsub, ypObject *newsub, yp_ssize_t count);
# ypObject *yp_replace(ypObject *s, ypObject *oldsub, ypObject *newsub);
yp_func(c_ypObject_p, "yp_replaceC4", ((c_ypObject_p, "s"),
                                       (c_ypObject_p, "oldsub"), (c_ypObject_p, "newsub"), (c_yp_ssize_t, "count")))
yp_func(c_ypObject_p, "yp_replace", ((c_ypObject_p, "s"),
                                     (c_ypObject_p, "oldsub"), (c_ypObject_p, "newsub")))

# ypObject *yp_lstrip2(ypObject *s, ypObject *chars);
# ypObject *yp_lstrip(ypObject *s);
yp_func(c_ypObject_p, "yp_lstrip2", ((c_ypObject_p, "s"), (c_ypObject_p, "chars")))
yp_func(c_ypObject_p, "yp_lstrip", ((c_ypObject_p, "s"), ))

# ypObject *yp_rstrip2(ypObject *s, ypObject *chars);
# ypObject *yp_rstrip(ypObject *s);
yp_func(c_ypObject_p, "yp_rstrip2", ((c_ypObject_p, "s"), (c_ypObject_p, "chars")))
yp_func(c_ypObject_p, "yp_rstrip", ((c_ypObject_p, "s"), ))

# ypObject *yp_strip2(ypObject *s, ypObject *chars);
# ypObject *yp_strip(ypObject *s);
yp_func(c_ypObject_p, "yp_strip2", ((c_ypObject_p, "s"), (c_ypObject_p, "chars")))
yp_func(c_ypObject_p, "yp_strip", ((c_ypObject_p, "s"), ))

# ypObject *yp_join(ypObject *s, ypObject *iterable);
yp_func(c_ypObject_p, "yp_join", ((c_ypObject_p, "s"), (c_ypObject_p, "iterable")))

# ypObject *yp_joinN(ypObject *s, int n, ...);
# ypObject *yp_joinNV(ypObject *s, int n, va_list args);
yp_func(c_ypObject_p, "yp_joinN", ((c_ypObject_p, "s"), c_multiN_ypObject_p))

# ypObject *yp_splitC3(ypObject *s, ypObject *sep, yp_ssize_t maxsplit);
# ypObject *yp_split2(ypObject *s, ypObject *sep);
yp_func(c_ypObject_p, "yp_splitC3", ((c_ypObject_p, "s"), (c_ypObject_p, "sep"),
                                     (c_yp_ssize_t, "maxsplit")))
yp_func(c_ypObject_p, "yp_split2", ((c_ypObject_p, "s"), (c_ypObject_p, "sep")))

# ypObject *yp_split(ypObject *s);
yp_func(c_ypObject_p, "yp_split", ((c_ypObject_p, "s"), ))

# ypObject *yp_rsplitC3(ypObject *s, ypObject *sep, yp_ssize_t maxsplit);
yp_func(c_ypObject_p, "yp_rsplitC3", ((c_ypObject_p, "s"), (c_ypObject_p, "sep"),
                                      (c_yp_ssize_t, "maxsplit")))

# ypObject *yp_splitlines2(ypObject *s, ypObject *keepends);
yp_func(c_ypObject_p, "yp_splitlines2", ((c_ypObject_p, "s"), (c_ypObject_p, "keepends")))

# ypObject *yp_encode3(ypObject *s, ypObject *encoding, ypObject *errors);
# ypObject *yp_encode(ypObject *s);
yp_func(c_ypObject_p, "yp_encode3", ((c_ypObject_p, "s"),
                                     (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")))
yp_func(c_ypObject_p, "yp_encode", ((c_ypObject_p, "s"), ))

# ypObject *yp_decode3(ypObject *b, ypObject *encoding, ypObject *errors);
# ypObject *yp_decode(ypObject *b);
yp_func(c_ypObject_p, "yp_decode3", ((c_ypObject_p, "b"),
                                     (c_ypObject_p, "encoding"), (c_ypObject_p, "errors")))
yp_func(c_ypObject_p, "yp_decode", ((c_ypObject_p, "b"), ))

# ypObject *yp_callN(ypObject *c, int n, ...);
yp_func(c_ypObject_p, "yp_callN", ((c_ypObject_p, "c"), c_multiN_ypObject_p))

# ypObject *yp_call_stars(ypObject *c, ypObject *args, ypObject *kwargs);
yp_func(c_ypObject_p, "yp_call_stars", ((c_ypObject_p, "c"),
                                        (c_ypObject_p, "args"), (c_ypObject_p, "kwargs")))

# ypObject *yp_add(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_add", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# ypObject *yp_sub(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_sub", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# ypObject *yp_mul(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_mul", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# ypObject *yp_truediv(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_truediv", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# ypObject *yp_floordiv(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_floordiv", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# ypObject *yp_mod(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_mod", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# void yp_divmod(ypObject *x, ypObject *y, ypObject **div, ypObject **mod);
yp_func(c_void, "yp_divmod", ((c_ypObject_p, "x"), (c_ypObject_p, "y"),
                              (c_ypObject_pp, "div"), (c_ypObject_pp, "mod")))
# ypObject *yp_pow(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_pow", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# ypObject *yp_pow3(ypObject *x, ypObject *y, ypObject *z);
# ypObject *yp_lshift(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_lshift", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# ypObject *yp_rshift(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_rshift", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# ypObject *yp_amp(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_amp", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# ypObject *yp_xor(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_xor", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# ypObject *yp_bar(ypObject *x, ypObject *y);
yp_func(c_ypObject_p, "yp_bar", ((c_ypObject_p, "x"), (c_ypObject_p, "y")))
# ypObject *yp_neg(ypObject *x);
yp_func(c_ypObject_p, "yp_neg", ((c_ypObject_p, "x"), ))
# ypObject *yp_pos(ypObject *x);
yp_func(c_ypObject_p, "yp_pos", ((c_ypObject_p, "x"), ))
# ypObject *yp_abs(ypObject *x);
yp_func(c_ypObject_p, "yp_abs", ((c_ypObject_p, "x"), ))
# ypObject *yp_invert(ypObject *x);
yp_func(c_ypObject_p, "yp_invert", ((c_ypObject_p, "x"), ))

# yp_int_t yp_asintC(ypObject *x, ypObject **exc);
yp_func(c_yp_int_t, "yp_asintC", ((c_ypObject_p, "x"), c_ypObject_pp_exc))
# yp_float_t yp_asfloatC(ypObject *x, ypObject **exc);
yp_func(c_yp_float_t, "yp_asfloatC", ((c_ypObject_p, "x"), c_ypObject_pp_exc))

# yp_int_t yp_bit_lengthC(ypObject *x, ypObject **exc);
yp_func(c_yp_int_t, "yp_bit_lengthC", ((c_ypObject_p, "x"), c_ypObject_pp_exc))

# ypObject *yp_type(ypObject *object);
yp_func(c_ypObject_p, "yp_type", ((c_ypObject_p, "object"), ))

# ypObject *yp_asbytesCX(ypObject *seq, yp_ssize_t *len, const yp_uint8_t * *bytes);
yp_func(c_ypObject_p, "yp_asbytesCX", ((c_ypObject_p, "seq"),
                                       (c_yp_ssize_t_p, "len"), (c_char_pp, "bytes")), errcheck=False)

# ypObject *yp_asencodedCX(ypObject *seq, yp_ssize_t *size, const yp_uint8_t * *encoded,
#        ypObject * *encoding);
yp_func(c_ypObject_p, "yp_asencodedCX", ((c_ypObject_p, "seq"),
                                         (c_yp_ssize_t_p, "size"), (c_char_pp, "encoded"), (c_ypObject_pp, "encoding")),
        errcheck=False)


_ypExc2py = {}
_pyExc2yp = {}


def ypObject_p_exception(name, pyExc, *, one_to_one=True):
    """Use one_to_one=False when pyExc should not be mapped back to the nohtyP exception."""
    ypExc = c_ypObject_p.in_dll(ypdll, name)
    _ypExc2py[ypExc.value] = (name, pyExc)
    if one_to_one:
        _pyExc2yp[pyExc] = ypExc
    globals()["_"+name] = ypExc

ypObject_p_exception("yp_BaseException", BaseException)
ypObject_p_exception("yp_SystemExit", SystemExit)
ypObject_p_exception("yp_KeyboardInterrupt", KeyboardInterrupt)
ypObject_p_exception("yp_GeneratorExit", GeneratorExit)
ypObject_p_exception("yp_Exception", Exception)
ypObject_p_exception("yp_StopIteration", StopIteration)
ypObject_p_exception("yp_ArithmeticError", ArithmeticError)
ypObject_p_exception("yp_FloatingPointError", FloatingPointError)
ypObject_p_exception("yp_OverflowError", OverflowError)
ypObject_p_exception("yp_ZeroDivisionError", ZeroDivisionError)
ypObject_p_exception("yp_AssertionError", AssertionError)
ypObject_p_exception("yp_AttributeError", AttributeError)
ypObject_p_exception("yp_BufferError", BufferError)
ypObject_p_exception("yp_EOFError", EOFError)
ypObject_p_exception("yp_ImportError", ImportError)
ypObject_p_exception("yp_LookupError", LookupError)
ypObject_p_exception("yp_IndexError", IndexError)
ypObject_p_exception("yp_KeyError", KeyError)
ypObject_p_exception("yp_MemoryError", MemoryError)
ypObject_p_exception("yp_NameError", NameError)
ypObject_p_exception("yp_UnboundLocalError", UnboundLocalError)
ypObject_p_exception("yp_OSError", OSError)
ypObject_p_exception("yp_ReferenceError", ReferenceError)
ypObject_p_exception("yp_RuntimeError", RuntimeError)
ypObject_p_exception("yp_NotImplementedError", NotImplementedError)
ypObject_p_exception("yp_SyntaxError", SyntaxError)
ypObject_p_exception("yp_SystemError", SystemError)
ypObject_p_exception("yp_TypeError", TypeError)
ypObject_p_exception("yp_ValueError", ValueError)
ypObject_p_exception("yp_UnicodeError", UnicodeError)
ypObject_p_exception("yp_UnicodeEncodeError", UnicodeEncodeError)
ypObject_p_exception("yp_UnicodeDecodeError", UnicodeDecodeError)
ypObject_p_exception("yp_UnicodeTranslateError", UnicodeTranslateError)

# Raised when the object does not support the given method; subexception of yp_AttributeError
ypObject_p_exception("yp_MethodError", AttributeError, one_to_one=False)
# Raised when an allocation size calculation overflows; subexception of yp_MemoryError
ypObject_p_exception("yp_MemorySizeOverflowError", MemoryError, one_to_one=False)
# Raised on an error in a function's parameters definition; subexception of yp_SyntaxError.
ypObject_p_exception("yp_ParameterSyntaxError", SyntaxError, one_to_one=False)
# Indicates a limitation in the implementation of nohtyP; subexception of yp_SystemError
ypObject_p_exception("yp_SystemLimitationError", SystemError, one_to_one=False)
# Raised when an invalidated object is passed to a function; subexception of yp_TypeError
ypObject_p_exception("yp_InvalidatedError", TypeError, one_to_one=False)

# int yp_isexceptionC2(ypObject *x, ypObject *exc);
yp_func(c_int, "yp_isexceptionC2", ((c_ypObject_p, "x"), (c_ypObject_p, "exc")))

# int yp_isexceptionCN(ypObject *x, int n, ...);

# typedef struct _yp_initialize_parameters_t {...} yp_initialize_parameters_t;
c_yp_malloc_func_t = CFUNCTYPE(c_void_p, c_yp_ssize_t_p, c_yp_ssize_t)
c_yp_malloc_resize_func_t = CFUNCTYPE(c_void_p, c_yp_ssize_t_p, c_void_p, c_yp_ssize_t, c_yp_ssize_t)
c_yp_free_func_t = CFUNCTYPE(c_void, c_void_p)
class c_yp_initialize_parameters_t(Structure):
    _fields_ = [
        ("struct_size", c_yp_ssize_t),
        ("yp_malloc", c_yp_malloc_func_t),
        ("yp_malloc_resize", c_yp_malloc_resize_func_t),
        ("yp_free", c_yp_free_func_t),
        ("everything_immortal", c_int),
    ]

# void yp_initialize(yp_initialize_parameters_t *kwparams);
yp_func(c_void, "yp_initialize", ((POINTER(c_yp_initialize_parameters_t), "kwparams"), ),
        errcheck=False)

# void *yp_mem_default_malloc(yp_ssize_t *actual, yp_ssize_t size);
_yp_mem_default_malloc = c_yp_malloc_func_t(("yp_mem_default_malloc", ypdll))
# void *yp_mem_default_malloc_resize(yp_ssize_t *actual, void *p, yp_ssize_t size, yp_ssize_t extra);
_yp_mem_default_malloc_resize = c_yp_malloc_resize_func_t(("yp_mem_default_malloc_resize", ypdll))
# void yp_mem_default_free(void *p);
_yp_mem_default_free = c_yp_free_func_t(("yp_mem_default_free", ypdll))


# Some nohtyP objects need to hold references to Python objects; in particular, yp_iter and
# yp_function must hold references to ctypes callback functions. References here are discarded after
# the associated nohtyP object is discarded (see yp_free_hook).
_yp_reverse_refs = collections.defaultdict(list)

@c_yp_free_func_t
def yp_free_hook(p):
    _yp_mem_default_free(p)
    _yp_reverse_refs.pop(p, None)


# Initialize nohtyP
_yp_initparams = c_yp_initialize_parameters_t(
    struct_size=sizeof(c_yp_initialize_parameters_t),
    yp_malloc=_yp_mem_default_malloc,
    yp_malloc_resize=_yp_mem_default_malloc_resize,
    yp_free=yp_free_hook,
    everything_immortal=False
)
_yp_initialize(_yp_initparams)


class ypObject(c_ypObject_p):
    def __new__(cls, *args, **kwargs):
        if cls is ypObject:
            raise NotImplementedError("can't instantiate ypObject directly")

        return _yp_call_stars(cls._yp_type, args, kwargs)

    def __init__(self, *args, **kwargs): pass

    def __del__(self):
        # TODO It seems that _yp_decref and yp_None gets set to None when Python is closing:
        # "Python guarantees that globals whose name begins with a single underscore are deleted
        # from their module before other globals are deleted"
        # TODO Why is self.value sometimes None (ie a null pointer)?  Should never happen.
        try:
            if self.value is not None:
                _yp_decref(self)
        except:
            pass
        return  # TODO Causing a Segmentation Fault sometimes?!?!
        try:
            self.value = yp_None.value
        except:
            pass
    _pytype2yp = {}
    _yptype2yp = {}

    @classmethod
    def _from_python(cls, pyobj, *, default=None):
        """ypObject._from_python is a factory that returns the correct yp_* object based on the type
        of pyobj. All other _from_python class methods always return that exact type. If pyobj isn't
        a known type then default is used (yp_iter, perhaps?).
        """
        if cls is not ypObject:
            raise NotImplementedError(f'{cls.__name__}._from_python')
        if pyobj is None:
            return yp_None
        if isinstance(pyobj, ypObject):
            return pyobj
        if isinstance(pyobj, type) and issubclass(pyobj, ypObject):
            return pyobj._yp_type
        subcls = ypObject._pytype2yp.get(type(pyobj), default)
        if subcls is None:
            raise NotImplementedError(f'no converters for {pyobj.__class__}')
        return subcls._from_python(pyobj)

    # __str__ and __repr__ must always return a Python str, but we want nohtyP-aware code to be
    # able to get the original yp_str object via _yp_str/_yp_repr
    def __str__(self): return str(self._yp_str())

    def __repr__(self): return str(self._yp_repr())

    def copy(self): return _yp_copy(self)

    def __copy__(self): return _yp_copy(self)

    def __deepcopy__(self, memo): return _yp_deepcopy(self)

    # TODO will this work if yp_bool returns an exception?
    def __bool__(self): return bool(yp_bool(self))

    def __lt__(self, other): return _yp_lt(self, other)

    def __le__(self, other): return _yp_le(self, other)

    def __eq__(self, other): return _yp_eq(self, other)

    def __ne__(self, other): return _yp_ne(self, other)

    def __ge__(self, other): return _yp_ge(self, other)

    def __gt__(self, other): return _yp_gt(self, other)

    def __iter__(self): return _yp_iter(self)

    def __hash__(self): return _yp_hashC(self, yp_None)

    def __next__(self): return _yp_next(self)

    def __reversed__(self): return _yp_reversed(self)

    def __contains__(self, x): return _yp_contains(self, x)

    def __len__(self):
        # errcheck disabled for _yp_lenC, so do it here
        exc = c_ypObject_pp(yp_None)
        result = _yp_lenC(self, exc)
        exc._yp_errcheck()
        return result

    def push(self, x): _yp_push(self, x, yp_None)

    def clear(self): _yp_clear(self, yp_None)

    def pop(self): return _yp_pop(self)

    def _sliceSearch(self, func2, func4, x, i, j, *extra):
        if i is None and j is None:
            return func2(self, x, *extra)
        if i is None:
            i = 0
        if j is None:
            j = _yp_SLICE_LAST
        return func4(self, x, i, j, *extra)

    def find(self, x, i=None, j=None):
        return yp_int(self._sliceSearch(_yp_findC, _yp_findC5, x, i, j, yp_None))

    def index(self, x, i=None, j=None):
        return yp_int(self._sliceSearch(_yp_indexC, _yp_indexC5, x, i, j, yp_None))

    def rfind(self, x, i=None, j=None):
        return yp_int(self._sliceSearch(_yp_rfindC, _yp_rfindC5, x, i, j, yp_None))

    def rindex(self, x, i=None, j=None):
        return yp_int(self._sliceSearch(_yp_rindexC, _yp_rindexC5, x, i, j, yp_None))

    def count(self, x, i=None, j=None):
        return yp_int(self._sliceSearch(_yp_countC, _yp_countC5, x, i, j, yp_None))

    def append(self, x): _yp_append(self, x, yp_None)

    def extend(self, t): _yp_extend(self, _yp_iterable(t), yp_None)

    def insert(self, i, x): _yp_insertC(self, i, x, yp_None)

    def reverse(self): _yp_reverse(self, yp_None)

    def sort(self, *, key=None, reverse=False):
        _yp_sort4(self, key, reverse, yp_None)

    def isdisjoint(self, other):
        return _yp_isdisjoint(self, _yp_iterable(other))

    def issubset(self, other):
        return _yp_issubset(self, _yp_iterable(other))

    def issuperset(self, other):
        return _yp_issuperset(self, _yp_iterable(other))

    def union(self, *others):
        if len(others) < 1: return self.copy()
        return functools.reduce(_yp_union, others, self)

    def intersection(self, *others):
        if len(others) < 1: return self.copy()
        return functools.reduce(_yp_intersection, others, self)

    def difference(self, *others):
        if len(others) < 1: return self.copy()
        return functools.reduce(_yp_difference, others, self)

    def symmetric_difference(self, other):
        return _yp_symmetric_difference(self, _yp_iterable(other))

    def update(self, *others):
        for other in others: _yp_update(self, _yp_iterable(other), yp_None)

    def intersection_update(self, *others):
        for other in others: _yp_intersection_update(self, _yp_iterable(other), yp_None)

    def difference_update(self, *others):
        for other in others: _yp_difference_update(self, _yp_iterable(other), yp_None)

    def symmetric_difference_update(self, other):
        _yp_symmetric_difference_update(self, _yp_iterable(other), yp_None)

    def remove(self, elem): _yp_remove(self, elem, yp_None)

    def discard(self, elem): _yp_discard(self, elem, yp_None)

    def _slice(self, func, key, *args):
        start, stop, step = key.start, key.stop, key.step
        if start is None:
            start = _yp_SLICE_DEFAULT
        if stop is None:
            stop = _yp_SLICE_DEFAULT
        if step is None:
            step = 1
        return func(self, start, stop, step, *args)

    def __getitem__(self, key):
        if isinstance(key, slice):
            return self._slice(_yp_getsliceC4, key)
        else:
            return _yp_getitem(self, key)

    def __setitem__(self, key, value):
        if isinstance(key, slice):
            self._slice(_yp_setsliceC6, key, value, yp_None)
        else:
            _yp_setitem(self, key, value, yp_None)

    def __delitem__(self, key):
        if isinstance(key, slice):
            self._slice(_yp_delsliceC5, key, yp_None)
        else:
            _yp_delitem(self, key, yp_None)

    def get(self, key, default=None): return _yp_getdefault(self, key, default)

    def setdefault(self, key, default=None): return _yp_setdefault(self, key, default)

    def isalnum(self): return _yp_isalnum(self)

    def isalpha(self): return _yp_isalpha(self)

    def isdecimal(self): return _yp_isdecimal(self)

    def isdigit(self): return _yp_isdigit(self)

    def isidentifier(self): return _yp_isidentifier(self)

    def islower(self): return _yp_islower(self)

    def isnumeric(self): return _yp_isnumeric(self)

    def isprintable(self): return _yp_isprintable(self)

    def isspace(self): return _yp_isspace(self)

    def isupper(self): return _yp_isupper(self)

    def startswith(self, prefix, start=None, end=None):
        return self._sliceSearch(_yp_startswith, _yp_startswithC4, prefix, start, end)

    def endswith(self, suffix, start=None, end=None):
        return self._sliceSearch(_yp_endswith, _yp_endswithC4, suffix, start, end)

    def lower(self): return _yp_lower(self)

    def upper(self): return _yp_upper(self)

    def casefold(self): return _yp_casefold(self)

    def swapcase(self): return _yp_swapcase(self)

    def capitalize(self): return _yp_capitalize(self)

    def ljust(self, width, fillchar=None):
        if fillchar is None:
            return _yp_ljustC(self, width)
        return _yp_ljustC3(self, width, ord(fillchar))

    def rjust(self, width, fillchar=None):
        if fillchar is None:
            return _yp_rjustC(self, width)
        return _yp_rjustC3(self, width, ord(fillchar))

    def center(self, width, fillchar=None):
        if fillchar is None:
            return _yp_centerC(self, width)
        return _yp_centerC3(self, width, ord(fillchar))

    def expandtabs(self, tabsize=8): return _yp_expandtabsC(self, tabsize)

    def replace(self, oldsub, newsub, count=None):
        if count is None:
            return _yp_replace(self, oldsub, newsub)
        return _yp_replaceC4(self, oldsub, newsub, count)

    def lstrip(self, chars=_yp_arg_missing):
        if chars is _yp_arg_missing:
            return _yp_lstrip(self)
        return _yp_lstrip2(self, chars)

    def rstrip(self, chars=_yp_arg_missing):
        if chars is _yp_arg_missing:
            return _yp_rstrip(self)
        return _yp_rstrip2(self, chars)

    def strip(self, chars=_yp_arg_missing):
        if chars is _yp_arg_missing:
            return _yp_strip(self)
        return _yp_strip2(self, chars)

    def join(self, iterable): return _yp_join(self, _yp_iterable(iterable))

    def _split(self, func3, sep, maxsplit):
        if maxsplit is _yp_arg_missing:
            if sep is _yp_arg_missing:
                return _yp_split(self)
            return _yp_split2(self, sep)
        if sep is _yp_arg_missing:
            sep = yp_None
        return func3(self, sep, maxsplit)

    def split(self, sep=_yp_arg_missing, maxsplit=_yp_arg_missing):
        return self._split(_yp_splitC3, sep, maxsplit)

    def rsplit(self, sep=_yp_arg_missing, maxsplit=_yp_arg_missing):
        return self._split(_yp_rsplitC3, sep, maxsplit)

    def splitlines(self, keepends=True): return _yp_splitlines2(self, keepends)

    def _encdec(self, func1, func3, encoding, errors):
        if errors is None:
            if encoding is None:
                return func1(self)
            errors = yp_s_strict
        if encoding is None:
            encoding = yp_s_utf_8
        return func3(self, encoding, errors)

    def encode(self, encoding=None, errors=None):
        return self._encdec(_yp_encode, _yp_encode3, encoding, errors)

    def decode(self, encoding=None, errors=None):
        return self._encdec(_yp_decode, _yp_decode3, encoding, errors)

    def __call__(self, *args, **kwargs):
        return _yp_call_stars(self, args, kwargs)

    # Python requires arithmetic methods to _return_ NotImplemented
    @staticmethod
    def _arithmetic(func, *args):
        try:
            return func(*args)
        except TypeError:
            return NotImplemented

    def __add__(self, other): return self._arithmetic(_yp_add, self, other)

    def __sub__(self, other): return self._arithmetic(_yp_sub, self, other)

    def __mul__(self, other): return self._arithmetic(_yp_mul, self, other)

    def __truediv__(self, other): return self._arithmetic(_yp_truediv, self, other)

    def __floordiv__(self, other): return self._arithmetic(_yp_floordiv, self, other)

    def __mod__(self, other): return self._arithmetic(_yp_mod, self, other)

    def __pow__(self, other): return self._arithmetic(_yp_pow, self, other)

    def __lshift__(self, other): return self._arithmetic(_yp_lshift, self, other)

    def __rshift__(self, other): return self._arithmetic(_yp_rshift, self, other)

    def __and__(self, other): return self._arithmetic(_yp_amp, self, other)

    def __xor__(self, other): return self._arithmetic(_yp_xor, self, other)

    def __or__(self, other): return self._arithmetic(_yp_bar, self, other)

    def __neg__(self): return self._arithmetic(_yp_neg, self)

    def __pos__(self): return self._arithmetic(_yp_pos, self)

    def __abs__(self): return self._arithmetic(_yp_abs, self)

    def __invert__(self): return self._arithmetic(_yp_invert, self)

    def __radd__(self, other): return self._arithmetic(_yp_add, other, self)

    def __rsub__(self, other): return self._arithmetic(_yp_sub, other, self)

    def __rmul__(self, other): return self._arithmetic(_yp_mul, other, self)

    def __rtruediv__(self, other): return self._arithmetic(_yp_truediv, other, self)

    def __rfloordiv__(self, other): return self._arithmetic(_yp_floordiv, other, self)

    def __rmod__(self, other): return self._arithmetic(_yp_mod, other, self)

    def __rpow__(self, other): return self._arithmetic(_yp_pow, other, self)

    def __rlshift__(self, other): return self._arithmetic(_yp_lshift, other, self)

    def __rrshift__(self, other): return self._arithmetic(_yp_rshift, other, self)

    def __rand__(self, other): return self._arithmetic(_yp_amp, other, self)

    def __rxor__(self, other): return self._arithmetic(_yp_xor, other, self)

    def __ror__(self, other): return self._arithmetic(_yp_bar, other, self)

    # divmod is a little more complicated
    def __divmod__(self, other):
        div_p = c_ypObject_pp(yp_None)
        mod_p = c_ypObject_pp(yp_None)
        _yp_divmod(self, other, div_p, mod_p)
        return (div_p[0], mod_p[0])

    def __rdivmod__(self, other):
        return ypObject._from_python(other).__divmod__(self)


def pytype(yptype, pytypes):
    if not isinstance(pytypes, tuple):
        pytypes = (pytypes, )

    def _pytype(cls):
        cls._yp_type = yptype
        for pytype in pytypes:
            ypObject._pytype2yp[pytype] = cls
        ypObject._yptype2yp[yptype.value] = cls
        return cls
    return _pytype

# There's a circular reference between yp_t_type and yp_type we need to dance around
yp_t_type = c_ypObject_p.in_dll(ypdll, "yp_t_type")


class yp_type(ypObject):
    _yp_type = yp_t_type

    def __new__(cls, object):
        if not isinstance(object, ypObject):
            raise TypeError("expected ypObject in yp_type")
        return object._yp_type
yp_t_type.__class__ = yp_type
_yp_pyobj_cache[yp_t_type.value] = yp_t_type
ypObject._pytype2yp[type] = yp_type
ypObject._yptype2yp[yp_t_type.value] = yp_type

# Now that the "type(type) is type" issue has been dealt with, we can import the other type
# objects as normal
c_ypObject_p_value("yp_t_invalidated")
c_ypObject_p_value("yp_t_exception")
c_ypObject_p_value("yp_t_NoneType")
c_ypObject_p_value("yp_t_bool")
c_ypObject_p_value("yp_t_int")
c_ypObject_p_value("yp_t_intstore")
c_ypObject_p_value("yp_t_float")
c_ypObject_p_value("yp_t_floatstore")
c_ypObject_p_value("yp_t_iter")
c_ypObject_p_value("yp_t_bytes")
c_ypObject_p_value("yp_t_bytearray")
c_ypObject_p_value("yp_t_str")
c_ypObject_p_value("yp_t_chrarray")
c_ypObject_p_value("yp_t_tuple")
c_ypObject_p_value("yp_t_list")
c_ypObject_p_value("yp_t_frozenset")
c_ypObject_p_value("yp_t_set")
c_ypObject_p_value("yp_t_frozendict")
c_ypObject_p_value("yp_t_dict")
c_ypObject_p_value("yp_t_range")
c_ypObject_p_value("yp_t_function")


@pytype(yp_t_exception, BaseException)
class yp_BaseException(ypObject):
    def __new__(cls, *args, **kwargs):
        raise NotImplementedError("can't instantiate yp_BaseException directly")

    def _yp_errcheck(self):
        """Raises the appropriate Python exception"""
        super()._yp_errcheck()
        name, pyExc = _ypExc2py[self.value]
        if issubclass(pyExc, UnicodeEncodeError):
            raise pyExc("<null>", "", 0, 0, name)
        elif issubclass(pyExc, UnicodeDecodeError):
            raise pyExc("<null>", b"", 0, 0, name)
        else:
            raise pyExc(name)


@pytype(yp_t_NoneType, type(None))
class yp_NoneType(ypObject):
    def __new__(cls, *args, **kwargs):
        raise NotImplementedError("can't instantiate yp_NoneType directly")

    # TODO When nohtyP has str/repr, use it instead of this faked-out version

    def _yp_str(self): return yp_s_None
    _yp_repr = _yp_str
c_ypObject_p_value("yp_None")


@pytype(yp_t_bool, bool)
class yp_bool(ypObject):
    def __new__(cls, *args, **kwargs):
        if args and not isinstance(args[0], (ypObject, bool)):
            raise TypeError("expected ypObject or bool in yp_bool")
        return _yp_call_stars(cls._yp_type, args, kwargs)

    @classmethod
    def _from_python(cls, pyobj):
        return yp_True if pyobj else yp_False

    def _as_int(self): return yp_i_one if self.value == yp_True.value else yp_i_zero

    # TODO When nohtyP has str/repr, use it instead of this faked-out version
    def _yp_str(self): return yp_s_True if self.value == yp_True.value else yp_s_False
    _yp_repr = _yp_str

    def __bool__(self): return self.value == yp_True.value

    # TODO If/when nohtyP supports arithmetic on bool, remove these comparison hacks
    def __lt__(self, other): return bool(self) < other

    def __le__(self, other): return bool(self) <= other

    def __eq__(self, other): return bool(self) == other

    def __ne__(self, other): return bool(self) != other

    def __ge__(self, other): return bool(self) >= other

    def __gt__(self, other): return bool(self) > other

    # TODO If/when nohtyP supports arithmetic on bool, remove these _as_int/_arithmetic hacks
    @staticmethod
    def _arithmetic(left, op, right):
        if isinstance(left, yp_bool):
            left = left._as_int()
        if isinstance(right, yp_bool):
            right = right._as_int()
        return op(left, right)

    def __add__(self, other): return yp_bool._arithmetic(self, operator.add, other)

    def __sub__(self, other): return yp_bool._arithmetic(self, operator.sub, other)

    def __mul__(self, other): return yp_bool._arithmetic(self, operator.mul, other)

    def __truediv__(self, other): return yp_bool._arithmetic(self, operator.truediv, other)

    def __floordiv__(self, other): return yp_bool._arithmetic(self, operator.floordiv, other)

    def __mod__(self, other): return yp_bool._arithmetic(self, operator.mod, other)

    def __pow__(self, other): return yp_bool._arithmetic(self, operator.pow, other)

    def __lshift__(self, other): return yp_bool._arithmetic(self, operator.lshift, other)

    def __rshift__(self, other): return yp_bool._arithmetic(self, operator.rshift, other)

    def __radd__(self, other): return yp_bool._arithmetic(other, operator.add, self)

    def __rsub__(self, other): return yp_bool._arithmetic(other, operator.sub, self)

    def __rmul__(self, other): return yp_bool._arithmetic(other, operator.mul, self)

    def __rtruediv__(self, other): return yp_bool._arithmetic(other, operator.truediv, self)

    def __rfloordiv__(self, other): return yp_bool._arithmetic(other, operator.floordiv, self)

    def __rmod__(self, other): return yp_bool._arithmetic(other, operator.mod, self)

    def __rpow__(self, other): return yp_bool._arithmetic(other, operator.pow, self)

    def __neg__(self): return -self._as_int()

    def __pos__(self): return +self._as_int()

    def __abs__(self): return abs(self._as_int())

    def __invert__(self): return ~self._as_int()

    @property
    def real(self): return self._as_int()

    @property
    def imag(self): return yp_i_zero

    @staticmethod
    def _boolean(left, op, right):
        result = yp_bool._arithmetic(left, op, right)
        if isinstance(left, yp_bool) and isinstance(right, yp_bool):
            return yp_bool(result)
        return result

    def __and__(self, other): return yp_bool._boolean(self, operator.and_, other)

    def __xor__(self, other): return yp_bool._boolean(self, operator.xor, other)

    def __or__(self, other): return yp_bool._boolean(self, operator.or_, other)

    def __rand__(self, other): return yp_bool._boolean(other, operator.and_, self)

    def __rxor__(self, other): return yp_bool._boolean(other, operator.xor, self)

    def __ror__(self, other): return yp_bool._boolean(other, operator.or_, self)

c_ypObject_p_value("yp_True")
c_ypObject_p_value("yp_False")


@pytype(yp_t_iter, (iter, type(x for x in ())))
class yp_iter(ypObject):
    def __new__(cls, object, sentinel=_yp_arg_missing, /):
        if sentinel is not _yp_arg_missing:
            return _yp_callN(cls._yp_type, _yp_callable(object), sentinel)
        elif isinstance(object, ypObject):
            return _yp_callN(cls._yp_type, object)
        elif hasattr(object, "_yp_iter"):
            return object._yp_iter()
        else:
            return cls._from_python(object)

    @staticmethod
    def _pyiter_wrapper(pyiter, yp_self, yp_value):
        try:
            if _yp_isexceptionC(yp_value):
                return _yp_incref(yp_value)  # yp_GeneratorExit, in particular
            try:
                py_result = next(pyiter)
            except BaseException as e: # exceptions from the iterator get passed to nohtyP
                return _yp_incref(_pyExc2yp[type(e)])
            return _yp_incref(ypObject._from_python(py_result))
        except BaseException as e: # ensure unexpected exceptions don't crash the program
            traceback.print_exc()
            return _yp_incref(_pyExc2yp.get(type(e), _yp_BaseException))

    @classmethod
    def _from_python(cls, pyobj):
        length_hint = operator.length_hint(pyobj)
        ypfunc = c_yp_generator_func_t(functools.partial(cls._pyiter_wrapper, iter(pyobj)))
        declaration = c_yp_generator_decl_t(ypfunc, length_hint)
        self = _yp_generatorC(declaration)
        _yp_reverse_refs[self.value].append(ypfunc)
        return self

    def __iter__(self): return self

    def __length_hint__(self): return _yp_length_hintC(self, yp_None)


def _yp_iterable(iterable):
    """Returns a ypObject that nohtyP can iterate over directly, which may be iterable itself or a
    yp_iter based on iterable."""
    return ypObject._from_python(iterable, default=yp_iter)


@pytype(yp_t_int, int)
class yp_int(ypObject):
    @classmethod
    def _from_python(cls, pyobj):
        return _yp_intC(pyobj)

    def _asint(self): return _yp_asintC(self, yp_None)
    # TODO When nohtyP has str/repr, use it instead of this faked-out version

    def _yp_str(self): return yp_str(str(self._asint()))

    def _yp_repr(self): return yp_str(repr(self._asint()))

    def bit_length(self): return yp_int(_yp_bit_lengthC(self, yp_None))

    # TODO Implement yp_index
    def __index__(self): return self._asint()
c_ypObject_p_value("yp_sys_maxint")
c_ypObject_p_value("yp_sys_minint")
c_ypObject_p_value("yp_i_neg_one")
c_ypObject_p_value("yp_i_zero")
c_ypObject_p_value("yp_i_one")
c_ypObject_p_value("yp_i_two")
ypObject_LEN_MAX = _yp_intC(0x7FFFFFFF)


def yp_len(obj, /):
    """Returns len(obj) of a ypObject as a yp_int"""
    if not isinstance(obj, ypObject):
        raise TypeError("expected ypObject in yp_len")
    return yp_func_len(obj)


def yp_hash(obj, /):
    """Returns hash(obj) of a ypObject as a yp_int"""
    if not isinstance(obj, ypObject):
        raise TypeError("expected ypObject in yp_hash")
    return yp_func_hash(obj)


@pytype(yp_t_float, float)
class yp_float(ypObject):
    @classmethod
    def _from_python(cls, pyobj):
        return _yp_floatCF(pyobj)

    def _asfloat(self): return _yp_asfloatC(self, yp_None)
    # TODO When nohtyP has str/repr, use it instead of this faked-out version

    def _yp_str(self): return yp_str(str(self._asfloat()))

    def _yp_repr(self): return yp_str(repr(self._asfloat()))


class _ypBytes(ypObject):
    def __new__(cls, *args, **kwargs):
        if args:
            args = (_yp_iterable(args[0]), *args[1:])
        return _yp_call_stars(cls._yp_type, args, kwargs)

    def _get_data_size(self):
        size = c_yp_ssize_t_p(c_yp_ssize_t(0))
        data = c_char_pp(c_char_p())
        # errcheck disabled for _yp_asbytesCX, so do it here
        _yp_asbytesCX(self, size, data)._yp_errcheck()
        return size[0], cast(data.contents, c_void_p)

    def _asbytes(self):
        size, data = self._get_data_size()
        return string_at(data, size)

    def _yp_errcheck(self):
        super()._yp_errcheck()
        size, data = self._get_data_size()
        assert string_at(data.value+size, 1) == b"\x00", "missing null terminator"
        return size, data

    # nohtyP currently doesn't overload yp_add et al, but Python expects this
    def __add__(self, other): return _yp_concat(self, other)

    def __mul__(self, factor):
        if isinstance(factor, float):
            raise TypeError
        return _yp_repeatC(self, factor)

    def __rmul__(self, factor):
        if isinstance(factor, float):
            raise TypeError
        return _yp_repeatC(self, factor)

    # TODO When nohtyP has hex/fromhex, use it instead of this faked-out version

    @classmethod
    def fromhex(cls, s):
        if isinstance(s, yp_str):
            s = str(s)
        return cls(bytes.fromhex(s))

    def hex(self, sep=_yp_arg_missing, bytes_per_sep=1, /):
        # Python's default value for sep is NULL, i.e. the null pointer, which is not friendly
        if sep is _yp_arg_missing:
            return yp_str(self._asbytes().hex())
        else:
            return yp_str(self._asbytes().hex(sep, bytes_per_sep))

@pytype(yp_t_bytes, bytes)
class yp_bytes(_ypBytes):
    @classmethod
    def _from_python(cls, pyobj):
        return _yp_bytesC(len(pyobj), pyobj)

    # TODO When nohtyP has str/repr, use it instead of this faked-out version

    def _yp_str(self): return yp_str(str(self._asbytes()))
    _yp_repr = _yp_str

    def _yp_errcheck(self):
        data, size = super()._yp_errcheck()
        # TODO ...unless it's built with an empty tuple; is it worth replacing with empty?
        # if size < 1 and "yp_bytes_empty" in globals():
        #    assert self is yp_bytes_empty, "an empty bytes should be yp_bytes_empty"

    def decode(self, encoding="utf-8", errors="strict"):
        return _yp_str3(self, encoding, errors)
c_ypObject_p_value("yp_bytes_empty")


@pytype(yp_t_bytearray, bytearray)
class yp_bytearray(_ypBytes):
    # TODO When nohtyP has str/repr, use it instead of this faked-out version

    @classmethod
    def _from_python(cls, pyobj):
        return _yp_bytearrayC(len(pyobj), pyobj)

    def _yp_str(self): return yp_str("bytearray(%r)" % self._asbytes())
    _yp_repr = _yp_str

    def pop(self, i=_yp_arg_missing):
        if i is _yp_arg_missing:
            return _yp_pop(self)
        else:
            return _yp_popindexC(self, i)

    def __iadd__(self, other):
        _yp_extend(self, other, yp_None)
        return self

    def __imul__(self, factor):
        if isinstance(factor, float):
            raise TypeError
        _yp_irepeatC(self, factor, None)
        return self
    # XXX nohtyP will return a chrarray if asked to decode a bytearray, but Python expects str

    def decode(self, encoding="utf-8", errors="strict"):
        return _yp_str3(self, encoding, errors)

# TODO Generally, need to adjust constructors and functions to only accept exact, specific types
# from Python

# TODO When nohtyP has types that have string representations, update this
# TODO Just generally move more of this logic into nohtyP, when available
class _ypStr(ypObject):
    def _asstr(self): return self.encode()._asbytes().decode()

    # nohtyP currently doesn't overload yp_add et al, but Python expects this
    def __add__(self, other): return _yp_concat(self, other)

    def __mul__(self, factor):
        if isinstance(factor, float):
            raise TypeError
        return _yp_repeatC(self, factor)

    __rmul__ = __mul__

    # TODO Use nohtyP's versions when supported, not these faked-out versions
    def replace(self, old, new, count=-1):
        if isinstance(old, _ypStr):
            old = old._asstr()
        if isinstance(new, _ypStr):
            new = new._asstr()
        return yp_type(self)(self._asstr().replace(old, new, count))

    def split(self, sep=None, maxsplit=-1):
        if isinstance(sep, _ypStr):
            sep = sep._asstr()
        return yp_list(self._asstr().split(sep, maxsplit))

@pytype(yp_t_str, str)
class yp_str(_ypStr):
    def __new__(cls, *args, **kwargs):
        if yp_str._new_bypass_call_stars(*args, **kwargs):
            return args[0]._yp_str()
        return _yp_call_stars(cls._yp_type, args, kwargs)

    @staticmethod
    def _new_bypass_call_stars(object=_yp_arg_missing, encoding=_yp_arg_missing, errors=_yp_arg_missing):
        """Returns True if we should return object._yp_str(). This is temporary until we add proper
        str/repr functionality into nohtyP.
        """
        return isinstance(object, ypObject) and encoding is _yp_arg_missing and errors is _yp_arg_missing

    @classmethod
    def _from_python(cls, pyobj):
        encoded = pyobj.encode("utf-8", "surrogatepass")
        return _yp_str_frombytesC4(len(encoded), encoded, yp_s_utf_8, yp_s_surrogatepass)

    # Just as yp_bool.__bool__ must return a bool, so too must this return a str
    def __str__(self): return self._asstr()

    def _yp_str(self): return self

    # TODO When nohtyP supports repr, replace this faked-out version
    def _yp_repr(self): return yp_str._from_python(repr(self._asstr()))

@pytype(yp_t_chrarray, ())
class yp_chrarray(_ypStr):
    @classmethod
    def _from_python(cls, pyobj):
        encoded = pyobj.encode("utf-8", "surrogatepass")
        return _yp_chrarray_frombytesC4(len(encoded), encoded, yp_s_utf_8, yp_s_surrogatepass)

    def _yp_str(self): return yp_str._from_python(self._asstr())

    # TODO When nohtyP supports repr, replace this faked-out version
    def _yp_repr(self): return yp_str._from_python("chrarray(%r)" % self._asstr())

c_ypObject_p_value("yp_s_ascii")
c_ypObject_p_value("yp_s_latin_1")
c_ypObject_p_value("yp_s_utf_8")
c_ypObject_p_value("yp_s_ucs_2")
c_ypObject_p_value("yp_s_ucs_4")
c_ypObject_p_value("yp_s_strict")
c_ypObject_p_value("yp_s_replace")
c_ypObject_p_value("yp_s_ignore")
c_ypObject_p_value("yp_s_xmlcharrefreplace")
c_ypObject_p_value("yp_s_backslashreplace")
c_ypObject_p_value("yp_s_surrogateescape")
c_ypObject_p_value("yp_s_surrogatepass")
c_ypObject_p_value("yp_s_star_args")
c_ypObject_p_value("yp_s_star_star_kwargs")
c_ypObject_p_value("yp_str_empty")
yp_s_None = _yp_str_frombytesC2(4, b"None")
yp_s_True = _yp_str_frombytesC2(4, b"True")
yp_s_False = _yp_str_frombytesC2(5, b"False")


# FIXME Support repr, then make this a proper nohtyP function object.
def yp_repr(object):
    """Returns repr(object) of a ypObject as a yp_str"""
    if isinstance(object, str):
        object = yp_str(object)
    if not isinstance(object, ypObject):
        raise TypeError("expected ypObject in yp_repr")
    return object._yp_repr()


class _ypTuple(ypObject):
    def __new__(cls, *args, **kwargs):
        if args:
            args = (_yp_iterable(args[0]), *args[1:])
        return _yp_call_stars(cls._yp_type, args, kwargs)

    # nohtyP currently doesn't overload yp_add et al, but Python expects this
    def __add__(self, other): return _yp_concat(self, other)

    def __mul__(self, factor):
        if isinstance(factor, float):
            raise TypeError
        return _yp_repeatC(self, factor)

    def __rmul__(self, factor):
        if isinstance(factor, float):
            raise TypeError
        return _yp_repeatC(self, factor)


@pytype(yp_t_tuple, tuple)
class yp_tuple(_ypTuple):
    @classmethod
    def _from_python(cls, pyobj):
        if len(pyobj) > CTYPES_MAX_ARGCOUNT-1:
            return _yp_tuple(yp_iter._from_python(pyobj))
        return _yp_tupleN(*pyobj)

    def _yp_errcheck(self):
        super()._yp_errcheck()
        # TODO ...unless it's built with an empty tuple; is it worth replacing with empty?
        # if len(self) < 1 and "yp_tuple_empty" in globals():
        #    assert self is yp_tuple_empty, "an empty tuple should be yp_tuple_empty"
    # TODO When nohtyP supports str/repr, replace this faked-out version

    def _yp_str(self):
        return yp_str("(%s)" % ", ".join(repr(x) for x in self))
    _yp_repr = _yp_str
c_ypObject_p_value("yp_tuple_empty")


@pytype(yp_t_list, list)
class yp_list(_ypTuple):
    @classmethod
    def _from_python(cls, pyobj):
        if len(pyobj) > CTYPES_MAX_ARGCOUNT-1:
            return _yp_list(yp_iter._from_python(pyobj))
        return _yp_listN(*pyobj)

    @reprlib.recursive_repr("[...]")
    def _yp_str(self):
        # TODO When nohtyP supports str/repr, replace this faked-out version
        return yp_str("[%s]" % ", ".join(repr(x) for x in self))
    _yp_repr = _yp_str

    def pop(self, i=_yp_arg_missing):
        if i is _yp_arg_missing:
            return _yp_pop(self)
        else:
            return _yp_popindexC(self, i)

    def __iadd__(self, other):
        _yp_extend(self, other, yp_None)
        return self

    def __imul__(self, factor):
        if isinstance(factor, float):
            raise TypeError
        _yp_irepeatC(self, factor, yp_None)
        return self


class _ypSet(ypObject):
    def __new__(cls, *args, **kwargs):
        if args:
            args = (_yp_iterable(args[0]), *args[1:])
        return _yp_call_stars(cls._yp_type, args, kwargs)

    @staticmethod
    def _bad_other(other): return not isinstance(other, (_ypSet, frozenset, set))

    def __or__(self, other):
        if self._bad_other(other):
            return NotImplemented
        return self.union(other)

    def __and__(self, other):
        if self._bad_other(other):
            return NotImplemented
        return self.intersection(other)

    def __sub__(self, other):
        if self._bad_other(other):
            return NotImplemented
        return self.difference(other)

    def __xor__(self, other):
        if self._bad_other(other):
            return NotImplemented
        return self.symmetric_difference(other)

    def __ror__(self, other):
        if self._bad_other(other):
            return NotImplemented
        return other.union(self)

    def __rand__(self, other):
        if self._bad_other(other):
            return NotImplemented
        return other.intersection(self)

    def __rsub__(self, other):
        if self._bad_other(other):
            return NotImplemented
        return other.difference(self)

    def __rxor__(self, other):
        if self._bad_other(other):
            return NotImplemented
        return other.symmetric_difference(self)

    def __ior__(self, other):
        if self._bad_other(other):
            return NotImplemented
        self.update(other)
        return self

    def __iand__(self, other):
        if self._bad_other(other):
            return NotImplemented
        self.intersection_update(other)
        return self

    def __isub__(self, other):
        if self._bad_other(other):
            return NotImplemented
        self.difference_update(other)
        return self

    def __ixor__(self, other):
        if self._bad_other(other):
            return NotImplemented
        self.symmetric_difference_update(other)
        return self

    def add(self, elem): _yp_push(self, elem, yp_None)


@pytype(yp_t_frozenset, frozenset)
class yp_frozenset(_ypSet):
    @classmethod
    def _from_python(cls, pyobj):
        if len(pyobj) > CTYPES_MAX_ARGCOUNT-1:
            return _yp_frozenset(yp_iter._from_python(pyobj))
        return _yp_frozensetN(*pyobj)

    def _yp_errcheck(self):
        super()._yp_errcheck()
        # TODO ...unless it's built with an empty tuple; is it worth replacing with empty?
        # if len(self) < 1 and "yp_frozenset_empty" in globals():
        #    assert self is yp_frozenset_empty, "an empty frozenset should be yp_frozenset_empty"

    def _yp_str(self):
        # TODO When nohtyP supports str/repr, replace this faked-out version
        if not self:
            return yp_str("frozenset()")
        return yp_str("frozenset({%s})" % ", ".join(repr(x) for x in self))
    _yp_repr = _yp_str

c_ypObject_p_value("yp_frozenset_empty")


@pytype(yp_t_set, set)
class yp_set(_ypSet):
    @classmethod
    def _from_python(cls, pyobj):
        if len(pyobj) > CTYPES_MAX_ARGCOUNT-1:
            return _yp_set(yp_iter._from_python(pyobj))
        return _yp_setN(*pyobj)

    def _yp_str(self):
        # TODO When nohtyP supports str/repr, replace this faked-out version
        if not self:
            return yp_str("set()")
        return yp_str("{%s}" % ", ".join(repr(x) for x in self))
    _yp_repr = _yp_str


# Python dict objects need to be passed through this then sent to the "K" version of the function;
# all other objects can be converted to nohtyP and passed in thusly
def _yp_flatten_dict(args):
    retval = []
    for key, value in args.items():
        retval.append(key)
        retval.append(value)
    return retval


class _yp_item_iter(yp_iter):
    @classmethod
    def _from_python(cls, pyobj):
        # help(dict.update) states that it only looks for a .keys() method
        if hasattr(pyobj, "keys"):  # there's a test that only defines keys...
            return super()._from_python((k, pyobj[k]) for k in pyobj.keys())
        else:
            return super()._from_python(pyobj)


def _yp_dict_iterable(iterable):
    """Like _yp_iterable, but returns an "item iterator" if iterable is a mapping object."""
    return ypObject._from_python(iterable, default=_yp_item_iter)


# TODO If nohtyP ever supports "dict view" objects, replace these faked-out versions
class _setlike_dictview:
    def __init__(self, mp): self._mp = mp

    def __iter__(self): return self._yp_iter()

    def _as_set(self): return yp_set(self._yp_iter())

    @staticmethod
    def _conv_other(other):
        if isinstance(other, _setlike_dictview):
            return other._as_set()
        return other

    def __lt__(self, other): return self._as_set() < self._conv_other(other)

    def __le__(self, other): return self._as_set() <= self._conv_other(other)

    def __eq__(self, other): return self._as_set() == self._conv_other(other)

    def __ne__(self, other): return self._as_set() != self._conv_other(other)

    def __ge__(self, other): return self._as_set() >= self._conv_other(other)

    def __gt__(self, other): return self._as_set() > self._conv_other(other)

    def __or__(self, other): return self._as_set() | self._conv_other(other)

    def __and__(self, other): return self._as_set() & self._conv_other(other)

    def __sub__(self, other): return self._as_set() - self._conv_other(other)

    def __xor__(self, other): return self._as_set() ^ self._conv_other(other)

    def __ror__(self, other): return self._conv_other(other) | self._as_set()

    def __rand__(self, other): return self._conv_other(other) & self._as_set()

    def __rsub__(self, other): return self._conv_other(other) - self._as_set()

    def __rxor__(self, other): return self._conv_other(other) ^ self._as_set()


class _keys_dictview(_setlike_dictview):
    def _yp_iter(self):
        return _yp_iter_keys(self._mp)


class _values_dictview:
    def __init__(self, mp): self._mp = mp

    def _yp_iter(self):
        return _yp_iter_values(self._mp)

    __iter__ = _yp_iter


class _items_dictview(_setlike_dictview):
    def _yp_iter(self):
        return _yp_iter_items(self._mp)


# TODO Adapt the Python test suite to test for frozendict, adding in tests similar to those found
# between list/tuple and set/frozenset (ie the singleton empty frozendict, etc)


class _ypDict(ypObject):
    def __new__(cls, *args, **kwargs):
        if args:
            args = (_yp_dict_iterable(args[0]), *args[1:])
        return _yp_call_stars(cls._yp_type, args, kwargs)

    @classmethod
    def _from_python(cls, pyobj):
        if len(pyobj) > (CTYPES_MAX_ARGCOUNT-1) // 2:
            return _yp_dict(_yp_item_iter._from_python(pyobj))
        return _yp_dictK(*_yp_flatten_dict(pyobj))

    def keys(self): return _keys_dictview(self)

    def values(self): return _values_dictview(self)

    def items(self): return _items_dictview(self)

    def pop(self, key, default=_yp_KeyError):
        if default is _yp_KeyError:
            return _yp_popvalue2(self, key)
        else:
            return _yp_popvalue3(self, key, default)

    def popitem(self):
        key_p = c_ypObject_pp(yp_None)
        value_p = c_ypObject_pp(yp_None)
        _yp_popitem(self, key_p, value_p)
        return (key_p[0], value_p[0])

    def update(self, object=yp_tuple_empty, /, **kwargs):
        _yp_update(self, _yp_dict_iterable(object), yp_None)
        _yp_update(self, kwargs, yp_None)


@pytype(yp_t_frozendict, ())
class yp_frozendict(_ypDict):
    @classmethod
    def fromkeys(cls, seq, value=None): return _yp_frozendict_fromkeysN(value, *seq)

    @classmethod
    def _from_python(cls, pyobj):
        if len(pyobj) > (CTYPES_MAX_ARGCOUNT-1) // 2:
            return _yp_frozendict(_yp_item_iter._from_python(pyobj))
        return _yp_frozendictK(*_yp_flatten_dict(pyobj))

    def _yp_str(self):
        # TODO When nohtyP supports str/repr, replace this faked-out version
        return yp_str("frozendict({%s})" % ", ".join(f"{k!r}: {v!r}" for k, v in self.items()))
    _yp_repr = _yp_str


@pytype(yp_t_dict, dict)
class yp_dict(_ypDict):
    @classmethod
    def fromkeys(cls, seq, value=None): return _yp_dict_fromkeysN(value, *seq)

    @classmethod
    def _from_python(cls, pyobj):
        if len(pyobj) > (CTYPES_MAX_ARGCOUNT-1) // 2:
            return _yp_dict(_yp_item_iter._from_python(pyobj))
        return _yp_dictK(*_yp_flatten_dict(pyobj))

    @reprlib.recursive_repr("{...}")
    def _yp_str(self):
        # TODO When nohtyP supports str/repr, replace this faked-out version
        return yp_str("{%s}" % ", ".join(f"{k!r}: {v!r}" for k, v in self.items()))
    _yp_repr = _yp_str


@pytype(yp_t_range, range)
class yp_range(ypObject):
    @classmethod
    def _from_python(cls, pyobj):
        return _yp_rangeC3(pyobj.start, pyobj.stop, pyobj.step)

    def _yp_errcheck(self):
        super()._yp_errcheck()
        if len(self) < 1 and "yp_range_empty" in globals():
            assert self is yp_range_empty, "an empty range should be yp_range_empty"
    # TODO When nohtyP has str/repr, use it instead of this faked-out version

    def _yp_str(self):
        self_len = len(self)
        if self_len < 1:
            return yp_str("range(0, 0)")
        self_start = self[0]._asint()
        if self_len < 2:
            return yp_str("range(%d, %d)" % (self_start, self_start+1))
        self_step = self[1]._asint() - self_start
        self_end = self_start + (self_len * self_step)
        if self_step == 1:
            return yp_str("range(%d, %d)" % (self_start, self_end))
        return yp_str("range(%d, %d, %d)" % (self_start, self_end, self_step))
    _yp_repr = _yp_str
c_ypObject_p_value("yp_range_empty")


# XXX Could also add type(repr) to this (builtins are a different type) but we want nohtyP versions
# of the builtins, so the error is useful.
@pytype(yp_t_function, type(lambda: 1))
class yp_function(ypObject):
    def __new__(cls, *args, **kwargs):
        raise NotImplementedError("can't instantiate yp_function directly")

    @staticmethod
    def _pyfunction_wrapper(pyfunction, yp_self, n, argarray):
        try:
            transmuted = tuple(_yp_transmute_and_cache(argarray[i], steal=False) for i in range(n))
            try:
                py_result = pyfunction(*transmuted)
            except BaseException as e: # exceptions from the function get passed to nohtyP
                return _yp_incref(_pyExc2yp[type(e)])
            return _yp_incref(ypObject._from_python(py_result))
        except BaseException as e: # ensure unexpected exceptions don't crash the program
            traceback.print_exc()
            return _yp_incref(_pyExc2yp.get(type(e), _yp_BaseException))

    @classmethod
    # TODO Get fancy: read the signature from pyobj and create the param decl from that?
    def with_parameters(cls, pyfunction, parameters):
        """Creates a yp_function object where the parameters are parsed by nohtyP. pyfunction will
        be called with *argarray.
        """
        ypcode = c_yp_function_code_t(functools.partial(cls._pyfunction_wrapper, pyfunction))
        declaration = c_yp_function_decl_t(ypcode, 0, len(parameters), (c_yp_parameter_decl_t * len(parameters))(*parameters))
        self = _yp_functionC(declaration)
        _yp_reverse_refs[self.value].append(ypcode)
        return self

    @classmethod
    def _from_python(cls, pyobj):
        return cls.with_parameters(
            lambda args, kwargs: pyobj(*args, **kwargs),
            ((yp_s_star_args, ), (yp_s_star_star_kwargs, ))
        )

c_ypObject_p_value("yp_func_chr")
c_ypObject_p_value("yp_func_hash")
c_ypObject_p_value("yp_func_iscallable")
c_ypObject_p_value("yp_func_len")
c_ypObject_p_value("yp_func_reversed")
c_ypObject_p_value("yp_func_sorted")

yp_chr = yp_func_chr
yp_iscallable = yp_func_iscallable
yp_reversed = yp_func_reversed

def yp_sorted(*args, **kwargs):
    if args:
        args = (_yp_iterable(args[0]), *args[1:])
    return _yp_call_stars(yp_func_sorted, args, kwargs)


def _yp_callable(callable):
    """Returns a ypObject that nohtyP can call directly, which may be callable itself or a
    yp_function based on callable."""
    return ypObject._from_python(callable, default=yp_function)
