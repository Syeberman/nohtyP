from yp import *
import unittest
from yp_test import support
import gc
import weakref
import operator
import copy
import pickle
from random import randrange, shuffle
import sys
import warnings
import collections
import collections.abc

# Extra assurance that we're not accidentally testing Python's frozenset and set
def frozenset( *args, **kwargs ): raise NotImplementedError( "convert script to yp_frozenset here" )
def set( *args, **kwargs ): raise NotImplementedError( "convert script to yp_set here" )

# Choose an error unlikely to be confused with anything else in Python or nohtyP
PassThru = OverflowError

def check_pass_thru():
    raise PassThru
    yield 1

class BadCmp:
    def __hash__(self):
        return 1
    def __eq__(self, other):
        raise RuntimeError

class ReprWrapper:
    'Used to test self-referential repr() calls'
    def __repr__(self):
        return repr(self.value)

class HashCountingInt(int):
    'int-like object that counts the number of times __hash__ is called'
    def __init__(self, *args):
        self.hash_count = 0
    def __hash__(self):
        self.hash_count += 1
        return int.__hash__(self)

class TestJointOps(unittest.TestCase):
    # Tests common to both set and frozenset

    def setUp(self):
        self.word = word = 'simsalabim'
        self.otherword = 'madagascar'
        self.letters = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'
        self.s = self.thetype(word)
        self.d = dict.fromkeys(word)

    @unittest.skip("Not applicable to nohtyP")
    def test_new_or_init(self):
        self.assertRaises(TypeError, self.thetype, [], 2)
        self.assertRaises(TypeError, yp_set().__init__, a=1)

    def test_uniquification(self):
        actual = sorted(self.s)
        expected = sorted(self.d)
        self.assertEqual(actual, expected)
        self.assertRaises(PassThru, self.thetype, check_pass_thru())
        self.assertRaises(TypeError, self.thetype, [[]])

    def test_len(self):
        self.assertEqual(len(self.s), len(self.d))

    def test_contains(self):
        for c in self.letters:
            self.assertEqual(c in self.s, c in self.d)
        self.assertNotIn(yp_list(), self.s) # nohtyP sets accept mutable types for "in"
        s = self.thetype([yp_frozenset(self.letters)])
        self.assertIn(self.thetype(self.letters), s)

    def test_union(self):
        u = self.s.union(self.otherword)
        for c in self.letters:
            self.assertEqual(c in u, c in self.d or c in self.otherword)
        self.assertEqual(self.s, self.thetype(self.word))
        self.assertEqual(type(u), self.basetype)
        self.assertRaises(PassThru, self.s.union, check_pass_thru())
        self.assertRaises(TypeError, self.s.union, [[]])
        for C in yp_set, yp_frozenset, dict.fromkeys, str, list, tuple:
            self.assertEqual(self.thetype('abcba').union(C('cdc')), yp_set('abcd'))
            self.assertEqual(self.thetype('abcba').union(C('efgfe')), yp_set('abcefg'))
            self.assertEqual(self.thetype('abcba').union(C('ccb')), yp_set('abc'))
            self.assertEqual(self.thetype('abcba').union(C('ef')), yp_set('abcef'))
            self.assertEqual(self.thetype('abcba').union(C('ef'), C('fg')), yp_set('abcefg'))

        # Issue #6573
        x = self.thetype()
        self.assertEqual(x.union(yp_set([1]), x, yp_set([2])), self.thetype([1, 2]))

    def test_or(self):
        i = self.s.union(self.otherword)
        self.assertEqual(self.s | yp_set(self.otherword), i)
        self.assertEqual(self.s | yp_frozenset(self.otherword), i)
        try:
            self.s | self.otherword
        except TypeError:
            pass
        else:
            self.fail("s|t did not screen-out general iterables")

    def test_intersection(self):
        i = self.s.intersection(self.otherword)
        for c in self.letters:
            self.assertEqual(c in i, c in self.d and c in self.otherword)
        self.assertEqual(self.s, self.thetype(self.word))
        self.assertEqual(type(i), self.basetype)
        self.assertRaises(PassThru, self.s.intersection, check_pass_thru())
        for C in yp_set, yp_frozenset, dict.fromkeys, str, list, tuple:
            self.assertEqual(self.thetype('abcba').intersection(C('cdc')), yp_set('cc'))
            self.assertEqual(self.thetype('abcba').intersection(C('efgfe')), yp_set(''))
            self.assertEqual(self.thetype('abcba').intersection(C('ccb')), yp_set('bc'))
            self.assertEqual(self.thetype('abcba').intersection(C('ef')), yp_set(''))
            self.assertEqual(self.thetype('abcba').intersection(C('cbcf'), C('bag')), yp_set('b'))
        s = self.thetype('abcba')
        z = s.intersection()
        if self.thetype == yp_frozenset:
            self.assertEqual(id(s), id(z))
        else:
            self.assertNotEqual(id(s), id(z))

    def test_isdisjoint(self):
        def f(s1, s2):
            'Pure python equivalent of isdisjoint()'
            return not yp_set(s1).intersection(s2)
        for larg in '', 'a', 'ab', 'abc', 'ababac', 'cdc', 'cc', 'efgfe', 'ccb', 'ef':
            s1 = self.thetype(larg)
            for rarg in '', 'a', 'ab', 'abc', 'ababac', 'cdc', 'cc', 'efgfe', 'ccb', 'ef':
                for C in yp_set, yp_frozenset, dict.fromkeys, str, list, tuple:
                    s2 = C(rarg)
                    actual = s1.isdisjoint(s2)
                    expected = f(s1, s2)
                    self.assertEqual(actual, expected)
                    #self.assertTrue(actual is True or actual is False) # N/A for nohtyP

    def test_and(self):
        i = self.s.intersection(self.otherword)
        self.assertEqual(self.s & yp_set(self.otherword), i)
        self.assertEqual(self.s & yp_frozenset(self.otherword), i)
        try:
            self.s & self.otherword
        except TypeError:
            pass
        else:
            self.fail("s&t did not screen-out general iterables")

    def test_difference(self):
        i = self.s.difference(self.otherword)
        for c in self.letters:
            self.assertEqual(c in i, c in self.d and c not in self.otherword)
        self.assertEqual(self.s, self.thetype(self.word))
        self.assertEqual(type(i), self.basetype)
        self.assertRaises(PassThru, self.s.difference, check_pass_thru())
        self.assertEqual(self.s, self.s.difference([[]])) # nohtyP sets accept mutable types here
        for C in yp_set, yp_frozenset, dict.fromkeys, str, list, tuple:
            self.assertEqual(self.thetype('abcba').difference(C('cdc')), yp_set('ab'))
            self.assertEqual(self.thetype('abcba').difference(C('efgfe')), yp_set('abc'))
            self.assertEqual(self.thetype('abcba').difference(C('ccb')), yp_set('a'))
            self.assertEqual(self.thetype('abcba').difference(C('ef')), yp_set('abc'))
            self.assertEqual(self.thetype('abcba').difference(), yp_set('abc'))
            self.assertEqual(self.thetype('abcba').difference(C('a'), C('b')), yp_set('c'))

    def test_sub(self):
        i = self.s.difference(self.otherword)
        self.assertEqual(self.s - yp_set(self.otherword), i)
        self.assertEqual(self.s - yp_frozenset(self.otherword), i)
        try:
            self.s - self.otherword
        except TypeError:
            pass
        else:
            self.fail("s-t did not screen-out general iterables")

    def test_symmetric_difference(self):
        i = self.s.symmetric_difference(self.otherword)
        for c in self.letters:
            self.assertEqual(c in i, (c in self.d) ^ (c in self.otherword))
        self.assertEqual(self.s, self.thetype(self.word))
        self.assertEqual(type(i), self.basetype)
        self.assertRaises(PassThru, self.s.symmetric_difference, check_pass_thru())
        self.assertRaises(TypeError, self.s.symmetric_difference, [[]])
        for C in yp_set, yp_frozenset, dict.fromkeys, str, list, tuple:
            self.assertEqual(self.thetype('abcba').symmetric_difference(C('cdc')), yp_set('abd'))
            self.assertEqual(self.thetype('abcba').symmetric_difference(C('efgfe')), yp_set('abcefg'))
            self.assertEqual(self.thetype('abcba').symmetric_difference(C('ccb')), yp_set('a'))
            self.assertEqual(self.thetype('abcba').symmetric_difference(C('ef')), yp_set('abcef'))

    def test_xor(self):
        i = self.s.symmetric_difference(self.otherword)
        self.assertEqual(self.s ^ yp_set(self.otherword), i)
        self.assertEqual(self.s ^ yp_frozenset(self.otherword), i)
        try:
            self.s ^ self.otherword
        except TypeError:
            pass
        else:
            self.fail("s^t did not screen-out general iterables")

    def test_equality(self):
        self.assertEqual(self.s, yp_set(self.word))
        self.assertEqual(self.s, yp_frozenset(self.word))
        self.assertEqual(self.s == self.word, False)
        self.assertNotEqual(self.s, yp_set(self.otherword))
        self.assertNotEqual(self.s, yp_frozenset(self.otherword))
        self.assertEqual(self.s != self.word, True)

    def test_setOfFrozensets(self):
        t = map(yp_frozenset, ['abcdef', 'bcd', 'bdcb', 'fed', 'fedccba'])
        s = self.thetype(t)
        self.assertEqual(len(s), 3)

    def test_sub_and_super(self):
        p, q, r = map(self.thetype, ['ab', 'abcde', 'def'])
        self.assertTrue(p < q)
        self.assertTrue(p <= q)
        self.assertTrue(q <= q)
        self.assertTrue(q > p)
        self.assertTrue(q >= p)
        self.assertFalse(q < r)
        self.assertFalse(q <= r)
        self.assertFalse(q > r)
        self.assertFalse(q >= r)
        self.assertTrue(yp_set('a').issubset('abc'))
        self.assertTrue(yp_set('abc').issuperset('a'))
        self.assertFalse(yp_set('a').issubset('cbs'))
        self.assertFalse(yp_set('cbs').issuperset('a'))

    @unittest.skip("TODO: Implement nohtyP pickling")
    def test_pickling(self):
        for i in range(pickle.HIGHEST_PROTOCOL + 1):
            p = pickle.dumps(self.s, i)
            dup = pickle.loads(p)
            self.assertEqual(self.s, dup, "%s != %s" % (self.s, dup))
            if type(self.s) not in (yp_set, yp_frozenset):
                self.s.x = 10
                p = pickle.dumps(self.s)
                dup = pickle.loads(p)
                self.assertEqual(self.s.x, dup.x)

    @unittest.skip("TODO: Implement nohtyP pickling")
    def test_iterator_pickling(self):
        itorg = iter(self.s)
        data = self.thetype(self.s)
        d = pickle.dumps(itorg)
        it = pickle.loads(d)
        # Set iterators unpickle as list iterators due to the
        # undefined order of set items.
        # self.assertEqual(type(itorg), type(it))
        self.assertTrue(isinstance(it, collections.abc.Iterator))
        self.assertEqual(self.thetype(it), data)

        it = pickle.loads(d)
        try:
            drop = next(it)
        except StopIteration:
            return
        d = pickle.dumps(it)
        it = pickle.loads(d)
        self.assertEqual(self.thetype(it), data - self.thetype((drop,)))

    @unittest.skip("REWORK: nohtyP sets don't store user-defined types")
    def test_deepcopy(self):
        class Tracer:
            def __init__(self, value):
                self.value = value
            def __hash__(self):
                return self.value
            def __deepcopy__(self, memo=None):
                return Tracer(self.value + 1)
        t = Tracer(10)
        s = self.thetype([t])
        dup = copy.deepcopy(s)
        self.assertNotEqual(id(s), id(dup))
        for elem in dup:
            newt = elem
        self.assertNotEqual(id(t), id(newt))
        self.assertEqual(t.value + 1, newt.value)

    @unittest.skip("REWORK: nohtyP sets don't store user-defined types")
    def test_gc(self):
        # Create a nest of cycles to exercise overall ref count check
        class A:
            pass
        s = yp_set(A() for i in range(1000))
        for elem in s:
            elem.cycle = s
            elem.sub = elem
            elem.set = yp_set([elem])

    @unittest.skip("REWORK: nohtyP sets don't store user-defined types")
    def test_subclass_with_custom_hash(self):
        # Bug #1257731
        class H(self.thetype):
            def __hash__(self):
                return int(id(self) & 0x7fffffff)
        s=H()
        f=yp_set()
        f.add(s)
        self.assertIn(s, f)
        f.remove(s)
        f.add(s)
        f.discard(s)

    @unittest.skip("nohtyP sets don't store user-defined types")
    def test_badcmp(self):
        s = self.thetype([BadCmp()])
        # Detect comparison errors during insertion and lookup
        self.assertRaises(RuntimeError, self.thetype, [BadCmp(), BadCmp()])
        self.assertRaises(RuntimeError, s.__contains__, BadCmp())
        # Detect errors during mutating operations
        if hasattr(s, 'add'):
            self.assertRaises(RuntimeError, s.add, BadCmp())
            self.assertRaises(RuntimeError, s.discard, BadCmp())
            self.assertRaises(RuntimeError, s.remove, BadCmp())

    @unittest.skip("REWORK: nohtyP sets don't store user-defined types")
    def test_cyclical_repr(self):
        w = ReprWrapper()
        s = self.thetype([w])
        w.value = s
        if self.thetype == yp_set:
            self.assertEqual(repr(s), '{set(...)}')
        else:
            name = repr(s).partition('(')[0]    # strip class name
            self.assertEqual(repr(s), '%s({%s(...)})' % (name, name))

    @unittest.skip("REWORK: nohtyP sets don't store user-defined types")
    def test_cyclical_print(self):
        w = ReprWrapper()
        s = self.thetype([w])
        w.value = s
        fo = open(support.TESTFN, "w")
        try:
            fo.write(str(s))
            fo.close()
            fo = open(support.TESTFN, "r")
            self.assertEqual(fo.read(), repr(s))
        finally:
            fo.close()
            support.unlink(support.TESTFN)

    @unittest.skip("REWORK: nohtyP sets don't store user-defined types")
    def test_do_not_rehash_dict_keys(self):
        n = 10
        d = dict.fromkeys(map(HashCountingInt, range(n)))
        self.assertEqual(sum(elem.hash_count for elem in d), n)
        s = self.thetype(d)
        self.assertEqual(sum(elem.hash_count for elem in d), n)
        s.difference(d)
        self.assertEqual(sum(elem.hash_count for elem in d), n)
        if hasattr(s, 'symmetric_difference_update'):
            s.symmetric_difference_update(d)
        self.assertEqual(sum(elem.hash_count for elem in d), n)
        d2 = dict.fromkeys(yp_set(d))
        self.assertEqual(sum(elem.hash_count for elem in d), n)
        d3 = dict.fromkeys(yp_frozenset(d))
        self.assertEqual(sum(elem.hash_count for elem in d), n)
        d3 = dict.fromkeys(yp_frozenset(d), 123)
        self.assertEqual(sum(elem.hash_count for elem in d), n)
        self.assertEqual(d3, dict.fromkeys(d, 123))

    @unittest.skip("REWORK: nohtyP sets don't store user-defined types")
    def test_container_iterator(self):
        # Bug #3680: tp_traverse was not implemented for set iterator object
        class C(object):
            pass
        obj = C()
        ref = weakref.ref(obj)
        container = yp_set([obj, 1])
        obj.x = iter(container)
        del obj, container
        gc.collect()
        self.assertTrue(ref() is None, "Cycle was not collected")

