"""Unit tests for the copy module."""

from yp import *
import copy
import copyreg
import weakref
import abc
from operator import le, lt, ge, gt, eq, ne

from yp_test import yp_unittest
from yp_test import support

order_comparisons = le, lt, ge, gt
equality_comparisons = eq, ne
comparisons = order_comparisons + equality_comparisons

class TestCopy(yp_unittest.TestCase):

    # Attempt full line coverage of copy.py from top to bottom

    @yp_unittest.skip_not_applicable
    def test_exceptions(self):
        self.assertIs(copy.Error, copy.error)
        self.assertTrue(issubclass(copy.Error, Exception))

    # The copy() method

    def test_copy_basic(self):
        x = yp_int(42)
        y = copy.copy(x)
        self.assertEqual(x, y)

    @yp_unittest.skip_user_defined_types
    def test_copy_copy(self):
        class C(object):
            def __init__(self, foo):
                self.foo = foo
            def __copy__(self):
                return C(self.foo)
        x = C(42)
        y = copy.copy(x)
        self.assertEqual(y.__class__, x.__class__)
        self.assertEqual(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_copy_registry(self):
        class C(object):
            def __new__(cls, foo):
                obj = object.__new__(cls)
                obj.foo = foo
                return obj
        def pickle_C(obj):
            return (C, (obj.foo,))
        x = C(42)
        self.assertRaises(TypeError, copy.copy, x)
        copyreg.pickle(C, pickle_C, C)
        y = copy.copy(x)

    @yp_unittest.skip_user_defined_types
    def test_copy_reduce_ex(self):
        class C(object):
            def __reduce_ex__(self, proto):
                c.append(1)
                return ""
            def __reduce__(self):
                self.fail("shouldn't call this")
        c = []
        x = C()
        y = copy.copy(x)
        self.assertIs(y, x)
        self.assertEqual(c, [1])

    @yp_unittest.skip_user_defined_types
    def test_copy_reduce(self):
        class C(object):
            def __reduce__(self):
                c.append(1)
                return ""
        c = []
        x = C()
        y = copy.copy(x)
        self.assertIs(y, x)
        self.assertEqual(c, [1])

    @yp_unittest.skip_user_defined_types
    def test_copy_cant(self):
        class C(object):
            def __getattribute__(self, name):
                if name.startswith("__reduce"):
                    raise AttributeError(name)
                return object.__getattribute__(self, name)
        x = C()
        self.assertRaises(copy.Error, copy.copy, x)

    # Type-specific _copy_xxx() methods

    @yp_unittest.skip_user_defined_types
    def test_copy_atomic(self):
        class Classic:
            pass
        class NewStyle(object):
            pass
        def f():
            pass
        class WithMetaclass(metaclass=abc.ABCMeta):
            pass
        tests = [None, ..., NotImplemented,
                 42, 2**100, 3.14, True, False, 1j,
                 "hello", "hello\u1234", f.__code__,
                 b"world", bytes(range(256)), range(10), slice(1, 10, 2),
                 NewStyle, Classic, max, WithMetaclass, property()]
        for x in tests:
            self.assertIs(copy.copy(x), x)

    # yp_copy attempts lazy copies (returning the same object)
    def test_copy_atomic_builtins(self):
        tests = [yp_None,
                 yp_int(42), yp_int(2022), yp_float(3.14), yp_True, yp_False,
                 yp_dict,
                 yp_str("hello"), yp_str("hello\u1234"),
                 yp_bytes(b"world"), yp_bytes(yp_range(256)), yp_range(10),
                 yp_tuple(yp_range(5)), yp_frozenset(yp_range(3)),
                 yp_frozendict.fromkeys(yp_range(2)),
                 yp_chr]
        for x in tests:
            self.assertIs(copy.copy(x), x)

    def test_copy_list(self):
        x = yp_list([1, 2, 3])
        y = copy.copy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        x = yp_list([])
        y = copy.copy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)

    def test_copy_tuple(self):
        x = yp_tuple((1, 2, 3))
        self.assertIs(copy.copy(x), x)
        x = yp_tuple()
        self.assertIs(copy.copy(x), x)
        x = yp_tuple((1, 2, 3, []))
        self.assertIs(copy.copy(x), x)

    def test_copy_dict(self):
        x = yp_dict({"foo": 1, "bar": 2})
        y = copy.copy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        x = yp_dict()
        y = copy.copy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)

    def test_copy_set(self):
        x = yp_set({1, 2, 3})
        y = copy.copy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        x = yp_set()
        y = copy.copy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)

    def test_copy_frozenset(self):
        x = yp_frozenset({1, 2, 3})
        self.assertIs(copy.copy(x), x)
        x = yp_frozenset()
        self.assertIs(copy.copy(x), x)

    def test_copy_bytearray(self):
        x = yp_bytearray(b'abc')
        y = copy.copy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        x = yp_bytearray()
        y = copy.copy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)

    def test_copy_chrarray(self):
        x = yp_chrarray('abc')
        y = copy.copy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        x = yp_chrarray()
        y = copy.copy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)

    @yp_unittest.skip_user_defined_types
    def test_copy_inst_vanilla(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __eq__(self, other):
                return self.foo == other.foo
        x = C(42)
        self.assertEqual(copy.copy(x), x)

    @yp_unittest.skip_user_defined_types
    def test_copy_inst_copy(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __copy__(self):
                return C(self.foo)
            def __eq__(self, other):
                return self.foo == other.foo
        x = C(42)
        self.assertEqual(copy.copy(x), x)

    @yp_unittest.skip_user_defined_types
    def test_copy_inst_getinitargs(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __getinitargs__(self):
                return (self.foo,)
            def __eq__(self, other):
                return self.foo == other.foo
        x = C(42)
        self.assertEqual(copy.copy(x), x)

    @yp_unittest.skip_user_defined_types
    def test_copy_inst_getnewargs(self):
        class C(int):
            def __new__(cls, foo):
                self = int.__new__(cls)
                self.foo = foo
                return self
            def __getnewargs__(self):
                return self.foo,
            def __eq__(self, other):
                return self.foo == other.foo
        x = C(42)
        y = copy.copy(x)
        self.assertIsInstance(y, C)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        self.assertEqual(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_copy_inst_getnewargs_ex(self):
        class C(int):
            def __new__(cls, *, foo):
                self = int.__new__(cls)
                self.foo = foo
                return self
            def __getnewargs_ex__(self):
                return (), {'foo': self.foo}
            def __eq__(self, other):
                return self.foo == other.foo
        x = C(foo=42)
        y = copy.copy(x)
        self.assertIsInstance(y, C)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        self.assertEqual(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_copy_inst_getstate(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __getstate__(self):
                return {"foo": self.foo}
            def __eq__(self, other):
                return self.foo == other.foo
        x = C(42)
        self.assertEqual(copy.copy(x), x)

    @yp_unittest.skip_user_defined_types
    def test_copy_inst_setstate(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __setstate__(self, state):
                self.foo = state["foo"]
            def __eq__(self, other):
                return self.foo == other.foo
        x = C(42)
        self.assertEqual(copy.copy(x), x)

    @yp_unittest.skip_user_defined_types
    def test_copy_inst_getstate_setstate(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __getstate__(self):
                return self.foo
            def __setstate__(self, state):
                self.foo = state
            def __eq__(self, other):
                return self.foo == other.foo
        x = C(42)
        self.assertEqual(copy.copy(x), x)
        # State with boolean value is false (issue #25718)
        x = C(0.0)
        self.assertEqual(copy.copy(x), x)

    # The deepcopy() method

    def test_deepcopy_basic(self):
        x = yp_int(42)
        y = copy.deepcopy(x)
        self.assertEqual(y, x)

    def test_deepcopy_memo(self):
        # Tests of reflexive objects are under type-specific sections below.
        # This tests only repetitions of objects.
        x = yp_list()
        x = yp_list([x, x])
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        self.assertIsNot(y[0], x[0])
        self.assertIs(y[0], y[1])

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_issubclass(self):
        # XXX Note: there's no way to test the TypeError coming out of
        # issubclass() -- this can only happen when an extension
        # module defines a "type" that doesn't formally inherit from
        # type.
        class Meta(type):
            pass
        class C(metaclass=Meta):
            pass
        self.assertEqual(copy.deepcopy(C), C)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_deepcopy(self):
        class C(object):
            def __init__(self, foo):
                self.foo = foo
            def __deepcopy__(self, memo=None):
                return C(self.foo)
        x = C(42)
        y = copy.deepcopy(x)
        self.assertEqual(y.__class__, x.__class__)
        self.assertEqual(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_registry(self):
        class C(object):
            def __new__(cls, foo):
                obj = object.__new__(cls)
                obj.foo = foo
                return obj
        def pickle_C(obj):
            return (C, (obj.foo,))
        x = C(42)
        self.assertRaises(TypeError, copy.deepcopy, x)
        copyreg.pickle(C, pickle_C, C)
        y = copy.deepcopy(x)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_reduce_ex(self):
        class C(object):
            def __reduce_ex__(self, proto):
                c.append(1)
                return ""
            def __reduce__(self):
                self.fail("shouldn't call this")
        c = []
        x = C()
        y = copy.deepcopy(x)
        self.assertIs(y, x)
        self.assertEqual(c, [1])

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_reduce(self):
        class C(object):
            def __reduce__(self):
                c.append(1)
                return ""
        c = []
        x = C()
        y = copy.deepcopy(x)
        self.assertIs(y, x)
        self.assertEqual(c, [1])

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_cant(self):
        class C(object):
            def __getattribute__(self, name):
                if name.startswith("__reduce"):
                    raise AttributeError(name)
                return object.__getattribute__(self, name)
        x = C()
        self.assertRaises(copy.Error, copy.deepcopy, x)

    # Type-specific _deepcopy_xxx() methods

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_atomic(self):
        class Classic:
            pass
        class NewStyle(object):
            pass
        def f():
            pass
        tests = [None, 42, 2**100, 3.14, True, False, 1j,
                 "hello", "hello\u1234", f.__code__,
                 NewStyle, range(10), Classic, max, property()]
        for x in tests:
            self.assertIs(copy.deepcopy(x), x)

    # yp_deepcopy copies most objects, but the statically-allocated objects are lazy copies.
    def test_deepcopy_atomic_builtins(self):
        tests = [yp_None, yp_int(42), yp_True, yp_False, yp_dict,
                 yp_bytes(), yp_str(), yp_tuple(), yp_frozenset(), yp_frozendict(), yp_range(0)]
        for x in tests:
            self.assertIs(copy.deepcopy(x), x)

    def test_deepcopy_list(self):
        x = yp_list([[1, 2], 3])
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(x, y)
        self.assertIsNot(x[0], y[0])

    def test_deepcopy_reflexive_list(self):
        x = yp_list()
        x.append(x)
        y = copy.deepcopy(x)
        # TODO(skip_cmp_recursion) Detect recursion in comparison operators.
        # for op in comparisons:
        #     self.assertRaises(RecursionError, op, y, x)
        self.assertIsNot(y, x)
        self.assertIs(y[0], y)
        self.assertEqual(yp_len(y), 1)

    def test_deepcopy_empty_tuple(self):
        x = yp_tuple()
        y = copy.deepcopy(x)
        self.assertIs(x, y)

    def test_deepcopy_tuple(self):
        x = yp_tuple(([1, 2], 3))
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(x, y)
        self.assertIsNot(x[0], y[0])

    # yp_deepcopy copies most objects, as it's used in thread safety, where Python attempts to
    # lazy copy tuples-of-immutables.
    @yp_unittest.skip_not_applicable
    def test_deepcopy_tuple_of_immutables(self):
        x = yp_tuple(((1, 2), 3))
        y = copy.deepcopy(x)
        self.assertIs(x, y)

    def test_deepcopy_reflexive_tuple(self):
        x = yp_tuple(([],))
        x[0].append(x)
        y = copy.deepcopy(x)
        # TODO(skip_cmp_recursion) Detect recursion in comparison operators.
        # for op in comparisons:
        #     self.assertRaises(RecursionError, op, y, x)
        self.assertIsNot(y, x)
        self.assertIsNot(y[0], x[0])
        self.assertIs(y[0][0], y)

    @yp_unittest.skip_dict_deepcopy
    def test_deepcopy_dict(self):
        x = yp_dict({"foo": [1, 2], "bar": 3})
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(x, y)
        self.assertIsNot(x["foo"], y["foo"])

    @yp_unittest.skip_dict_deepcopy
    def test_deepcopy_reflexive_dict(self):
        x = yp_dict()
        x['foo'] = x
        y = copy.deepcopy(x)
        for op in order_comparisons:
            self.assertRaises(TypeError, op, y, x)
        # TODO(skip_cmp_recursion) Detect recursion in comparison operators.
        # for op in equality_comparisons:
        #     self.assertRaises(RecursionError, op, y, x)
        self.assertIsNot(y, x)
        self.assertIs(y['foo'], y)
        self.assertEqual(yp_len(y), 1)

    @yp_unittest.skip_deepcopy_memo
    def test_deepcopy_keepalive(self):
        memo = {}
        x = []
        y = copy.deepcopy(x, memo)
        self.assertIs(memo[id(memo)][0], x)

    @yp_unittest.skip_deepcopy_memo
    def test_deepcopy_dont_memo_immutable(self):
        memo = {}
        x = [1, 2, 3, 4]
        y = copy.deepcopy(x, memo)
        self.assertEqual(y, x)
        # There's the entry for the new list, and the keep alive.
        self.assertEqual(yp_len(memo), 2)

        memo = {}
        x = [(1, 2)]
        y = copy.deepcopy(x, memo)
        self.assertEqual(y, x)
        # Tuples with immutable contents are immutable for deepcopy.
        self.assertEqual(yp_len(memo), 2)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_inst_vanilla(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __eq__(self, other):
                return self.foo == other.foo
        x = C([42])
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_inst_deepcopy(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __deepcopy__(self, memo):
                return C(copy.deepcopy(self.foo, memo))
            def __eq__(self, other):
                return self.foo == other.foo
        x = C([42])
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        self.assertIsNot(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_inst_getinitargs(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __getinitargs__(self):
                return (self.foo,)
            def __eq__(self, other):
                return self.foo == other.foo
        x = C([42])
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        self.assertIsNot(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_inst_getnewargs(self):
        class C(int):
            def __new__(cls, foo):
                self = int.__new__(cls)
                self.foo = foo
                return self
            def __getnewargs__(self):
                return self.foo,
            def __eq__(self, other):
                return self.foo == other.foo
        x = C([42])
        y = copy.deepcopy(x)
        self.assertIsInstance(y, C)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        self.assertEqual(y.foo, x.foo)
        self.assertIsNot(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_inst_getnewargs_ex(self):
        class C(int):
            def __new__(cls, *, foo):
                self = int.__new__(cls)
                self.foo = foo
                return self
            def __getnewargs_ex__(self):
                return (), {'foo': self.foo}
            def __eq__(self, other):
                return self.foo == other.foo
        x = C(foo=[42])
        y = copy.deepcopy(x)
        self.assertIsInstance(y, C)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        self.assertEqual(y.foo, x.foo)
        self.assertIsNot(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_inst_getstate(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __getstate__(self):
                return {"foo": self.foo}
            def __eq__(self, other):
                return self.foo == other.foo
        x = C([42])
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        self.assertIsNot(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_inst_setstate(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __setstate__(self, state):
                self.foo = state["foo"]
            def __eq__(self, other):
                return self.foo == other.foo
        x = C([42])
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        self.assertIsNot(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_inst_getstate_setstate(self):
        class C:
            def __init__(self, foo):
                self.foo = foo
            def __getstate__(self):
                return self.foo
            def __setstate__(self, state):
                self.foo = state
            def __eq__(self, other):
                return self.foo == other.foo
        x = C([42])
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        self.assertIsNot(y.foo, x.foo)
        # State with boolean value is false (issue #25718)
        x = C([])
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y, x)
        self.assertIsNot(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_reflexive_inst(self):
        class C:
            pass
        x = C()
        x.foo = x
        y = copy.deepcopy(x)
        self.assertIsNot(y, x)
        self.assertIs(y.foo, y)

    # _reconstruct()

    @yp_unittest.skip_user_defined_types
    def test_reconstruct_string(self):
        class C(object):
            def __reduce__(self):
                return ""
        x = C()
        y = copy.copy(x)
        self.assertIs(y, x)
        y = copy.deepcopy(x)
        self.assertIs(y, x)

    @yp_unittest.skip_user_defined_types
    def test_reconstruct_nostate(self):
        class C(object):
            def __reduce__(self):
                return (C, ())
        x = C()
        x.foo = 42
        y = copy.copy(x)
        self.assertIs(y.__class__, x.__class__)
        y = copy.deepcopy(x)
        self.assertIs(y.__class__, x.__class__)

    @yp_unittest.skip_user_defined_types
    def test_reconstruct_state(self):
        class C(object):
            def __reduce__(self):
                return (C, (), self.__dict__)
            def __eq__(self, other):
                return self.__dict__ == other.__dict__
        x = C()
        x.foo = [42]
        y = copy.copy(x)
        self.assertEqual(y, x)
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_reconstruct_state_setstate(self):
        class C(object):
            def __reduce__(self):
                return (C, (), self.__dict__)
            def __setstate__(self, state):
                self.__dict__.update(state)
            def __eq__(self, other):
                return self.__dict__ == other.__dict__
        x = C()
        x.foo = [42]
        y = copy.copy(x)
        self.assertEqual(y, x)
        y = copy.deepcopy(x)
        self.assertEqual(y, x)
        self.assertIsNot(y.foo, x.foo)

    @yp_unittest.skip_user_defined_types
    def test_reconstruct_reflexive(self):
        class C(object):
            pass
        x = C()
        x.foo = x
        y = copy.deepcopy(x)
        self.assertIsNot(y, x)
        self.assertIs(y.foo, y)

    # Additions for Python 2.3 and pickle protocol 2

    @yp_unittest.skip_user_defined_types
    def test_reduce_4tuple(self):
        class C(list):
            def __reduce__(self):
                return (C, (), self.__dict__, iter(self))
            def __eq__(self, other):
                return (list(self) == list(other) and
                        self.__dict__ == other.__dict__)
        x = C([[1, 2], 3])
        y = copy.copy(x)
        self.assertEqual(x, y)
        self.assertIsNot(x, y)
        self.assertIs(x[0], y[0])
        y = copy.deepcopy(x)
        self.assertEqual(x, y)
        self.assertIsNot(x, y)
        self.assertIsNot(x[0], y[0])

    @yp_unittest.skip_user_defined_types
    def test_reduce_5tuple(self):
        class C(dict):
            def __reduce__(self):
                return (C, (), self.__dict__, None, self.items())
            def __eq__(self, other):
                return (dict(self) == dict(other) and
                        self.__dict__ == other.__dict__)
        x = C([("foo", [1, 2]), ("bar", 3)])
        y = copy.copy(x)
        self.assertEqual(x, y)
        self.assertIsNot(x, y)
        self.assertIs(x["foo"], y["foo"])
        y = copy.deepcopy(x)
        self.assertEqual(x, y)
        self.assertIsNot(x, y)
        self.assertIsNot(x["foo"], y["foo"])

    @yp_unittest.skip_user_defined_types
    def test_copy_slots(self):
        class C(object):
            __slots__ = ["foo"]
        x = C()
        x.foo = [42]
        y = copy.copy(x)
        self.assertIs(x.foo, y.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_slots(self):
        class C(object):
            __slots__ = ["foo"]
        x = C()
        x.foo = [42]
        y = copy.deepcopy(x)
        self.assertEqual(x.foo, y.foo)
        self.assertIsNot(x.foo, y.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_dict_subclass(self):
        class C(dict):
            def __init__(self, d=None):
                if not d:
                    d = {}
                self._keys = list(d.keys())
                super().__init__(d)
            def __setitem__(self, key, item):
                super().__setitem__(key, item)
                if key not in self._keys:
                    self._keys.append(key)
        x = C(d={'foo':0})
        y = copy.deepcopy(x)
        self.assertEqual(x, y)
        self.assertEqual(x._keys, y._keys)
        self.assertIsNot(x, y)
        x['bar'] = 1
        self.assertNotEqual(x, y)
        self.assertNotEqual(x._keys, y._keys)

    @yp_unittest.skip_user_defined_types
    def test_copy_list_subclass(self):
        class C(list):
            pass
        x = C([[1, 2], 3])
        x.foo = [4, 5]
        y = copy.copy(x)
        self.assertEqual(list(x), list(y))
        self.assertEqual(x.foo, y.foo)
        self.assertIs(x[0], y[0])
        self.assertIs(x.foo, y.foo)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_list_subclass(self):
        class C(list):
            pass
        x = C([[1, 2], 3])
        x.foo = [4, 5]
        y = copy.deepcopy(x)
        self.assertEqual(list(x), list(y))
        self.assertEqual(x.foo, y.foo)
        self.assertIsNot(x[0], y[0])
        self.assertIsNot(x.foo, y.foo)

    @yp_unittest.skip_user_defined_types
    def test_copy_tuple_subclass(self):
        class C(tuple):
            pass
        x = C([1, 2, 3])
        self.assertEqual(tuple(x), (1, 2, 3))
        y = copy.copy(x)
        self.assertEqual(tuple(y), (1, 2, 3))

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_tuple_subclass(self):
        class C(tuple):
            pass
        x = C([[1, 2], 3])
        self.assertEqual(tuple(x), ([1, 2], 3))
        y = copy.deepcopy(x)
        self.assertEqual(tuple(y), ([1, 2], 3))
        self.assertIsNot(x, y)
        self.assertIsNot(x[0], y[0])

    @yp_unittest.skip_user_defined_types
    def test_getstate_exc(self):
        class EvilState(object):
            def __getstate__(self):
                raise ValueError("ain't got no stickin' state")
        self.assertRaises(ValueError, copy.copy, EvilState())

    @yp_unittest.skip_function_deepcopy
    def test_copy_function(self):
        self.assertEqual(copy.copy(global_foo), global_foo)
        # def foo(x, y): return x+y
        foo = yp_function.with_parameters(lambda x, y: x+y, ((yp_str("x"), ), (yp_str("y"), )))
        self.assertEqual(copy.copy(foo), foo)
        # bar = lambda: None
        bar = yp_function.with_parameters(lambda: None, ())
        self.assertEqual(copy.copy(bar), bar)

    @yp_unittest.skip_function_deepcopy
    def test_deepcopy_function(self):
        self.assertEqual(copy.deepcopy(global_foo), global_foo)
        # def foo(x, y): return x+y
        foo = yp_function.with_parameters(lambda x, y: x+y, ((yp_str("x"), ), (yp_str("y"), )))
        self.assertEqual(copy.deepcopy(foo), foo)
        # bar = lambda: None
        bar = yp_function.with_parameters(lambda: None, ())
        self.assertEqual(copy.deepcopy(bar), bar)

    def _check_weakref(self, _copy):
        class C(object):
            pass
        obj = C()
        x = weakref.ref(obj)
        y = _copy(x)
        self.assertIs(y, x)
        del obj
        y = _copy(x)
        self.assertIs(y, x)

    @yp_unittest.skip_weakref
    def test_copy_weakref(self):
        self._check_weakref(copy.copy)

    @yp_unittest.skip_weakref
    def test_deepcopy_weakref(self):
        self._check_weakref(copy.deepcopy)

    def _check_copy_weakdict(self, _dicttype):
        class C(object):
            pass
        a, b, c, d = [C() for i in range(4)]
        u = _dicttype()
        u[a] = b
        u[c] = d
        v = copy.copy(u)
        self.assertIsNot(v, u)
        self.assertEqual(v, u)
        self.assertEqual(v[a], b)
        self.assertEqual(v[c], d)
        self.assertEqual(yp_len(v), 2)
        del c, d
        support.gc_collect()  # For PyPy or other GCs.
        self.assertEqual(yp_len(v), 1)
        x, y = C(), C()
        # The underlying containers are decoupled
        v[x] = y
        self.assertNotIn(x, u)

    @yp_unittest.skip_weakref
    def test_copy_weakkeydict(self):
        self._check_copy_weakdict(weakref.WeakKeyDictionary)

    @yp_unittest.skip_weakref
    def test_copy_weakvaluedict(self):
        self._check_copy_weakdict(weakref.WeakValueDictionary)

    @yp_unittest.skip_weakref
    def test_deepcopy_weakkeydict(self):
        class C(object):
            def __init__(self, i):
                self.i = i
        a, b, c, d = [C(i) for i in range(4)]
        u = weakref.WeakKeyDictionary()
        u[a] = b
        u[c] = d
        # Keys aren't copied, values are
        v = copy.deepcopy(u)
        self.assertNotEqual(v, u)
        self.assertEqual(yp_len(v), 2)
        self.assertIsNot(v[a], b)
        self.assertIsNot(v[c], d)
        self.assertEqual(v[a].i, b.i)
        self.assertEqual(v[c].i, d.i)
        del c
        support.gc_collect()  # For PyPy or other GCs.
        self.assertEqual(yp_len(v), 1)

    @yp_unittest.skip_weakref
    def test_deepcopy_weakvaluedict(self):
        class C(object):
            def __init__(self, i):
                self.i = i
        a, b, c, d = [C(i) for i in range(4)]
        u = weakref.WeakValueDictionary()
        u[a] = b
        u[c] = d
        # Keys are copied, values aren't
        v = copy.deepcopy(u)
        self.assertNotEqual(v, u)
        self.assertEqual(yp_len(v), 2)
        (x, y), (z, t) = sorted(v.items(), key=lambda pair: pair[0].i)
        self.assertIsNot(x, a)
        self.assertEqual(x.i, a.i)
        self.assertIs(y, b)
        self.assertIsNot(z, c)
        self.assertEqual(z.i, c.i)
        self.assertIs(t, d)
        del x, y, z, t
        del d
        support.gc_collect()  # For PyPy or other GCs.
        self.assertEqual(yp_len(v), 1)

    @yp_unittest.skip_user_defined_types
    def test_deepcopy_bound_method(self):
        class Foo(object):
            def m(self):
                pass
        f = Foo()
        f.b = f.m
        g = copy.deepcopy(f)
        self.assertEqual(g.m, g.b)
        self.assertIs(g.b.__self__, g)
        g.b()


# def global_foo(x, y): return x+y
global_foo = yp_function.with_parameters(lambda x, y: x+y, ((yp_str("x"), ), (yp_str("y"), )))

if __name__ == "__main__":
    yp_unittest.main()
