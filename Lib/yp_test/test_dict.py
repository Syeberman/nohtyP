from yp import *
from yp_test import yp_unittest
from yp_test import support

import collections, random, string
import collections.abc
import gc, weakref
import pickle

# Extra assurance that we're not accidentally testing Python's dict
def dict( *args, **kwargs ): raise NotImplementedError( "convert script to yp_dict here" )


class DictTest(yp_unittest.TestCase):

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_invalid_keyword_arguments(self):
        class Custom(dict):
            pass
        for invalid in {1 : 2}, Custom({1 : 2}):
            with self.assertRaises(TypeError):
                yp_dict(**invalid)
            with self.assertRaises(TypeError):
                yp_dict().update(**invalid)

    def test_constructor(self):
        # calling built-in types without argument must return empty
        self.assertEqual(yp_len(yp_dict()), 0)
        self.assertIsNot(yp_dict(), yp_dict())

    def test_literal_constructor(self):
        # check literal constructor for different sized dicts
        # (to exercise the BUILD_MAP oparg).
        for n in (0, 1, 6, 256, 400):
            items = [(''.join(random.sample(string.ascii_letters, 8)), i)
                     for i in range(n)]
            random.shuffle(items)
            formatted_items = ('{!r}: {:d}'.format(k, v) for k, v in items)
            dictliteral = '{' + ', '.join(formatted_items) + '}'
            self.assertEqual(yp_dict(eval(dictliteral)), yp_dict(items))

    def test_bool(self):
        self.assertFalse(yp_dict())
        self.assertTrue(yp_dict({1: 2}))
        self.assertIs(yp_bool(yp_dict()), yp_False)
        self.assertIs(yp_bool(yp_dict({1: 2})), yp_True)

    @yp_unittest.skip( "TODO: Implement yp_str/yp_repr" )
    def test_keys(self):
        d = yp_dict()
        self.assertEqual(set(d.keys()), set())
        d = yp_dict({'a': 1, 'b': 2})
        k = d.keys()
        self.assertEqual(yp_set(k), {'a', 'b'})
        self.assertIn('a', k)
        self.assertIn('b', k)
        self.assertIn('a', d)
        self.assertIn('b', d)
        self.assertRaises(TypeError, d.keys, None)
        self.assertEqual(repr(yp_dict(a=1).keys()), "dict_keys(['a'])")

    @yp_unittest.skip( "TODO: Implement yp_str/yp_repr" )
    def test_values(self):
        d = yp_dict()
        self.assertEqual(set(d.values()), set())
        d = yp_dict({1:2})
        self.assertEqual(set(d.values()), {2})
        self.assertRaises(TypeError, d.values, None)
        self.assertEqual(repr(yp_dict(a=1).values()), "dict_values([1])")

    @yp_unittest.skip( "TODO: Implement yp_str/yp_repr" )
    def test_items(self):
        d = yp_dict()
        self.assertEqual(set(d.items()), set())

        d = yp_dict({1:2})
        self.assertEqual(set(d.items()), {(1, 2)})
        self.assertRaises(TypeError, d.items, None)
        self.assertEqual(repr(yp_dict(a=1).items()), "dict_items([('a', 1)])")

    def test_contains(self):
        d = yp_dict()
        self.assertNotIn('a', d)
        # XXX Not applicable to nohtyP
        #self.assertFalse('a' in d)
        #self.assertTrue('a' not in d)
        d = yp_dict({'a': 1, 'b': 2})
        self.assertIn('a', d)
        self.assertIn('b', d)
        self.assertNotIn('c', d)

        self.assertRaises(TypeError, d.__contains__)

    def test_len(self):
        d = yp_dict()
        self.assertEqual(yp_len(d), 0)
        d = yp_dict({'a': 1, 'b': 2})
        self.assertEqual(yp_len(d), 2)

    def test_getitem(self):
        d = yp_dict({'a': 1, 'b': 2})
        self.assertEqual(d['a'], 1)
        self.assertEqual(d['b'], 2)
        d['c'] = 3
        d['a'] = 4
        self.assertEqual(d['c'], 3)
        self.assertEqual(d['a'], 4)
        del d['b']
        self.assertEqual(d, yp_dict({'a': 4, 'c': 3}))

        self.assertRaises(TypeError, d.__getitem__)

    @yp_unittest.skip("REWORK: nohtyP dicts don't store user-defined types")
    def test_getitem_badobj(self):
        class BadEq(object):
            def __eq__(self, other):
                raise Exc()
            def __hash__(self):
                return 24

        d = yp_dict()
        d[BadEq()] = 42
        self.assertRaises(KeyError, d.__getitem__, 23)

        class Exc(Exception): pass

        class BadHash(object):
            fail = False
            def __hash__(self):
                if self.fail:
                    raise Exc()
                else:
                    return 42

        x = BadHash()
        d[x] = 42
        x.fail = True
        self.assertRaises(Exc, d.__getitem__, x)

    def test_clear(self):
        d = yp_dict({1:1, 2:2, 3:3})
        d.clear()
        self.assertEqual(d, yp_dict())

        self.assertRaises(TypeError, d.clear, None)

    def test_update(self):
        d = yp_dict()
        d.update(yp_dict({1:100}))
        d.update(yp_dict({2:20}))
        d.update(yp_dict({1:1, 2:2, 3:3}))
        self.assertEqual(d, yp_dict({1:1, 2:2, 3:3}))

        d.update()
        self.assertEqual(d, yp_dict({1:1, 2:2, 3:3}))

        self.assertRaises((TypeError, AttributeError), d.update, None)

        class SimpleUserDict:
            def __init__(self):
                self.d = yp_dict({1:1, 2:2, 3:3})
            def keys(self):
                return self.d.keys()
            def __getitem__(self, i):
                return self.d[i]
        d.clear()
        d.update(SimpleUserDict())
        self.assertEqual(d, yp_dict({1:1, 2:2, 3:3}))

        # Pick a Python exception that nohtyP knows, but that isn't likely to occur naturally
        Exc = ImportError

        d.clear()
        class FailingUserDict:
            def keys(self):
                raise Exc
        self.assertRaises(Exc, d.update, FailingUserDict())

        class FailingUserDict:
            def keys(self):
                class BogonIter:
                    def __init__(self):
                        self.i = 1
                    def __iter__(self):
                        return self
                    def __next__(self):
                        if self.i:
                            self.i = 0
                            return 'a'
                        raise Exc
                return BogonIter()
            def __getitem__(self, key):
                return key
        self.assertRaises(Exc, d.update, FailingUserDict())

        class FailingUserDict:
            def keys(self):
                class BogonIter:
                    def __init__(self):
                        self.i = ord('a')
                    def __iter__(self):
                        return self
                    def __next__(self):
                        if self.i <= ord('z'):
                            rtn = chr(self.i)
                            self.i += 1
                            return rtn
                        raise StopIteration
                return BogonIter()
            def __getitem__(self, key):
                raise Exc
        self.assertRaises(Exc, d.update, FailingUserDict())

        class badseq(object):
            def __iter__(self):
                return self
            def __next__(self):
                raise Exc()

        self.assertRaises(Exc, yp_dict().update, badseq())

        self.assertRaises(ValueError, yp_dict().update, [(1, 2, 3)])

    def test_fromkeys(self):
        self.assertEqual(yp_dict.fromkeys('abc'), yp_dict({'a':None, 'b':None, 'c':None}))
        d = yp_dict()
        self.assertIsNot(d.fromkeys('abc'), d)
        self.assertEqual(d.fromkeys('abc'), yp_dict({'a':None, 'b':None, 'c':None}))
        self.assertEqual(d.fromkeys((4,5),0), yp_dict({4:0, 5:0}))
        self.assertEqual(d.fromkeys([]), yp_dict())
        def g():
            yield 1
        self.assertEqual(d.fromkeys(g()), yp_dict({1:None}))
        self.assertRaises(TypeError, yp_dict().fromkeys, 3)

    @yp_unittest.skip("REWORK: nohtyP dicts aren't naturally subclassable")
    def test_fromkeys_subclass1(self):
        class dictlike(yp_dict): pass
        self.assertEqual(dictlike.fromkeys('a'), yp_dict({'a':None}))
        self.assertEqual(dictlike().fromkeys('a'), yp_dict({'a':None}))
        self.assertIsInstance(dictlike.fromkeys('a'), dictlike)
        self.assertIsInstance(dictlike().fromkeys('a'), dictlike)
        class mydict(yp_dict):
            def __new__(cls):
                return collections.UserDict()
        ud = mydict.fromkeys('ab')
        self.assertEqual(ud, yp_dict({'a':None, 'b':None}))
        self.assertIsInstance(ud, collections.UserDict)
        self.assertRaises(TypeError, yp_dict.fromkeys)

        class Exc(Exception): pass

        class baddict1(yp_dict):
            def __init__(self):
                raise Exc()

        self.assertRaises(Exc, baddict1.fromkeys, [1])

    def test_fromkeys_badseq(self):
        class Exc(Exception): pass

        class BadSeq(object):
            def __iter__(self):
                return self
            def __next__(self):
                raise Exc()

        self.assertRaises(Exc, yp_dict.fromkeys, BadSeq())

    @yp_unittest.skip("REWORK: nohtyP dicts aren't naturally subclassable")
    def test_fromkeys_subclass2(self):
        class Exc(Exception): pass

        class baddict2(yp_dict):
            def __setitem__(self, key, value):
                raise Exc()

        self.assertRaises(Exc, baddict2.fromkeys, [1])

    def test_fromkeys_fastpath(self):
        # test fast path for dictionary inputs
        d = yp_dict(zip(range(6), range(6)))
        self.assertEqual(yp_dict.fromkeys(d, 0), yp_dict(zip(range(6), [0]*6)))

    @yp_unittest.skip("REWORK: nohtyP dicts aren't naturally subclassable")
    def test_fromkeys_fastpath_subclass(self):
        class baddict3(yp_dict):
            def __new__(cls):
                return d
        d = yp_dict({i : i for i in range(10)})
        res = d.copy()
        res.update(a=None, b=None, c=None)
        self.assertEqual(baddict3.fromkeys({"a", "b", "c"}), res)

    def test_copy(self):
        d = yp_dict({1:1, 2:2, 3:3})
        self.assertEqual(d.copy(), yp_dict({1:1, 2:2, 3:3}))
        self.assertEqual(yp_dict().copy(), yp_dict())
        self.assertRaises(TypeError, d.copy, None)

    def test_get(self):
        d = yp_dict()
        self.assertIs(d.get('c'), yp_None)
        self.assertEqual(d.get('c', 3), 3)
        d = yp_dict({'a': 1, 'b': 2})
        self.assertIs(d.get('c'), yp_None)
        self.assertEqual(d.get('c', 3), 3)
        self.assertEqual(d.get('a'), 1)
        self.assertEqual(d.get('a', 3), 1)
        self.assertRaises(TypeError, d.get)
        self.assertRaises(TypeError, d.get, None, None, None)

    def test_setdefault(self):
        # yp_dict.setdefault()
        d = yp_dict()
        self.assertIs(d.setdefault('key0'), yp_None)
        d.setdefault('key0', [])
        self.assertIs(d.setdefault('key0'), yp_None)
        d.setdefault('key', []).append(3)
        self.assertEqual(d['key'][0], 3)
        d.setdefault('key', []).append(4)
        self.assertEqual(yp_len(d['key']), 2)
        self.assertRaises(TypeError, d.setdefault)

    @yp_unittest.skip("REWORK: nohtyP dicts don't store user-defined types")
    def test_setdefault_badobj(self):
        class Exc(Exception): pass

        class BadHash(object):
            fail = False
            def __hash__(self):
                if self.fail:
                    raise Exc()
                else:
                    return 42

        x = BadHash()
        d[x] = 42
        x.fail = True
        self.assertRaises(Exc, d.setdefault, x, [])

    @yp_unittest.skip("REWORK: nohtyP dicts don't store user-defined types")
    def test_setdefault_atomic(self):
        # Issue #13521: setdefault() calls __hash__ and __eq__ only once.
        class Hashed(object):
            def __init__(self):
                self.hash_count = 0
                self.eq_count = 0
            def __hash__(self):
                self.hash_count += 1
                return 42
            def __eq__(self, other):
                self.eq_count += 1
                return id(self) == id(other)
        hashed1 = Hashed()
        y = yp_dict({hashed1: 5})
        hashed2 = Hashed()
        y.setdefault(hashed2, [])
        self.assertEqual(hashed1.hash_count, 1)
        self.assertEqual(hashed2.hash_count, 1)
        self.assertEqual(hashed1.eq_count + hashed2.eq_count, 1)

    def test_setitem_resize_inline(self):
        # Ensure _ypDict_resize handles moving data back and forth from the inline buff
        # TODO Dip into the internals to ensure we're testing what we think
        ints = yp_list(range((0x80*2)//3 - 1))
        d = yp_dict()       # inline
        self.assertEqual(d, yp_dict())
        d[-100] = None      # still inline
        self.assertEqual(d, yp_dict({-100: None}))
        for i in ints: d[i] = None  # now in seperate buff
        self.assertEqual(yp_len(d), len(ints)+1)
        self.assertIn(-100, d)
        for i in ints: del d[i]     # still in same buff (no resize on remove)
        self.assertEqual(d, yp_dict({-100: None}))
        d[-101] = None      # back to inline
        self.assertEqual(d, yp_dict({-100: None, -101: None}))

    @yp_unittest.skip("REWORK: nohtyP dicts don't store user-defined types")
    def test_setitem_atomic_at_resize(self):
        class Hashed(object):
            def __init__(self):
                self.hash_count = 0
                self.eq_count = 0
            def __hash__(self):
                self.hash_count += 1
                return 42
            def __eq__(self, other):
                self.eq_count += 1
                return id(self) == id(other)
        hashed1 = Hashed()
        # 5 items
        y = yp_dict({hashed1: 5, 0: 0, 1: 1, 2: 2, 3: 3})
        hashed2 = Hashed()
        # 6th item forces a resize
        y[hashed2] = []
        self.assertEqual(hashed1.hash_count, 1)
        self.assertEqual(hashed2.hash_count, 1)
        self.assertEqual(hashed1.eq_count + hashed2.eq_count, 1)

    def check_popitem(self, log2sizes):
        # yp_dict.popitem()
        for copymode in -1, +1:
            # -1: b has same structure as a
            # +1: b is a.copy()
            for log2size in log2sizes:
                size = 2**log2size
                a = yp_dict()
                b = yp_dict()
                for i in range(size):
                    a[repr(i)] = i
                    if copymode < 0:
                        b[repr(i)] = i
                if copymode > 0:
                    b = a.copy()
                for i in range(size):
                    ka, va = ta = a.popitem()
                    self.assertEqual(va, int(str(ka)))
                    kb, vb = tb = b.popitem()
                    self.assertEqual(vb, int(str(kb)))
                    self.assertFalse(yp_bool(copymode < 0 and ta != tb))
                self.assertFalse(a)
                self.assertFalse(b)

        d = yp_dict()
        self.assertRaises(KeyError, d.popitem)

    def test_popitem(self):
        self.check_popitem(range(9))

    @support.requires_resource('cpu')
    def test_popitem_cpu(self):
        self.check_popitem(range(9,12))

    def test_pop(self):
        # Tests for pop with specified key
        d = yp_dict()
        k, v = 'abc', 'def'
        d[k] = v
        self.assertRaises(KeyError, d.pop, 'ghi')

        self.assertEqual(d.pop(k), v)
        self.assertEqual(yp_len(d), 0)

        self.assertRaises(KeyError, d.pop, k)

        self.assertEqual(d.pop(k, v), v)
        d[k] = v
        self.assertEqual(d.pop(k, 1), v)

        self.assertRaises(TypeError, d.pop)

    @yp_unittest.skip("REWORK: nohtyP dicts don't store user-defined types")
    def test_pop_badobj(self):
        class Exc(Exception): pass

        class BadHash(object):
            fail = False
            def __hash__(self):
                if self.fail:
                    raise Exc()
                else:
                    return 42

        x = BadHash()
        d[x] = 42
        x.fail = True
        self.assertRaises(Exc, d.pop, x)

    @yp_unittest.skip( "TODO: Implement mutation detection in nohtyP?" )
    def test_mutating_iteration(self):
        # changing yp_dict size during iteration
        d = yp_dict()
        d[1] = 1
        with self.assertRaises(RuntimeError):
            for i in d:
                d[i+1] = 1

    @yp_unittest.skip("REWORK: nohtyP dicts don't store user-defined types")
    def test_mutating_lookup(self):
        # changing yp_dict during a lookup (issue #14417)
        class NastyKey:
            mutate_dict = None

            def __init__(self, value):
                self.value = value

            def __hash__(self):
                # hash collision!
                return 1

            def __eq__(self, other):
                if NastyKey.mutate_dict:
                    mydict, key = NastyKey.mutate_dict
                    NastyKey.mutate_dict = None
                    del mydict[key]
                return self.value == other.value

        key1 = NastyKey(1)
        key2 = NastyKey(2)
        d = yp_dict({key1: 1})
        NastyKey.mutate_dict = (d, key1)
        d[key2] = 2
        self.assertEqual(d, yp_dict({key2: 2}))

    @yp_unittest.skip( "TODO: Implement yp_str/yp_repr" )
    def test_repr(self):
        d = yp_dict()
        self.assertEqual(repr(d), '{}')
        d[1] = 2
        self.assertEqual(repr(d), '{1: 2}')
        d = yp_dict()
        d[1] = d
        self.assertEqual(repr(d), '{1: {...}}')

        class Exc(Exception): pass

        class BadRepr(object):
            def __repr__(self):
                raise Exc()

        d = yp_dict({1: BadRepr()})
        self.assertRaises(Exc, repr, d)

    def test_eq(self):
        self.assertEqual(yp_dict(), yp_dict())
        self.assertEqual(yp_dict({1: 2}), yp_dict({1: 2}))

    @yp_unittest.skip("REWORK: nohtyP dicts don't store user-defined types")
    def test_eq_badobj(self):
        class Exc(Exception): pass

        class BadCmp(object):
            def __eq__(self, other):
                raise Exc()
            def __hash__(self):
                return 1

        d1 = yp_dict({BadCmp(): 1})
        d2 = yp_dict({1: 1})

        with self.assertRaises(Exc):
            d1 == d2

    def test_keys_contained(self):
        self.helper_keys_contained(lambda x: x.keys())
        self.helper_keys_contained(lambda x: x.items())

    def helper_keys_contained(self, fn):
        # Test rich comparisons against yp_dict key views, which should behave the
        # same as sets.
        empty = fn(yp_dict())
        empty2 = fn(yp_dict())
        smaller = fn(yp_dict({1:1, 2:2}))
        larger = fn(yp_dict({1:1, 2:2, 3:3}))
        larger2 = fn(yp_dict({1:1, 2:2, 3:3}))
        larger3 = fn(yp_dict({4:1, 2:2, 3:3}))

        self.assertTrue(smaller <  larger)
        self.assertTrue(smaller <= larger)
        self.assertTrue(larger >  smaller)
        self.assertTrue(larger >= smaller)

        self.assertFalse(smaller >= larger)
        self.assertFalse(smaller >  larger)
        self.assertFalse(larger  <= smaller)
        self.assertFalse(larger  <  smaller)

        self.assertFalse(smaller <  larger3)
        self.assertFalse(smaller <= larger3)
        self.assertFalse(larger3 >  smaller)
        self.assertFalse(larger3 >= smaller)

        # Inequality strictness
        self.assertTrue(larger2 >= larger)
        self.assertTrue(larger2 <= larger)
        self.assertFalse(larger2 > larger)
        self.assertFalse(larger2 < larger)

        self.assertTrue(larger == larger2)
        self.assertTrue(smaller != larger)

        # There is an optimization on the zero-element case.
        self.assertTrue(empty == empty2)
        self.assertFalse(empty != empty2)
        self.assertFalse(empty == smaller)
        self.assertTrue(empty != smaller)

        # With the same size, an elementwise compare happens
        self.assertTrue(larger != larger3)
        self.assertFalse(larger == larger3)

    @yp_unittest.skip("REWORK: nohtyP dicts don't store user-defined types")
    def test_errors_in_view_containment_check(self):
        class C:
            def __eq__(self, other):
                raise RuntimeError

        d1 = yp_dict({1: C()})
        d2 = yp_dict({1: C()})
        with self.assertRaises(RuntimeError):
            d1.items() == d2.items()
        with self.assertRaises(RuntimeError):
            d1.items() != d2.items()
        with self.assertRaises(RuntimeError):
            d1.items() <= d2.items()
        with self.assertRaises(RuntimeError):
            d1.items() >= d2.items()

        d3 = yp_dict({1: C(), 2: C()})
        with self.assertRaises(RuntimeError):
            d2.items() < d3.items()
        with self.assertRaises(RuntimeError):
            d3.items() > d2.items()

    def test_dictview_set_operations_on_keys(self):
        k1 = yp_dict({1:1, 2:2}).keys()
        k2 = yp_dict({1:1, 2:2, 3:3}).keys()
        k3 = yp_dict({4:4}).keys()

        self.assertEqual(k1 - k2, set())
        self.assertEqual(k1 - k3, {1,2})
        self.assertEqual(k2 - k1, {3})
        self.assertEqual(k3 - k1, {4})
        self.assertEqual(k1 & k2, {1,2})
        self.assertEqual(k1 & k3, set())
        self.assertEqual(k1 | k2, {1,2,3})
        self.assertEqual(k1 ^ k2, {3})
        self.assertEqual(k1 ^ k3, {1,2,4})

    def test_dictview_set_operations_on_items(self):
        k1 = yp_dict({1:1, 2:2}).items()
        k2 = yp_dict({1:1, 2:2, 3:3}).items()
        k3 = yp_dict({4:4}).items()

        self.assertEqual(k1 - k2, set())
        self.assertEqual(k1 - k3, {(1,1), (2,2)})
        self.assertEqual(k2 - k1, {(3,3)})
        self.assertEqual(k3 - k1, {(4,4)})
        self.assertEqual(k1 & k2, {(1,1), (2,2)})
        self.assertEqual(k1 & k3, set())
        self.assertEqual(k1 | k2, {(1,1), (2,2), (3,3)})
        self.assertEqual(k1 ^ k2, {(3,3)})
        self.assertEqual(k1 ^ k3, {(1,1), (2,2), (4,4)})

    def test_dictview_mixed_set_operations(self):
        # Just a few for .keys()
        self.assertTrue(yp_dict({1:1}).keys() == {1})
        self.assertTrue({1} == yp_dict({1:1}).keys())
        self.assertEqual(yp_dict({1:1}).keys() | {2}, {1, 2})
        self.assertEqual({2} | yp_dict({1:1}).keys(), {1, 2})
        # And a few for .items()
        self.assertTrue(yp_dict({1:1}).items() == {(1,1)})
        self.assertTrue({(1,1)} == yp_dict({1:1}).items())
        self.assertEqual(yp_dict({1:1}).items() | {2}, {(1,1), 2})
        self.assertEqual({2} | yp_dict({1:1}).items(), {(1,1), 2})

    @yp_unittest.skip( "TODO: Implement __missing__ (or similar) in nohtyP?" )
    def test_missing(self):
        # Make sure yp_dict doesn't have a __missing__ method
        self.assertFalse(hasattr(yp_dict, "__missing__"))
        self.assertFalse(hasattr(yp_dict(), "__missing__"))
        # Test several cases:
        # (D) subclass defines __missing__ method returning a value
        # (E) subclass defines __missing__ method raising RuntimeError
        # (F) subclass sets __missing__ instance variable (no effect)
        # (G) subclass doesn't define __missing__ at a all
        class D(yp_dict):
            def __missing__(self, key):
                return 42
        d = D(yp_dict({1: 2, 3: 4}))
        self.assertEqual(d[1], 2)
        self.assertEqual(d[3], 4)
        self.assertNotIn(2, d)
        self.assertNotIn(2, d.keys())
        self.assertEqual(d[2], 42)

        class E(yp_dict):
            def __missing__(self, key):
                raise RuntimeError(key)
        e = E()
        with self.assertRaises(RuntimeError) as c:
            e[42]
        self.assertEqual(c.exception.args, (42,))

        class F(yp_dict):
            def __init__(self):
                # An instance variable __missing__ should have no effect
                self.__missing__ = lambda key: None
        f = F()
        with self.assertRaises(KeyError) as c:
            f[42]
        self.assertEqual(c.exception.args, (42,))

        class G(yp_dict):
            pass
        g = G()
        with self.assertRaises(KeyError) as c:
            g[42]
        self.assertEqual(c.exception.args, (42,))

    def test_tuple_keyerror(self):
        # SF #1576657
        d = yp_dict()
        with self.assertRaises(KeyError) as c:
            d[(1,)]
        #self.assertEqual(c.exception.args, ((1,),)) # not applicable to nohtyP

    @yp_unittest.skip("REWORK: nohtyP dicts don't store user-defined types")
    def test_bad_key(self):
        # Dictionary lookups should fail if __eq__() raises an exception.
        class CustomException(Exception):
            pass

        class BadDictKey:
            def __hash__(self):
                return hash(self.__class__)

            def __eq__(self, other):
                if isinstance(other, self.__class__):
                    raise CustomException
                return other

        d = yp_dict()
        x1 = BadDictKey()
        x2 = BadDictKey()
        d[x1] = 1
        for stmt in ['d[x2] = 2',
                     'z = d[x2]',
                     'x2 in d',
                     'd.get(x2)',
                     'd.setdefault(x2, 42)',
                     'd.pop(x2)',
                     'd.update({x2: 2})']:
            with self.assertRaises(CustomException):
                exec(stmt, locals())

    def test_resize1(self):
        # Dict resizing bug, found by Jack Jansen in 2.2 CVS development.
        # This version got an assert failure in debug build, infinite loop in
        # release build.  Unfortunately, provoking this kind of stuff requires
        # a mix of inserts and deletes hitting exactly the right hash codes in
        # exactly the right order, and I can't think of a randomized approach
        # that would be *likely* to hit a failing case in reasonable time.

        d = yp_dict()
        for i in range(5):
            d[i] = i
        for i in range(5):
            del d[i]
        for i in range(5, 9):  # i==8 was the problem
            d[i] = i

    @yp_unittest.skip("REWORK: nohtyP dicts don't store user-defined types")
    def test_resize2(self):
        # Another yp_dict resizing bug (SF bug #1456209).
        # This caused Segmentation faults or Illegal instructions.

        class X(object):
            def __hash__(self):
                return 5
            def __eq__(self, other):
                if resizing:
                    d.clear()
                return False
        d = yp_dict()
        resizing = False
        d[X()] = 1
        d[X()] = 2
        d[X()] = 3
        d[X()] = 4
        d[X()] = 5
        # now trigger a resize
        resizing = True
        d[9] = 6

    def test_empty_presized_dict_in_freelist(self):
        # Bug #3537: if an empty but presized yp_dict with a size larger
        # than 7 was in the freelist, it triggered an assertion failure
        with self.assertRaises(ZeroDivisionError):
            d = yp_dict({'a': 1 // 0, 'b': None, 'c': None, 'd': None, 'e': None,
                 'f': None, 'g': None, 'h': None})
        d = yp_dict()

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_container_iterator(self):
        # Bug #3680: tp_traverse was not implemented for dictiter and
        # dictview objects.
        class C(object):
            pass
        views = (yp_dict.items, yp_dict.values, yp_dict.keys)
        for v in views:
            obj = C()
            ref = weakref.ref(obj)
            container = yp_dict({obj: 1})
            obj.v = v(container)
            obj.x = iter(obj.v)
            del obj, container
            gc.collect()
            self.assertIs(ref(), None, "Cycle was not collected")

    def _not_tracked(self, t):
        # Nested containers can take several collections to untrack
        gc.collect()
        gc.collect()
        self.assertFalse(gc.is_tracked(t), t)

    def _tracked(self, t):
        self.assertTrue(gc.is_tracked(t), t)
        gc.collect()
        gc.collect()
        self.assertTrue(gc.is_tracked(t), t)

    @yp_unittest.skip("Not applicable to nohtyP")
    @support.cpython_only
    def test_track_literals(self):
        # Test GC-optimization of yp_dict literals
        x, y, z, w = 1.5, "a", (1, None), []

        self._not_tracked(yp_dict())
        self._not_tracked(yp_dict({x:(), y:x, z:1}))
        self._not_tracked(yp_dict({1: "a", "b": 2}))
        self._not_tracked(yp_dict({1: 2, (None, True, False, ()): int}))
        self._not_tracked(yp_dict({1: object()}))

        # Dicts with mutable elements are always tracked, even if those
        # elements are not tracked right now.
        self._tracked(yp_dict({1: []}))
        self._tracked(yp_dict({1: ([],)}))
        self._tracked(yp_dict({1: yp_dict()}))
        self._tracked(yp_dict({1: set()}))

    @yp_unittest.skip("Not applicable to nohtyP")
    @support.cpython_only
    def test_track_dynamic(self):
        # Test GC-optimization of dynamically-created dicts
        class MyObject(object):
            pass
        x, y, z, w, o = 1.5, "a", (1, object()), [], MyObject()

        d = yp_dict()
        self._not_tracked(d)
        d[1] = "a"
        self._not_tracked(d)
        d[y] = 2
        self._not_tracked(d)
        d[z] = 3
        self._not_tracked(d)
        self._not_tracked(d.copy())
        d[4] = w
        self._tracked(d)
        self._tracked(d.copy())
        d[4] = None
        self._not_tracked(d)
        self._not_tracked(d.copy())

        # dd isn't tracked right now, but it may mutate and therefore d
        # which contains it must be tracked.
        d = yp_dict()
        dd = yp_dict()
        d[1] = dd
        self._not_tracked(dd)
        self._tracked(d)
        dd[1] = d
        self._tracked(dd)

        d = yp_dict.fromkeys([x, y, z])
        self._not_tracked(d)
        dd = yp_dict()
        dd.update(d)
        self._not_tracked(dd)
        d = yp_dict.fromkeys([x, y, z, o])
        self._tracked(d)
        dd = yp_dict()
        dd.update(d)
        self._tracked(dd)

        d = yp_dict(x=x, y=y, z=z)
        self._not_tracked(d)
        d = yp_dict(x=x, y=y, z=z, w=w)
        self._tracked(d)
        d = yp_dict()
        d.update(x=x, y=y, z=z)
        self._not_tracked(d)
        d.update(w=w)
        self._tracked(d)

        d = yp_dict([(x, y), (z, 1)])
        self._not_tracked(d)
        d = yp_dict([(x, y), (z, w)])
        self._tracked(d)
        d = yp_dict()
        d.update([(x, y), (z, 1)])
        self._not_tracked(d)
        d.update([(x, y), (z, w)])
        self._tracked(d)

    @yp_unittest.skip("Not applicable to nohtyP")
    @support.cpython_only
    def test_track_subtypes(self):
        # Dict subtypes are always tracked
        class MyDict(yp_dict):
            pass
        self._tracked(MyDict())

    @yp_unittest.skip( "TODO: Implement pickling in nohtyP" )
    def test_iterator_pickling(self):
        data = yp_dict({1:"a", 2:"b", 3:"c"})
        it = iter(data)
        d = pickle.dumps(it)
        it = pickle.loads(d)
        self.assertEqual(sorted(it), sorted(data))

        it = pickle.loads(d)
        try:
            drop = next(it)
        except StopIteration:
            return
        d = pickle.dumps(it)
        it = pickle.loads(d)
        del data[drop]
        self.assertEqual(sorted(it), sorted(data))

    @yp_unittest.skip( "TODO: Implement pickling in nohtyP" )
    def test_itemiterator_pickling(self):
        data = yp_dict({1:"a", 2:"b", 3:"c"})
        # dictviews aren't picklable, only their iterators
        itorg = iter(data.items())
        d = pickle.dumps(itorg)
        it = pickle.loads(d)
        # note that the type of type of the unpickled iterator
        # is not necessarily the same as the original.  It is
        # merely an object supporting the iterator protocol, yielding
        # the same objects as the original one.
        # self.assertEqual(type(itorg), type(it))
        self.assertTrue(isinstance(it, collections.abc.Iterator))
        self.assertEqual(yp_dict(it), data)

        it = pickle.loads(d)
        drop = next(it)
        d = pickle.dumps(it)
        it = pickle.loads(d)
        del data[drop[0]]
        self.assertEqual(yp_dict(it), data)

    @yp_unittest.skip( "TODO: Implement pickling in nohtyP" )
    def test_valuesiterator_pickling(self):
        data = yp_dict({1:"a", 2:"b", 3:"c"})
        # data.values() isn't picklable, only its iterator
        it = iter(data.values())
        d = pickle.dumps(it)
        it = pickle.loads(d)
        self.assertEqual(sorted(list(it)), sorted(list(data.values())))

        it = pickle.loads(d)
        drop = next(it)
        d = pickle.dumps(it)
        it = pickle.loads(d)
        values = list(it) + [drop]
        self.assertEqual(sorted(values), sorted(list(data.values())))

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_instance_dict_getattr_str_subclass(self):
        class Foo:
            def __init__(self, msg):
                self.msg = msg
        f = Foo('123')
        class _str(str):
            pass
        self.assertEqual(f.msg, getattr(f, _str('msg')))
        self.assertEqual(f.msg, f.__dict__[_str('msg')])

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_object_set_item_single_instance_non_str_key(self):
        class Foo: pass
        f = Foo()
        f.__dict__[1] = 1
        f.a = 'a'
        self.assertEqual(f.__dict__, yp_dict({1:1, 'a':'a'}))

from yp_test import mapping_tests

class GeneralMappingTests(mapping_tests.BasicTestMappingProtocol):
    type2test = yp_dict

class Dict(yp_dict):
    pass

@yp_unittest.skip("Not applicable to nohtyP")
class SubclassMappingTests(mapping_tests.BasicTestMappingProtocol):
    type2test = Dict

def test_main():
    support.run_unittest(
        DictTest,
        GeneralMappingTests,
        SubclassMappingTests,
    )

if __name__ == "__main__":
    test_main()