class TestSet(TestJointOps):
    thetype = yp_set
    basetype = yp_set

    @unittest.skip("Not applicable to nohtyP")
    def test_init(self):
        s = self.thetype()
        s.__init__(self.word)
        self.assertEqual(s, yp_set(self.word))
        s.__init__(self.otherword)
        self.assertEqual(s, yp_set(self.otherword))
        self.assertRaises(TypeError, s.__init__, s, 2);
        self.assertRaises(TypeError, s.__init__, 1);

    def test_constructor_identity(self):
        s = self.thetype(range(3))
        t = self.thetype(s)
        self.assertNotEqual(id(s), id(t))

    def test_set_literal(self):
        s = yp_set([1,2,3])
        t = {1,2,3}
        self.assertEqual(s, t)

    def test_hash(self):
        self.assertRaises(TypeError, hash, self.s)

    def test_clear(self):
        self.s.clear()
        self.assertEqual(self.s, yp_set())
        self.assertEqual(len(self.s), 0)

    def test_copy(self):
        dup = self.s.copy()
        self.assertEqual(self.s, dup)
        self.assertNotEqual(id(self.s), id(dup))
        self.assertEqual(type(dup), self.basetype)

    def test_add(self):
        self.s.add('Q')
        self.assertIn('Q', self.s)
        dup = self.s.copy()
        self.s.add('Q')
        self.assertEqual(self.s, dup)
        self.assertRaises(TypeError, self.s.add, [])

    def test_remove(self):
        self.s.remove('a')
        self.assertNotIn('a', self.s)
        self.assertRaises(KeyError, self.s.remove, 'Q')
        self.assertRaises(KeyError, self.s.remove, []) # nohtyP sets accept mutable types here
        s = self.thetype([yp_frozenset(self.word)])
        self.assertIn(self.thetype(self.word), s)
        s.remove(self.thetype(self.word))
        self.assertNotIn(self.thetype(self.word), s)
        self.assertRaises(KeyError, self.s.remove, self.thetype(self.word))

    @unittest.skip("Not applicable to nohtyP")
    def test_remove_keyerror_unpacking(self):
        # bug:  www.python.org/sf/1576657
        for v1 in ['Q', (1,)]:
            try:
                self.s.remove(v1)
            except KeyError as e:
                v2 = e.args[0]
                self.assertEqual(v1, v2)
            else:
                self.fail()

    @unittest.skip("Not applicable to nohtyP")
    def test_remove_keyerror_set(self):
        key = self.thetype([3, 4])
        try:
            self.s.remove(key)
        except KeyError as e:
            self.assertTrue(e.args[0] is key,
                         "KeyError should be {0}, not {1}".format(key,
                                                                  e.args[0]))
        else:
            self.fail()

    def test_discard(self):
        self.s.discard('a')
        self.assertNotIn('a', self.s)
        self.s.discard('Q')
        
        s = self.s.copy()
        s.discard([]) # nohtyP sets accept mutable types here
        self.assertEqual(s, self.s)
        
        s = self.thetype([yp_frozenset(self.word)])
        self.assertIn(self.thetype(self.word), s)
        s.discard(self.thetype(self.word))
        self.assertNotIn(self.thetype(self.word), s)
        s.discard(self.thetype(self.word))

    def test_pop(self):
        for i in range(len(self.s)):
            elem = self.s.pop()
            self.assertNotIn(elem, self.s)
        self.assertRaises(KeyError, self.s.pop)

    def test_update(self):
        retval = self.s.update(self.otherword)
        self.assertEqual(retval, None)
        for c in (self.word + self.otherword):
            self.assertIn(c, self.s)
        self.assertRaises(PassThru, self.s.update, check_pass_thru())
        self.assertRaises(TypeError, self.s.update, [[]])
        for p, q in (('cdc', 'abcd'), ('efgfe', 'abcefg'), ('ccb', 'abc'), ('ef', 'abcef')):
            for C in yp_set, yp_frozenset, dict.fromkeys, str, list, tuple:
                s = self.thetype('abcba')
                self.assertEqual(s.update(C(p)), None)
                self.assertEqual(s, yp_set(q))
        for p in ('cdc', 'efgfe', 'ccb', 'ef', 'abcda'):
            q = 'ahi'
            for C in yp_set, yp_frozenset, dict.fromkeys, str, list, tuple:
                s = self.thetype('abcba')
                self.assertEqual(s.update(C(p), C(q)), None)
                self.assertEqual(s, yp_set(s) | yp_set(p) | yp_set(q))

    def test_ior(self):
        self.s |= yp_set(self.otherword)
        for c in (self.word + self.otherword):
            self.assertIn(c, self.s)

    def test_intersection_update(self):
        retval = self.s.intersection_update(self.otherword)
        self.assertEqual(retval, None)
        for c in (self.word + self.otherword):
            if c in self.otherword and c in self.word:
                self.assertIn(c, self.s)
            else:
                self.assertNotIn(c, self.s)
        self.assertRaises(PassThru, self.s.intersection_update, check_pass_thru())

        s = self.s.copy()
        s.intersection_update([[]]) # nohtyP sets accept mutable types here
        self.assertEqual(s, self.thetype())

        for p, q in (('cdc', 'c'), ('efgfe', ''), ('ccb', 'bc'), ('ef', '')):
            for C in yp_set, yp_frozenset, dict.fromkeys, str, list, tuple:
                s = self.thetype('abcba')
                self.assertEqual(s.intersection_update(C(p)), None)
                self.assertEqual(s, yp_set(q))
                ss = 'abcba'
                s = self.thetype(ss)
                t = 'cbc'
                self.assertEqual(s.intersection_update(C(p), C(t)), None)
                self.assertEqual(s, yp_set('abcba')&yp_set(p)&yp_set(t))

    def test_iand(self):
        self.s &= yp_set(self.otherword)
        for c in (self.word + self.otherword):
            if c in self.otherword and c in self.word:
                self.assertIn(c, self.s)
            else:
                self.assertNotIn(c, self.s)

    def test_difference_update(self):
        retval = self.s.difference_update(self.otherword)
        self.assertEqual(retval, None)
        for c in (self.word + self.otherword):
            if c in self.word and c not in self.otherword:
                self.assertIn(c, self.s)
            else:
                self.assertNotIn(c, self.s)
        self.assertRaises(PassThru, self.s.difference_update, check_pass_thru())
        
        s = self.s.copy()
        s.difference_update([[]]) # nohtyP sets accept mutable types here
        self.assertEqual(s, self.s)
        
        for p, q in (('cdc', 'ab'), ('efgfe', 'abc'), ('ccb', 'a'), ('ef', 'abc')):
            for C in yp_set, yp_frozenset, dict.fromkeys, str, list, tuple:
                s = self.thetype('abcba')
                self.assertEqual(s.difference_update(C(p)), None)
                self.assertEqual(s, yp_set(q))

                s = self.thetype('abcdefghih')
                s.difference_update()
                self.assertEqual(s, self.thetype('abcdefghih'))

                s = self.thetype('abcdefghih')
                s.difference_update(C('aba'))
                self.assertEqual(s, self.thetype('cdefghih'))

                s = self.thetype('abcdefghih')
                s.difference_update(C('cdc'), C('aba'))
                self.assertEqual(s, self.thetype('efghih'))

    def test_isub(self):
        self.s -= yp_set(self.otherword)
        for c in (self.word + self.otherword):
            if c in self.word and c not in self.otherword:
                self.assertIn(c, self.s)
            else:
                self.assertNotIn(c, self.s)

    def test_symmetric_difference_update(self):
        retval = self.s.symmetric_difference_update(self.otherword)
        self.assertEqual(retval, None)
        for c in (self.word + self.otherword):
            if (c in self.word) ^ (c in self.otherword):
                self.assertIn(c, self.s)
            else:
                self.assertNotIn(c, self.s)
        self.assertRaises(PassThru, self.s.symmetric_difference_update, check_pass_thru())
        self.assertRaises(TypeError, self.s.symmetric_difference_update, [[]])
        for p, q in (('cdc', 'abd'), ('efgfe', 'abcefg'), ('ccb', 'a'), ('ef', 'abcef')):
            for C in yp_set, yp_frozenset, dict.fromkeys, str, list, tuple:
                s = self.thetype('abcba')
                self.assertEqual(s.symmetric_difference_update(C(p)), None)
                self.assertEqual(s, yp_set(q))

    def test_ixor(self):
        self.s ^= yp_set(self.otherword)
        for c in (self.word + self.otherword):
            if (c in self.word) ^ (c in self.otherword):
                self.assertIn(c, self.s)
            else:
                self.assertNotIn(c, self.s)

    def test_inplace_on_self(self):
        t = self.s.copy()
        t |= t
        self.assertEqual(t, self.s)
        t &= t
        self.assertEqual(t, self.s)
        t -= t
        self.assertEqual(t, self.thetype())
        t = self.s.copy()
        t ^= t
        self.assertEqual(t, self.thetype())

    def test_weakref(self):
        s = self.thetype('gallahad')
        p = weakref.proxy(s)
        self.assertEqual(str(p), str(s))
        s = None
        self.assertRaises(ReferenceError, str, p)

    @unittest.skip("REWORK: nohtyP doesn't have user-defined types")
    def test_rich_compare(self):
        class TestRichSetCompare:
            def __gt__(self, some_set):
                self.gt_called = True
                return False
            def __lt__(self, some_set):
                self.lt_called = True
                return False
            def __ge__(self, some_set):
                self.ge_called = True
                return False
            def __le__(self, some_set):
                self.le_called = True
                return False

        # This first tries the builtin rich set comparison, which doesn't know
        # how to handle the custom object. Upon returning NotImplemented, the
        # corresponding comparison on the right object is invoked.
        myset = yp_set({1, 2, 3})

        myobj = TestRichSetCompare()
        myset < myobj
        self.assertTrue(myobj.gt_called)

        myobj = TestRichSetCompare()
        myset > myobj
        self.assertTrue(myobj.lt_called)

        myobj = TestRichSetCompare()
        myset <= myobj
        self.assertTrue(myobj.ge_called)

        myobj = TestRichSetCompare()
        myset >= myobj
        self.assertTrue(myobj.le_called)

    # C API test only available in a debug build
    if hasattr(yp_set, "test_c_api"):
        def test_c_api(self):
            self.assertEqual(yp_set().test_c_api(), True)

class SetSubclass(yp_set):
    pass

class TestSetSubclass(TestSet):
    thetype = SetSubclass
    basetype = yp_set

class SetSubclassWithKeywordArgs(yp_set):
    def __init__(self, iterable=[], newarg=None):
        yp_set.__init__(self, iterable)

class TestSetSubclassWithKeywordArgs(TestSet):

    @unittest.skip("Not applicable to nohtyP")
    def test_keywords_in_subclass(self):
        'SF bug #1486663 -- this used to erroneously raise a TypeError'
        SetSubclassWithKeywordArgs(newarg=1)

class TestFrozenSet(TestJointOps):
    thetype = yp_frozenset
    basetype = yp_frozenset

    @unittest.skip("Not applicable to nohtyP")
    def test_init(self):
        s = self.thetype(self.word)
        s.__init__(self.otherword)
        self.assertEqual(s, yp_set(self.word))

    def test_singleton_empty_frozenset(self):
        f = yp_frozenset()
        efs = [yp_frozenset(), yp_frozenset([]), yp_frozenset(()), yp_frozenset(''),
               yp_frozenset(), yp_frozenset([]), yp_frozenset(()), yp_frozenset(''),
               yp_frozenset(range(0)), yp_frozenset(yp_frozenset()),
               yp_frozenset(f), f]
        # All of the empty yp_frozensets should have just one id()
        self.assertEqual(len(yp_set(map(id, efs))), 1)

    def test_constructor_identity(self):
        s = self.thetype(range(3))
        t = self.thetype(s)
        self.assertEqual(id(s), id(t))

    def test_hash(self):
        self.assertEqual(hash(self.thetype('abcdeb')),
                         hash(self.thetype('ebecda')))

        # make sure that all permutations give the same hash value
        n = 100
        seq = [randrange(n) for i in range(n)]
        results = yp_set()
        for i in range(200):
            shuffle(seq)
            results.add(hash(self.thetype(seq)))
        self.assertEqual(len(results), 1)

    def test_copy(self):
        dup = self.s.copy()
        self.assertEqual(id(self.s), id(dup))

    def test_frozen_as_dictkey(self):
        seq = list(range(10)) + list('abcdefg') + ['apple']
        key1 = self.thetype(seq)
        key2 = self.thetype(reversed(seq))
        self.assertEqual(key1, key2)
        self.assertNotEqual(id(key1), id(key2))
        d = {}
        d[key1] = 42
        self.assertEqual(d[key2], 42)

    def test_hash_caching(self):
        f = self.thetype('abcdcda')
        self.assertEqual(hash(f), hash(f))

    def test_hash_effectiveness(self):
        n = 13
        hashvalues = yp_set()
        addhashvalue = hashvalues.add
        elemmasks = [(i+1, 1<<i) for i in range(n)]
        for i in range(2**n):
            addhashvalue(hash(yp_frozenset([e for e, m in elemmasks if m&i])))
        self.assertEqual(len(hashvalues), 2**n)

class FrozenSetSubclass(yp_frozenset):
    pass

class TestFrozenSetSubclass(TestFrozenSet):
    thetype = FrozenSetSubclass
    basetype = yp_frozenset

    def test_constructor_identity(self):
        s = self.thetype(range(3))
        t = self.thetype(s)
        self.assertNotEqual(id(s), id(t))

    def test_copy(self):
        dup = self.s.copy()
        self.assertNotEqual(id(self.s), id(dup))

    def test_nested_empty_constructor(self):
        s = self.thetype()
        t = self.thetype(s)
        self.assertEqual(s, t)

    def test_singleton_empty_frozenset(self):
        Frozenset = self.thetype
        f = yp_frozenset()
        F = Frozenset()
        efs = [Frozenset(), Frozenset([]), Frozenset(()), Frozenset(''),
               Frozenset(), Frozenset([]), Frozenset(()), Frozenset(''),
               Frozenset(range(0)), Frozenset(Frozenset()),
               Frozenset(yp_frozenset()), f, F, Frozenset(f), Frozenset(F)]
        # All empty yp_frozenset subclass instances should have different ids
        self.assertEqual(len(yp_set(map(id, efs))), len(efs))

# Tests taken from test_sets.py =============================================

empty_set = yp_set()

#==============================================================================

class TestBasicOps(unittest.TestCase):

    def test_repr(self):
        if self.repr is not None:
            self.assertEqual(repr(self.set), self.repr)

    def check_repr_against_values(self):
        text = repr(self.set)
        self.assertTrue(text.startswith('{'))
        self.assertTrue(text.endswith('}'))

        result = text[1:-1].split(', ')
        result.sort()
        sorted_repr_values = [repr(value) for value in self.values]
        sorted_repr_values.sort()
        self.assertEqual(result, sorted_repr_values)

    def test_print(self):
        try:
            fo = open(support.TESTFN, "w")
            fo.write(str(self.set))
            fo.close()
            fo = open(support.TESTFN, "r")
            self.assertEqual(fo.read(), repr(self.set))
        finally:
            fo.close()
            support.unlink(support.TESTFN)

    def test_length(self):
        self.assertEqual(len(self.set), self.length)

    def test_self_equality(self):
        self.assertEqual(self.set, self.set)

    def test_equivalent_equality(self):
        self.assertEqual(self.set, self.dup)

    def test_copy(self):
        self.assertEqual(self.set.copy(), self.dup)

    def test_self_union(self):
        result = self.set | self.set
        self.assertEqual(result, self.dup)

    def test_empty_union(self):
        result = self.set | empty_set
        self.assertEqual(result, self.dup)

    def test_union_empty(self):
        result = empty_set | self.set
        self.assertEqual(result, self.dup)

    def test_self_intersection(self):
        result = self.set & self.set
        self.assertEqual(result, self.dup)

    def test_empty_intersection(self):
        result = self.set & empty_set
        self.assertEqual(result, empty_set)

    def test_intersection_empty(self):
        result = empty_set & self.set
        self.assertEqual(result, empty_set)

    def test_self_isdisjoint(self):
        result = self.set.isdisjoint(self.set)
        self.assertEqual(result, not self.set)

    def test_empty_isdisjoint(self):
        result = self.set.isdisjoint(empty_set)
        self.assertEqual(result, True)

    def test_isdisjoint_empty(self):
        result = empty_set.isdisjoint(self.set)
        self.assertEqual(result, True)

    def test_self_symmetric_difference(self):
        result = self.set ^ self.set
        self.assertEqual(result, empty_set)

    def test_empty_symmetric_difference(self):
        result = self.set ^ empty_set
        self.assertEqual(result, self.set)

    def test_self_difference(self):
        result = self.set - self.set
        self.assertEqual(result, empty_set)

    def test_empty_difference(self):
        result = self.set - empty_set
        self.assertEqual(result, self.dup)

    def test_empty_difference_rev(self):
        result = empty_set - self.set
        self.assertEqual(result, empty_set)

    def test_iteration(self):
        for v in self.set:
            self.assertIn(v, self.values)
        setiter = iter(self.set)
        # note: __length_hint__ is an internal undocumented API,
        # don't rely on it in your own programs
        self.assertEqual(setiter.__length_hint__(), len(self.set))

    @unittest.skip("TODO: Implement nohtyP pickling")
    def test_pickling(self):
        p = pickle.dumps(self.set)
        copy = pickle.loads(p)
        self.assertEqual(self.set, copy,
                         "%s != %s" % (self.set, copy))

#------------------------------------------------------------------------------

class TestBasicOpsEmpty(TestBasicOps):
    def setUp(self):
        self.case   = "empty set"
        self.values = []
        self.set    = yp_set(self.values)
        self.dup    = yp_set(self.values)
        self.length = 0
        self.repr   = "set()"

#------------------------------------------------------------------------------

class TestBasicOpsSingleton(TestBasicOps):
    def setUp(self):
        self.case   = "unit set (number)"
        self.values = [3]
        self.set    = yp_set(self.values)
        self.dup    = yp_set(self.values)
        self.length = 1
        self.repr   = "{3}"

    def test_in(self):
        self.assertIn(3, self.set)

    def test_not_in(self):
        self.assertNotIn(2, self.set)

#------------------------------------------------------------------------------

class TestBasicOpsTuple(TestBasicOps):
    def setUp(self):
        self.case   = "unit set (tuple)"
        self.values = [(0, "zero")]
        self.set    = yp_set(self.values)
        self.dup    = yp_set(self.values)
        self.length = 1
        self.repr   = "{(0, 'zero')}"

    def test_in(self):
        self.assertIn((0, "zero"), self.set)

    def test_not_in(self):
        self.assertNotIn(9, self.set)

#------------------------------------------------------------------------------

class TestBasicOpsTriple(TestBasicOps):
    def setUp(self):
        self.case   = "triple set"
        self.values = [0, "zero", operator.add]
        self.set    = yp_set(self.values)
        self.dup    = yp_set(self.values)
        self.length = 3
        self.repr   = None

#------------------------------------------------------------------------------

class TestBasicOpsString(TestBasicOps):
    def setUp(self):
        self.case   = "string set"
        self.values = ["a", "b", "c"]
        self.set    = yp_set(self.values)
        self.dup    = yp_set(self.values)
        self.length = 3

    def test_repr(self):
        self.check_repr_against_values()

#------------------------------------------------------------------------------

class TestBasicOpsBytes(TestBasicOps):
    def setUp(self):
        self.case   = "string set"
        self.values = [b"a", b"b", b"c"]
        self.set    = yp_set(self.values)
        self.dup    = yp_set(self.values)
        self.length = 3

    def test_repr(self):
        self.check_repr_against_values()

#------------------------------------------------------------------------------

class TestBasicOpsMixedStringBytes(TestBasicOps):
    def setUp(self):
        self._warning_filters = support.check_warnings()
        self._warning_filters.__enter__()
        warnings.simplefilter('ignore', BytesWarning)
        self.case   = "string and bytes set"
        self.values = ["a", "b", b"a", b"b"]
        self.set    = yp_set(self.values)
        self.dup    = yp_set(self.values)
        self.length = 4

    def tearDown(self):
        self._warning_filters.__exit__(None, None, None)

    def test_repr(self):
        self.check_repr_against_values()

#==============================================================================

def baditer():
    raise TypeError
    yield True

def gooditer():
    yield True

class TestExceptionPropagation(unittest.TestCase):
    """SF 628246:  Set constructor should not trap iterator TypeErrors"""

    def test_instanceWithException(self):
        self.assertRaises(TypeError, yp_set, baditer())

    def test_instancesWithoutException(self):
        # All of these iterables should load without exception.
        yp_set([1,2,3])
        yp_set((1,2,3))
        yp_set({'one':1, 'two':2, 'three':3})
        yp_set(range(3))
        yp_set('abc')
        yp_set(gooditer())

    @unittest.skip("Not applicable to nohtyP")
    def test_changingSizeWhileIterating(self):
        s = yp_set([1,2,3])
        try:
            for i in s:
                s.update([4])
        except RuntimeError:
            pass
        else:
            self.fail("no exception when changing size during iteration")

#==============================================================================

class TestSetOfSets(unittest.TestCase):
    def test_constructor(self):
        inner = yp_frozenset([1])
        outer = yp_set([inner])
        element = outer.pop()
        self.assertEqual(type(element), yp_frozenset)
        outer.add(inner)        # Rebuild set of sets with .add method
        outer.remove(inner)
        self.assertEqual(outer, yp_set())   # Verify that remove worked
        outer.discard(inner)    # Absence of KeyError indicates working fine

#==============================================================================

class TestBinaryOps(unittest.TestCase):
    def setUp(self):
        self.set = yp_set((2, 4, 6))

    def test_eq(self):              # SF bug 643115
        self.assertEqual(self.set, yp_set({2:1,4:3,6:5}))

    def test_union_subset(self):
        result = self.set | yp_set([2])
        self.assertEqual(result, yp_set((2, 4, 6)))

    def test_union_superset(self):
        result = self.set | yp_set([2, 4, 6, 8])
        self.assertEqual(result, yp_set([2, 4, 6, 8]))

    def test_union_overlap(self):
        result = self.set | yp_set([3, 4, 5])
        self.assertEqual(result, yp_set([2, 3, 4, 5, 6]))

    def test_union_non_overlap(self):
        result = self.set | yp_set([8])
        self.assertEqual(result, yp_set([2, 4, 6, 8]))

    def test_intersection_subset(self):
        result = self.set & yp_set((2, 4))
        self.assertEqual(result, yp_set((2, 4)))

    def test_intersection_superset(self):
        result = self.set & yp_set([2, 4, 6, 8])
        self.assertEqual(result, yp_set([2, 4, 6]))

    def test_intersection_overlap(self):
        result = self.set & yp_set([3, 4, 5])
        self.assertEqual(result, yp_set([4]))

    def test_intersection_non_overlap(self):
        result = self.set & yp_set([8])
        self.assertEqual(result, empty_set)

    def test_isdisjoint_subset(self):
        result = self.set.isdisjoint(yp_set((2, 4)))
        self.assertEqual(result, False)

    def test_isdisjoint_superset(self):
        result = self.set.isdisjoint(yp_set([2, 4, 6, 8]))
        self.assertEqual(result, False)

    def test_isdisjoint_overlap(self):
        result = self.set.isdisjoint(yp_set([3, 4, 5]))
        self.assertEqual(result, False)

    def test_isdisjoint_non_overlap(self):
        result = self.set.isdisjoint(yp_set([8]))
        self.assertEqual(result, True)

    def test_sym_difference_subset(self):
        result = self.set ^ yp_set((2, 4))
        self.assertEqual(result, yp_set([6]))

    def test_sym_difference_superset(self):
        result = self.set ^ yp_set((2, 4, 6, 8))
        self.assertEqual(result, yp_set([8]))

    def test_sym_difference_overlap(self):
        result = self.set ^ yp_set((3, 4, 5))
        self.assertEqual(result, yp_set([2, 3, 5, 6]))

    def test_sym_difference_non_overlap(self):
        result = self.set ^ yp_set([8])
        self.assertEqual(result, yp_set([2, 4, 6, 8]))

#==============================================================================

class TestUpdateOps(unittest.TestCase):
    def setUp(self):
        self.set = yp_set((2, 4, 6))

    def test_union_subset(self):
        self.set |= yp_set([2])
        self.assertEqual(self.set, yp_set((2, 4, 6)))

    def test_union_superset(self):
        self.set |= yp_set([2, 4, 6, 8])
        self.assertEqual(self.set, yp_set([2, 4, 6, 8]))

    def test_union_overlap(self):
        self.set |= yp_set([3, 4, 5])
        self.assertEqual(self.set, yp_set([2, 3, 4, 5, 6]))

    def test_union_non_overlap(self):
        self.set |= yp_set([8])
        self.assertEqual(self.set, yp_set([2, 4, 6, 8]))

    def test_union_method_call(self):
        self.set.update(yp_set([3, 4, 5]))
        self.assertEqual(self.set, yp_set([2, 3, 4, 5, 6]))

    def test_intersection_subset(self):
        self.set &= yp_set((2, 4))
        self.assertEqual(self.set, yp_set((2, 4)))

    def test_intersection_superset(self):
        self.set &= yp_set([2, 4, 6, 8])
        self.assertEqual(self.set, yp_set([2, 4, 6]))

    def test_intersection_overlap(self):
        self.set &= yp_set([3, 4, 5])
        self.assertEqual(self.set, yp_set([4]))

    def test_intersection_non_overlap(self):
        self.set &= yp_set([8])
        self.assertEqual(self.set, empty_set)

    def test_intersection_method_call(self):
        self.set.intersection_update(yp_set([3, 4, 5]))
        self.assertEqual(self.set, yp_set([4]))

    def test_sym_difference_subset(self):
        self.set ^= yp_set((2, 4))
        self.assertEqual(self.set, yp_set([6]))

    def test_sym_difference_superset(self):
        self.set ^= yp_set((2, 4, 6, 8))
        self.assertEqual(self.set, yp_set([8]))

    def test_sym_difference_overlap(self):
        self.set ^= yp_set((3, 4, 5))
        self.assertEqual(self.set, yp_set([2, 3, 5, 6]))

    def test_sym_difference_non_overlap(self):
        self.set ^= yp_set([8])
        self.assertEqual(self.set, yp_set([2, 4, 6, 8]))

    def test_sym_difference_method_call(self):
        self.set.symmetric_difference_update(yp_set([3, 4, 5]))
        self.assertEqual(self.set, yp_set([2, 3, 5, 6]))

    def test_difference_subset(self):
        self.set -= yp_set((2, 4))
        self.assertEqual(self.set, yp_set([6]))

    def test_difference_superset(self):
        self.set -= yp_set((2, 4, 6, 8))
        self.assertEqual(self.set, yp_set([]))

    def test_difference_overlap(self):
        self.set -= yp_set((3, 4, 5))
        self.assertEqual(self.set, yp_set([2, 6]))

    def test_difference_non_overlap(self):
        self.set -= yp_set([8])
        self.assertEqual(self.set, yp_set([2, 4, 6]))

    def test_difference_method_call(self):
        self.set.difference_update(yp_set([3, 4, 5]))
        self.assertEqual(self.set, yp_set([2, 6]))

#==============================================================================

class TestMutate(unittest.TestCase):
    def setUp(self):
        self.values = ["a", "b", "c"]
        self.set = yp_set(self.values)

    def test_add_present(self):
        self.set.add("c")
        self.assertEqual(self.set, yp_set("abc"))

    def test_add_absent(self):
        self.set.add("d")
        self.assertEqual(self.set, yp_set("abcd"))

    def test_add_until_full(self):
        tmp = yp_set()
        expected_len = 0
        for v in self.values:
            tmp.add(v)
            expected_len += 1
            self.assertEqual(len(tmp), expected_len)
        self.assertEqual(tmp, self.set)

    def test_remove_present(self):
        self.set.remove("b")
        self.assertEqual(self.set, yp_set("ac"))

    def test_remove_absent(self):
        try:
            self.set.remove("d")
            self.fail("Removing missing element should have raised LookupError")
        except LookupError:
            pass

    def test_remove_until_empty(self):
        expected_len = len(self.set)
        for v in self.values:
            self.set.remove(v)
            expected_len -= 1
            self.assertEqual(len(self.set), expected_len)

    def test_discard_present(self):
        self.set.discard("c")
        self.assertEqual(self.set, yp_set("ab"))

    def test_discard_absent(self):
        self.set.discard("d")
        self.assertEqual(self.set, yp_set("abc"))

    def test_clear(self):
        self.set.clear()
        self.assertEqual(len(self.set), 0)

    def test_pop(self):
        popped = {}
        while self.set:
            popped[self.set.pop()] = None
        self.assertEqual(len(popped), len(self.values))
        for v in self.values:
            self.assertIn(v, popped)

    def test_update_empty_tuple(self):
        self.set.update(())
        self.assertEqual(self.set, yp_set(self.values))

    def test_update_unit_tuple_overlap(self):
        self.set.update(("a",))
        self.assertEqual(self.set, yp_set(self.values))

    def test_update_unit_tuple_non_overlap(self):
        self.set.update(("a", "z"))
        self.assertEqual(self.set, yp_set(self.values + ["z"]))

#==============================================================================

class TestSubsets(unittest.TestCase):

    case2method = {"<=": "issubset",
                   ">=": "issuperset",
                  }

    reverse = {"==": "==",
               "!=": "!=",
               "<":  ">",
               ">":  "<",
               "<=": ">=",
               ">=": "<=",
              }

    def test_issubset(self):
        x = self.left
        y = self.right
        for case in "!=", "==", "<", "<=", ">", ">=":
            expected = case in self.cases
            # Test the binary infix spelling.
            result = eval("x" + case + "y", locals())
            self.assertEqual(result, expected)
            # Test the "friendly" method-name spelling, if one exists.
            if case in TestSubsets.case2method:
                method = getattr(x, TestSubsets.case2method[case])
                result = method(y)
                self.assertEqual(result, expected)

            # Now do the same for the operands reversed.
            rcase = TestSubsets.reverse[case]
            result = eval("y" + rcase + "x", locals())
            self.assertEqual(result, expected)
            if rcase in TestSubsets.case2method:
                method = getattr(y, TestSubsets.case2method[rcase])
                result = method(x)
                self.assertEqual(result, expected)
#------------------------------------------------------------------------------

class TestSubsetEqualEmpty(TestSubsets):
    left  = yp_set()
    right = yp_set()
    name  = "both empty"
    cases = "==", "<=", ">="

#------------------------------------------------------------------------------

class TestSubsetEqualNonEmpty(TestSubsets):
    left  = yp_set([1, 2])
    right = yp_set([1, 2])
    name  = "equal pair"
    cases = "==", "<=", ">="

#------------------------------------------------------------------------------

class TestSubsetEmptyNonEmpty(TestSubsets):
    left  = yp_set()
    right = yp_set([1, 2])
    name  = "one empty, one non-empty"
    cases = "!=", "<", "<="

#------------------------------------------------------------------------------

class TestSubsetPartial(TestSubsets):
    left  = yp_set([1])
    right = yp_set([1, 2])
    name  = "one a non-empty proper subset of other"
    cases = "!=", "<", "<="

#------------------------------------------------------------------------------

class TestSubsetNonOverlap(TestSubsets):
    left  = yp_set([1])
    right = yp_set([2])
    name  = "neither empty, neither contains"
    cases = "!="

#==============================================================================

class TestOnlySetsInBinaryOps(unittest.TestCase):

    def test_eq_ne(self):
        # Unlike the others, this is testing that == and != *are* allowed.
        self.assertEqual(self.other == self.set, False)
        self.assertEqual(self.set == self.other, False)
        self.assertEqual(self.other != self.set, True)
        self.assertEqual(self.set != self.other, True)

    def test_ge_gt_le_lt(self):
        self.assertRaises(TypeError, lambda: self.set < self.other)
        self.assertRaises(TypeError, lambda: self.set <= self.other)
        self.assertRaises(TypeError, lambda: self.set > self.other)
        self.assertRaises(TypeError, lambda: self.set >= self.other)

        self.assertRaises(TypeError, lambda: self.other < self.set)
        self.assertRaises(TypeError, lambda: self.other <= self.set)
        self.assertRaises(TypeError, lambda: self.other > self.set)
        self.assertRaises(TypeError, lambda: self.other >= self.set)

    def test_update_operator(self):
        try:
            self.set |= self.other
        except TypeError:
            pass
        else:
            self.fail("expected TypeError")

    def test_update(self):
        if self.otherIsIterable:
            self.set.update(self.other)
        else:
            self.assertRaises(TypeError, self.set.update, self.other)

    def test_union(self):
        self.assertRaises(TypeError, lambda: self.set | self.other)
        self.assertRaises(TypeError, lambda: self.other | self.set)
        if self.otherIsIterable:
            self.set.union(self.other)
        else:
            self.assertRaises(TypeError, self.set.union, self.other)

    def test_intersection_update_operator(self):
        try:
            self.set &= self.other
        except TypeError:
            pass
        else:
            self.fail("expected TypeError")

    def test_intersection_update(self):
        if self.otherIsIterable:
            self.set.intersection_update(self.other)
        else:
            self.assertRaises(TypeError,
                              self.set.intersection_update,
                              self.other)

    def test_intersection(self):
        self.assertRaises(TypeError, lambda: self.set & self.other)
        self.assertRaises(TypeError, lambda: self.other & self.set)
        if self.otherIsIterable:
            self.set.intersection(self.other)
        else:
            self.assertRaises(TypeError, self.set.intersection, self.other)

    def test_sym_difference_update_operator(self):
        try:
            self.set ^= self.other
        except TypeError:
            pass
        else:
            self.fail("expected TypeError")

    def test_sym_difference_update(self):
        if self.otherIsIterable:
            self.set.symmetric_difference_update(self.other)
        else:
            self.assertRaises(TypeError,
                              self.set.symmetric_difference_update,
                              self.other)

    def test_sym_difference(self):
        self.assertRaises(TypeError, lambda: self.set ^ self.other)
        self.assertRaises(TypeError, lambda: self.other ^ self.set)
        if self.otherIsIterable:
            self.set.symmetric_difference(self.other)
        else:
            self.assertRaises(TypeError, self.set.symmetric_difference, self.other)

    def test_difference_update_operator(self):
        try:
            self.set -= self.other
        except TypeError:
            pass
        else:
            self.fail("expected TypeError")

    def test_difference_update(self):
        if self.otherIsIterable:
            self.set.difference_update(self.other)
        else:
            self.assertRaises(TypeError,
                              self.set.difference_update,
                              self.other)

    def test_difference(self):
        self.assertRaises(TypeError, lambda: self.set - self.other)
        self.assertRaises(TypeError, lambda: self.other - self.set)
        if self.otherIsIterable:
            self.set.difference(self.other)
        else:
            self.assertRaises(TypeError, self.set.difference, self.other)

#------------------------------------------------------------------------------

class TestOnlySetsNumeric(TestOnlySetsInBinaryOps):
    def setUp(self):
        self.set   = yp_set((1, 2, 3))
        self.other = 19
        self.otherIsIterable = False

#------------------------------------------------------------------------------

class TestOnlySetsDict(TestOnlySetsInBinaryOps):
    def setUp(self):
        self.set   = yp_set((1, 2, 3))
        self.other = {1:2, 3:4}
        self.otherIsIterable = True

#------------------------------------------------------------------------------

class TestOnlySetsOperator(TestOnlySetsInBinaryOps):
    def setUp(self):
        self.set   = yp_set((1, 2, 3))
        self.other = operator.add
        self.otherIsIterable = False

#------------------------------------------------------------------------------

class TestOnlySetsTuple(TestOnlySetsInBinaryOps):
    def setUp(self):
        self.set   = yp_set((1, 2, 3))
        self.other = (2, 4, 6)
        self.otherIsIterable = True

#------------------------------------------------------------------------------

class TestOnlySetsString(TestOnlySetsInBinaryOps):
    def setUp(self):
        self.set   = yp_set((1, 2, 3))
        self.other = 'abc'
        self.otherIsIterable = True

#------------------------------------------------------------------------------

class TestOnlySetsGenerator(TestOnlySetsInBinaryOps):
    def setUp(self):
        def gen():
            for i in range(0, 10, 2):
                yield i
        self.set   = yp_set((1, 2, 3))
        self.other = gen()
        self.otherIsIterable = True

#==============================================================================

class TestCopying(unittest.TestCase):

    def test_copy(self):
        dup = self.set.copy()
        dup_list = sorted(dup, key=repr)
        set_list = sorted(self.set, key=repr)
        self.assertEqual(len(dup_list), len(set_list))
        for i in range(len(dup_list)):
            self.assertTrue(dup_list[i] is set_list[i])

    def test_deep_copy(self):
        dup = copy.deepcopy(self.set)
        ##print type(dup), repr(dup)
        dup_list = sorted(dup, key=repr)
        set_list = sorted(self.set, key=repr)
        self.assertEqual(len(dup_list), len(set_list))
        for i in range(len(dup_list)):
            self.assertEqual(dup_list[i], set_list[i])

#------------------------------------------------------------------------------

class TestCopyingEmpty(TestCopying):
    def setUp(self):
        self.set = yp_set()

#------------------------------------------------------------------------------

class TestCopyingSingleton(TestCopying):
    def setUp(self):
        self.set = yp_set(["hello"])

#------------------------------------------------------------------------------

class TestCopyingTriple(TestCopying):
    def setUp(self):
        self.set = yp_set(["zero", 0, None])

#------------------------------------------------------------------------------

class TestCopyingTuple(TestCopying):
    def setUp(self):
        self.set = yp_set([(1, 2)])

#------------------------------------------------------------------------------

class TestCopyingNested(TestCopying):
    def setUp(self):
        self.set = yp_set([((1, 2), (3, 4))])

#==============================================================================

class TestIdentities(unittest.TestCase):
    def setUp(self):
        self.a = yp_set('abracadabra')
        self.b = yp_set('alacazam')

    def test_binopsVsSubsets(self):
        a, b = self.a, self.b
        self.assertTrue(a - b < a)
        self.assertTrue(b - a < b)
        self.assertTrue(a & b < a)
        self.assertTrue(a & b < b)
        self.assertTrue(a | b > a)
        self.assertTrue(a | b > b)
        self.assertTrue(a ^ b < a | b)

    def test_commutativity(self):
        a, b = self.a, self.b
        self.assertEqual(a&b, b&a)
        self.assertEqual(a|b, b|a)
        self.assertEqual(a^b, b^a)
        if a != b:
            self.assertNotEqual(a-b, b-a)

    def test_summations(self):
        # check that sums of parts equal the whole
        a, b = self.a, self.b
        self.assertEqual((a-b)|(a&b)|(b-a), a|b)
        self.assertEqual((a&b)|(a^b), a|b)
        self.assertEqual(a|(b-a), a|b)
        self.assertEqual((a-b)|b, a|b)
        self.assertEqual((a-b)|(a&b), a)
        self.assertEqual((b-a)|(a&b), b)
        self.assertEqual((a-b)|(b-a), a^b)

    def test_exclusion(self):
        # check that inverse operations show non-overlap
        a, b, zero = self.a, self.b, yp_set()
        self.assertEqual((a-b)&b, zero)
        self.assertEqual((b-a)&a, zero)
        self.assertEqual((a&b)&(a^b), zero)

# Tests derived from test_itertools.py =======================================

def R(seqn):
    'Regular generator'
    for i in seqn:
        yield i

class G:
    'Sequence using __getitem__'
    def __init__(self, seqn):
        self.seqn = seqn
    def __getitem__(self, i):
        return self.seqn[i]

class I:
    'Sequence using iterator protocol'
    def __init__(self, seqn):
        self.seqn = seqn
        self.i = 0
    def __iter__(self):
        return self
    def __next__(self):
        if self.i >= len(self.seqn): raise StopIteration
        v = self.seqn[self.i]
        self.i += 1
        return v

class Ig:
    'Sequence using iterator protocol defined with a generator'
    def __init__(self, seqn):
        self.seqn = seqn
        self.i = 0
    def __iter__(self):
        for val in self.seqn:
            yield val

class X:
    'Missing __getitem__ and __iter__'
    def __init__(self, seqn):
        self.seqn = seqn
        self.i = 0
    def __next__(self):
        if self.i >= len(self.seqn): raise StopIteration
        v = self.seqn[self.i]
        self.i += 1
        return v

class N:
    'Iterator missing __next__()'
    def __init__(self, seqn):
        self.seqn = seqn
        self.i = 0
    def __iter__(self):
        return self

class E:
    'Test propagation of exceptions'
    def __init__(self, seqn):
        self.seqn = seqn
        self.i = 0
    def __iter__(self):
        return self
    def __next__(self):
        3 // 0

class S:
    'Test immediate stop'
    def __init__(self, seqn):
        pass
    def __iter__(self):
        return self
    def __next__(self):
        raise StopIteration

from itertools import chain
def L(seqn):
    'Test multiple tiers of iterators'
    return chain(map(lambda x:x, R(Ig(G(seqn)))))

class TestVariousIteratorArgs(unittest.TestCase):

    def test_constructor(self):
        for cons in (yp_set, yp_frozenset):
            for s in ("123", "", range(1000), ('do', 1.2), range(2000,2200,5)):
                for g in (G, I, Ig, S, L, R):
                    self.assertEqual(sorted(cons(g(s)), key=repr), sorted(g(s), key=repr))
                self.assertRaises(TypeError, cons , X(s))
                self.assertRaises(TypeError, cons , N(s))
                self.assertRaises(ZeroDivisionError, cons , E(s))

    def test_inline_methods(self):
        s = yp_set('november')
        for data in ("123", "", range(1000), ('do', 1.2), range(2000,2200,5), 'december'):
            for meth in (s.union, s.intersection, s.difference, s.symmetric_difference, s.isdisjoint):
                for g in (G, I, Ig, L, R):
                    expected = meth(data)
                    actual = meth(G(data))
                    if isinstance(expected, bool):
                        self.assertEqual(actual, expected)
                    else:
                        self.assertEqual(sorted(actual, key=repr), sorted(expected, key=repr))
                self.assertRaises(TypeError, meth, X(s))
                self.assertRaises(TypeError, meth, N(s))
                self.assertRaises(ZeroDivisionError, meth, E(s))

    def test_inplace_methods(self):
        for data in ("123", "", range(1000), ('do', 1.2), range(2000,2200,5), 'december'):
            for methname in ('update', 'intersection_update',
                             'difference_update', 'symmetric_difference_update'):
                for g in (G, I, Ig, S, L, R):
                    s = yp_set('january')
                    t = s.copy()
                    getattr(s, methname)(list(g(data)))
                    getattr(t, methname)(g(data))
                    self.assertEqual(sorted(s, key=repr), sorted(t, key=repr))

                self.assertRaises(TypeError, getattr(yp_set('january'), methname), X(data))
                self.assertRaises(TypeError, getattr(yp_set('january'), methname), N(data))
                self.assertRaises(ZeroDivisionError, getattr(yp_set('january'), methname), E(data))

class bad_eq:
    def __eq__(self, other):
        if be_bad:
            set2.clear()
            raise ZeroDivisionError
        return self is other
    def __hash__(self):
        return 0

class bad_dict_clear:
    def __eq__(self, other):
        if be_bad:
            dict2.clear()
        return self is other
    def __hash__(self):
        return 0

class TestWeirdBugs(unittest.TestCase):
    @unittest.skip("REWORK: nohtyP sets don't store user-defined types")
    def test_8420_set_merge(self):
        # This used to segfault
        global be_bad, set2, dict2
        be_bad = False
        set1 = yp_set({bad_eq()})
        set2 = yp_set({bad_eq() for i in range(75)})
        be_bad = True
        self.assertRaises(ZeroDivisionError, set1.update, set2)

        be_bad = False
        set1 = yp_set({bad_dict_clear()})
        dict2 = yp_set({bad_dict_clear(): None})
        be_bad = True
        set1.symmetric_difference_update(dict2)

# Application tests (based on David Eppstein's graph recipes ====================================

def powerset(U):
    """Generates all subsets of a set or sequence U."""
    U = iter(U)
    try:
        x = yp_frozenset([next(U)])
        for S in powerset(U):
            yield S
            yield S | x
    except StopIteration:
        yield yp_frozenset()

def cube(n):
    """Graph of n-dimensional hypercube."""
    singletons = [yp_frozenset([x]) for x in range(n)]
    return dict([(x, yp_frozenset([x^s for s in singletons]))
                 for x in powerset(range(n))])

def linegraph(G):
    """Graph, the vertices of which are edges of G,
    with two vertices being adjacent iff the corresponding
    edges share a vertex."""
    L = {}
    for x in G:
        for y in G[x]:
            nx = [yp_frozenset([x,z]) for z in G[x] if z != y]
            ny = [yp_frozenset([y,z]) for z in G[y] if z != x]
            L[yp_frozenset([x,y])] = yp_frozenset(nx+ny)
    return L

def faces(G):
    'Return a set of faces in G.  Where a face is a set of vertices on that face'
    # currently limited to triangles,squares, and pentagons
    f = yp_set()
    for v1, edges in G.items():
        for v2 in edges:
            for v3 in G[v2]:
                if v1 == v3:
                    continue
                if v1 in G[v3]:
                    f.add(yp_frozenset([v1, v2, v3]))
                else:
                    for v4 in G[v3]:
                        if v4 == v2:
                            continue
                        if v1 in G[v4]:
                            f.add(yp_frozenset([v1, v2, v3, v4]))
                        else:
                            for v5 in G[v4]:
                                if v5 == v3 or v5 == v2:
                                    continue
                                if v1 in G[v5]:
                                    f.add(yp_frozenset([v1, v2, v3, v4, v5]))
    return f


class TestGraphs(unittest.TestCase):

    def test_cube(self):

        g = cube(3)                             # vert --> {v1, v2, v3}
        vertices1 = yp_set(g)
        self.assertEqual(len(vertices1), 8)     # eight vertices
        for edge in g.values():
            self.assertEqual(len(edge), 3)      # each vertex connects to three edges
        vertices2 = yp_set(v for edges in g.values() for v in edges)
        self.assertEqual(vertices1, vertices2)  # edge vertices in original set

        cubefaces = faces(g)
        self.assertEqual(len(cubefaces), 6)     # six faces
        for face in cubefaces:
            self.assertEqual(len(face), 4)      # each face is a square

    def test_cuboctahedron(self):

        # http://en.wikipedia.org/wiki/Cuboctahedron
        # 8 triangular faces and 6 square faces
        # 12 indentical vertices each connecting a triangle and square

        g = cube(3)
        cuboctahedron = linegraph(g)            # V( --> {V1, V2, V3, V4}
        self.assertEqual(len(cuboctahedron), 12)# twelve vertices

        vertices = yp_set(cuboctahedron)
        for edges in cuboctahedron.values():
            self.assertEqual(len(edges), 4)     # each vertex connects to four other vertices
        othervertices = yp_set(edge for edges in cuboctahedron.values() for edge in edges)
        self.assertEqual(vertices, othervertices)   # edge vertices in original set

        cubofaces = faces(cuboctahedron)
        facesizes = collections.defaultdict(int)
        for face in cubofaces:
            facesizes[len(face)] += 1
        self.assertEqual(facesizes[3], 8)       # eight triangular faces
        self.assertEqual(facesizes[4], 6)       # six square faces

        for vertex in cuboctahedron:
            edge = vertex                       # Cuboctahedron vertices are edges in Cube
            self.assertEqual(len(edge), 2)      # Two cube vertices define an edge
            for cubevert in edge:
                self.assertIn(cubevert, g)


#==============================================================================

def test_main(verbose=None):
    test_classes = (
        TestSet,
        TestSetSubclass,
        TestSetSubclassWithKeywordArgs,
        TestFrozenSet,
        TestFrozenSetSubclass,
        TestSetOfSets,
        TestExceptionPropagation,
        TestBasicOpsEmpty,
        TestBasicOpsSingleton,
        TestBasicOpsTuple,
        TestBasicOpsTriple,
        TestBasicOpsString,
        TestBasicOpsBytes,
        TestBasicOpsMixedStringBytes,
        TestBinaryOps,
        TestUpdateOps,
        TestMutate,
        TestSubsetEqualEmpty,
        TestSubsetEqualNonEmpty,
        TestSubsetEmptyNonEmpty,
        TestSubsetPartial,
        TestSubsetNonOverlap,
        TestOnlySetsNumeric,
        TestOnlySetsDict,
        TestOnlySetsOperator,
        TestOnlySetsTuple,
        TestOnlySetsString,
        TestOnlySetsGenerator,
        TestCopyingEmpty,
        TestCopyingSingleton,
        TestCopyingTriple,
        TestCopyingTuple,
        TestCopyingNested,
        TestIdentities,
        TestVariousIteratorArgs,
        TestGraphs,
        TestWeirdBugs,
        )

    support.run_unittest(*test_classes)

    # verify reference counting
    if verbose and hasattr(sys, "gettotalrefcount"):
        import gc
        counts = [None] * 5
        for i in range(len(counts)):
            support.run_unittest(*test_classes)
            gc.collect()
            counts[i] = sys.gettotalrefcount()
        print(counts)

if __name__ == "__main__":
    test_main(verbose=True)
