# Test iterators.

from yp import *
import sys
from python_test import yp_unittest
from python_test.support import cpython_only
from python_test.support.os_helper import TESTFN, unlink
from python_test.support import check_free_after_iterating, ALWAYS_EQ, NEVER_EQ
import pickle
import collections.abc

# Extra assurance that we're not accidentally testing Python's data types
def iter(*args, **kwargs): raise NotImplementedError("convert script to yp_iter here")
def bytes(*args, **kwargs): raise NotImplementedError("convert script to yp_bytes here")
def bytearray(*args, **kwargs): raise NotImplementedError("convert script to yp_bytearray here")
def str(*args, **kwargs): raise NotImplementedError("convert script to yp_str here")
def tuple(*args, **kwargs): raise NotImplementedError("convert script to yp_tuple here")
def list(*args, **kwargs): raise NotImplementedError("convert script to yp_list here")
def frozenset(*args, **kwargs): raise NotImplementedError("convert script to yp_frozenset here")
def set(*args, **kwargs): raise NotImplementedError("convert script to yp_set here")
def dict(*args, **kwargs): raise NotImplementedError("convert script to yp_dict here")
# TODO same for yp_range, yp_min, yp_max, etc
# TODO yp_iter(x) throws TypeError if x not a ypObject

# Test result of triple loop (too big to inline)
TRIPLETS = yp_list(
           [(0, 0, 0), (0, 0, 1), (0, 0, 2),
            (0, 1, 0), (0, 1, 1), (0, 1, 2),
            (0, 2, 0), (0, 2, 1), (0, 2, 2),

            (1, 0, 0), (1, 0, 1), (1, 0, 2),
            (1, 1, 0), (1, 1, 1), (1, 1, 2),
            (1, 2, 0), (1, 2, 1), (1, 2, 2),

            (2, 0, 0), (2, 0, 1), (2, 0, 2),
            (2, 1, 0), (2, 1, 1), (2, 1, 2),
            (2, 2, 0), (2, 2, 1), (2, 2, 2)])

# Helper classes

class BasicIterClass:
    def __init__(self, n):
        self.n = yp_int(n)
        self.i = yp_int(0)
    def __next__(self):
        res = self.i
        if res >= self.n:
            raise StopIteration
        self.i = res + 1
        return res
    def __iter__(self):
        return self

class IteratingSequenceClass:
    def __init__(self, n):
        self.n = yp_int(n)
    def __iter__(self):
        return BasicIterClass(self.n)

class IteratorProxyClass:
    def __init__(self, i):
        self.i = i
    def __next__(self):
        return next(self.i)
    def __iter__(self):
        return self

class SequenceClass:
    def __init__(self, n):
        self.n = yp_int(n)
    def __getitem__(self, i):
        if 0 <= i < self.n:
            return yp_int(i)
        else:
            raise IndexError

class SequenceProxyClass:
    def __init__(self, s):
        self.s = s
    def __getitem__(self, i):
        return self.s[i]

class UnlimitedSequenceClass:
    def __getitem__(self, i):
        return i

class DefaultIterClass:
    pass

class NoIterClass:
    def __getitem__(self, i):
        return i
    __iter__ = None

class BadIterableClass:
    def __iter__(self):
        raise ZeroDivisionError

# Main test suite

class TestCase(yp_unittest.TestCase):

    # Helper to check that an iterator returns a given sequence
    def check_iterator(self, it, seq, pickle=True):
        if pickle:
            self.check_pickle(it, seq)
        res = yp_list()
        while 1:
            try:
                val = next(it)
            except StopIteration:
                break
            res.append(val)
        self.assertEqual(res, seq)

    # Helper to check that a for loop generates a given sequence
    def check_for_loop(self, expr, seq, pickle=True):
        if pickle:
            self.check_pickle(yp_iter(expr), seq)
        res = yp_list()
        for val in expr:
            res.append(val)
        self.assertEqual(res, seq)

    # Helper to check picklability
    @yp_unittest.skip_pickling
    def check_pickle(self, itorg, seq):
        for proto in range(pickle.HIGHEST_PROTOCOL + 1):
            d = pickle.dumps(itorg, proto)
            it = pickle.loads(d)
            # Cannot assert type equality because dict iterators unpickle as list
            # iterators.
            # self.assertEqual(type(itorg), type(it))
            self.assertTrue(isinstance(it, collections.abc.Iterator))
            self.assertEqual(yp_list(it), seq)

            it = pickle.loads(d)
            try:
                next(it)
            except StopIteration:
                continue
            d = pickle.dumps(it, proto)
            it = pickle.loads(d)
            self.assertEqual(yp_list(it), seq[1:])

    # Test basic use of iter() function
    def test_iter_basic(self):
        self.check_iterator(yp_iter(yp_range(10)), yp_list(yp_range(10)))

    # Test that iter(iter(x)) is the same as iter(x)
    def test_iter_idempotency(self):
        seq = yp_list(yp_range(10))
        it = yp_iter(seq)
        it2 = yp_iter(it)
        self.assertIs(it, it2)

    # Test that for loops over iterators work
    def test_iter_for_loop(self):
        self.check_for_loop(yp_iter(yp_range(10)), yp_list(yp_range(10)))

    # Test several independent iterators over the same list
    def test_iter_independence(self):
        seq = yp_range(3)
        res = yp_list()
        for i in yp_iter(seq):
            for j in yp_iter(seq):
                for k in yp_iter(seq):
                    res.append(yp_tuple((i, j, k)))
        self.assertEqual(res, TRIPLETS)

    # Test triple list comprehension using iterators
    def test_nested_comprehensions_iter(self):
        seq = yp_range(3)
        res = yp_list([yp_tuple((i, j, k))
               for i in yp_iter(seq) for j in yp_iter(seq) for k in yp_iter(seq)])
        self.assertEqual(res, TRIPLETS)

    # Test triple list comprehension without iterators
    def test_nested_comprehensions_for(self):
        seq = yp_range(3)
        res = yp_list([yp_tuple((i, j, k)) for i in seq for j in seq for k in seq])
        self.assertEqual(res, TRIPLETS)

    # Test a class with __iter__ in a for loop
    def test_iter_class_for(self):
        self.check_for_loop(IteratingSequenceClass(10), yp_list(yp_range(10)))

    # Test a class with __iter__ with explicit iter()
    def test_iter_class_iter(self):
        self.check_iterator(yp_iter(IteratingSequenceClass(10)), yp_list(yp_range(10)))

    # Test for loop on a sequence class without __iter__
    def test_seq_class_for(self):
        self.check_for_loop(SequenceClass(10), yp_list(yp_range(10)))

    # Test iter() on a sequence class without __iter__
    def test_seq_class_iter(self):
        self.check_iterator(yp_iter(SequenceClass(10)), yp_list(yp_range(10)))

    @yp_unittest.skip_pickling
    def test_mutating_seq_class_iter_pickle(self):
        orig = SequenceClass(5)
        for proto in range(pickle.HIGHEST_PROTOCOL + 1):
            # initial iterator
            itorig = yp_iter(orig)
            d = pickle.dumps((itorig, orig), proto)
            it, seq = pickle.loads(d)
            seq.n = 7
            self.assertIs(type(it), type(itorig))
            self.assertEqual(yp_list(it), yp_list(yp_range(7)))

            # running iterator
            next(itorig)
            d = pickle.dumps((itorig, orig), proto)
            it, seq = pickle.loads(d)
            seq.n = 7
            self.assertIs(type(it), type(itorig))
            self.assertEqual(yp_list(it), yp_list(yp_range(1, 7)))

            # empty iterator
            for i in range(1, 5):
                next(itorig)
            d = pickle.dumps((itorig, orig), proto)
            it, seq = pickle.loads(d)
            seq.n = 7
            self.assertIs(type(it), type(itorig))
            self.assertEqual(yp_list(it), yp_list(yp_range(5, 7)))

            # exhausted iterator
            self.assertRaises(StopIteration, next, itorig)
            d = pickle.dumps((itorig, orig), proto)
            it, seq = pickle.loads(d)
            seq.n = 7
            self.assertTrue(isinstance(it, collections.abc.Iterator))
            self.assertEqual(yp_list(it), [])

    def test_mutating_seq_class_exhausted_iter(self):
        a = SequenceClass(5)
        exhit = yp_iter(a)
        empit = yp_iter(a)
        for x in exhit:  # exhaust the iterator
            next(empit)  # not exhausted
        a.n = 7
        self.assertEqual(yp_list(exhit), [])
        self.assertEqual(yp_list(empit), [5, 6])
        self.assertEqual(yp_list(a), [0, 1, 2, 3, 4, 5, 6])

    # Test a new_style class with __iter__ but no next() method
    def test_new_style_iter_class(self):
        class IterClass(object):
            def __iter__(self):
                return self
        self.assertRaises(TypeError, yp_iter, IterClass())

    # Test two-argument iter() with callable instance
    def test_iter_callable(self):
        class C:
            def __init__(self):
                self.i = 0
            def __call__(self):
                i = self.i
                self.i = i + 1
                if i > 100:
                    raise IndexError # Emergency stop
                return i
        self.check_iterator(yp_iter(C(), 10), yp_list(yp_range(10)), pickle=False)

    # Test two-argument iter() with function
    def test_iter_function(self):
        def spam(state=[0]):
            i = state[0]
            state[0] = i+1
            return i
        self.check_iterator(yp_iter(spam, 10), yp_list(yp_range(10)), pickle=False)

    # Test two-argument iter() with function that raises StopIteration
    def test_iter_function_stop(self):
        def spam(state=[0]):
            i = state[0]
            if i == 10:
                raise StopIteration
            state[0] = i+1
            return i
        self.check_iterator(yp_iter(spam, 20), yp_list(yp_range(10)), pickle=False)

    # Test exception propagation through function iterator
    def test_exception_function(self):
        def spam(state=[0]):
            i = state[0]
            state[0] = i+1
            if i == 10:
                raise RuntimeError
            return i
        res = yp_list()
        try:
            for x in yp_iter(spam, 20):
                res.append(x)
        except RuntimeError:
            self.assertEqual(res, yp_list(yp_range(10)))
        else:
            self.fail("should have raised RuntimeError")

    # Test exception propagation through sequence iterator
    def test_exception_sequence(self):
        class MySequenceClass(SequenceClass):
            def __getitem__(self, i):
                if i == 10:
                    raise RuntimeError
                return SequenceClass.__getitem__(self, i)
        res = yp_list()
        try:
            for x in MySequenceClass(20):
                res.append(x)
        except RuntimeError:
            self.assertEqual(res, yp_list(yp_range(10)))
        else:
            self.fail("should have raised RuntimeError")

    # Test for StopIteration from __getitem__
    def test_stop_sequence(self):
        class MySequenceClass(SequenceClass):
            def __getitem__(self, i):
                if i == 10:
                    raise StopIteration
                return SequenceClass.__getitem__(self, i)
        self.check_for_loop(MySequenceClass(20), yp_list(yp_range(10)), pickle=False)

    # Test a big range
    def test_iter_big_range(self):
        self.check_for_loop(yp_iter(yp_range(10000)), yp_list(yp_range(10000)))

    # Test an empty list
    def test_iter_empty(self):
        self.check_for_loop(yp_iter(yp_list()), [])

    # Test a tuple
    def test_iter_tuple(self):
        self.check_for_loop(yp_iter(yp_tuple((0,1,2,3,4,5,6,7,8,9))), yp_list(yp_range(10)))

    # Test a range
    def test_iter_range(self):
        self.check_for_loop(yp_iter(yp_range(10)), yp_list(yp_range(10)))

    # Test a string
    def test_iter_string(self):
        self.check_for_loop(yp_iter(yp_str("abcde")), ["a", "b", "c", "d", "e"])

    # Test a directory
    def test_iter_dict(self):
        dict = yp_dict()
        for i in yp_range(10):
            dict[i] = None
        self.check_for_loop(dict, yp_list(dict.keys()))

    # Test a file
    @yp_unittest.skip_files
    def test_iter_file(self):
        f = open(TESTFN, "w", encoding="utf-8")
        try:
            for i in range(5):
                f.write("%d\n" % i)
        finally:
            f.close()
        f = open(TESTFN, "r", encoding="utf-8")
        try:
            self.check_for_loop(f, ["0\n", "1\n", "2\n", "3\n", "4\n"], pickle=False)
            self.check_for_loop(f, [], pickle=False)
        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass

    # Test list()'s use of iterators.
    def test_builtin_list(self):
        self.assertEqual(yp_list(SequenceClass(5)), yp_list(yp_range(5)))
        self.assertEqual(yp_list(SequenceClass(0)), [])
        self.assertEqual(yp_list(yp_tuple()), [])

        d = yp_dict({"one": 1, "two": 2, "three": 3})
        self.assertEqual(yp_list(d), yp_list(d.keys()))

        self.assertRaises(TypeError, yp_list, yp_list)
        self.assertRaises(TypeError, yp_list, 42)

        f = open(TESTFN, "w", encoding="utf-8")
        try:
            for i in range(5):
                f.write("%d\n" % i)
        finally:
            f.close()
        f = open(TESTFN, "r", encoding="utf-8")
        try:
            self.assertEqual(yp_list(f), ["0\n", "1\n", "2\n", "3\n", "4\n"])
            f.seek(0, 0)
            self.assertEqual(yp_list(f),
                             ["0\n", "1\n", "2\n", "3\n", "4\n"])
        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass

    # Test tuples()'s use of iterators.
    def test_builtin_tuple(self):
        self.assertEqual(yp_tuple(SequenceClass(5)), (0, 1, 2, 3, 4))
        self.assertEqual(yp_tuple(SequenceClass(0)), ())
        self.assertEqual(yp_tuple(yp_list()), ())
        self.assertEqual(yp_tuple(), ())
        self.assertEqual(yp_tuple(yp_str("abc")), ("a", "b", "c"))

        d = yp_dict({"one": 1, "two": 2, "three": 3})
        self.assertEqual(yp_tuple(d), yp_tuple(d.keys()))

        self.assertRaises(TypeError, yp_tuple, yp_list)
        self.assertRaises(TypeError, yp_tuple, 42)

        f = open(TESTFN, "w", encoding="utf-8")
        try:
            for i in range(5):
                f.write("%d\n" % i)
        finally:
            f.close()
        f = open(TESTFN, "r", encoding="utf-8")
        try:
            self.assertEqual(yp_tuple(f), ("0\n", "1\n", "2\n", "3\n", "4\n"))
            f.seek(0, 0)
            self.assertEqual(yp_tuple(f),
                             ("0\n", "1\n", "2\n", "3\n", "4\n"))
        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass

    # Test filter()'s use of iterators.
    @yp_unittest.skip_filter
    def test_builtin_filter(self):
        self.assertEqual(yp_list(yp_filter(None, SequenceClass(5))),
                         yp_list(yp_range(1, 5)))
        self.assertEqual(yp_list(yp_filter(None, SequenceClass(0))), [])
        self.assertEqual(yp_list(yp_filter(None, ())), [])
        self.assertEqual(yp_list(yp_filter(None, "abc")), ["a", "b", "c"])

        d = yp_dict({"one": 1, "two": 2, "three": 3})
        self.assertEqual(yp_list(yp_filter(None, d)), yp_list(d.keys()))

        self.assertRaises(TypeError, yp_filter, None, yp_list)
        self.assertRaises(TypeError, yp_filter, None, 42)

        #class Boolean:
        #    def __init__(self, truth):
        #        self.truth = truth
        #    def __bool__(self):
        #        return self.truth
        #bTrue = Boolean(True)
        #bFalse = Boolean(False)
        bTrue = yp_dict({1:1})
        bFalse = yp_dict()

        class Seq:
            def __init__(self, *args):
                self.vals = args
            def __iter__(self):
                class SeqIter:
                    def __init__(self, vals):
                        self.vals = vals
                        self.i = 0
                    def __iter__(self):
                        return self
                    def __next__(self):
                        i = self.i
                        self.i = i + 1
                        if i < len(self.vals):
                            return self.vals[i]
                        else:
                            raise StopIteration
                return SeqIter(self.vals)

        seq = Seq(*([bTrue, bFalse] * 25))
        self.assertEqual(yp_list(yp_filter(lambda x: not x, seq)), [bFalse]*25)
        self.assertEqual(yp_list(yp_filter(lambda x: not x, yp_iter(seq))), [bFalse]*25)

    # Test max() and min()'s use of iterators.
    @yp_unittest.skip_min
    def test_builtin_max_min(self):
        self.assertEqual(yp_max(SequenceClass(5)), 4)
        self.assertEqual(yp_min(SequenceClass(5)), 0)
        self.assertEqual(yp_max(8, -1), 8)
        self.assertEqual(yp_min(8, -1), -1)

        d = yp_dict({"one": 1, "two": 2, "three": 3})
        self.assertEqual(yp_max(d), "two")
        self.assertEqual(yp_min(d), "one")
        self.assertEqual(yp_max(d.values()), 3)
        self.assertEqual(yp_min(yp_iter(d.values())), 1)

        f = open(TESTFN, "w", encoding="utf-8")
        try:
            f.write("medium line\n")
            f.write("xtra large line\n")
            f.write("itty-bitty line\n")
        finally:
            f.close()
        f = open(TESTFN, "r", encoding="utf-8")
        try:
            self.assertEqual(yp_min(f), "itty-bitty line\n")
            f.seek(0, 0)
            self.assertEqual(yp_max(f), "xtra large line\n")
        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass

    # Test map()'s use of iterators.
    @yp_unittest.skip_map
    def test_builtin_map(self):
        self.assertEqual(yp_list(yp_map(lambda x: x+1, SequenceClass(5))),
                         yp_list(yp_range(1, 6)))

        d = yp_dict({"one": 1, "two": 2, "three": 3})
        self.assertEqual(yp_list(yp_map(lambda k, d=d: (k, d[k]), d)),
                         yp_list(d.items()))
        dkeys = yp_list(d.keys())
        expected = [(i < len(d) and dkeys[i] or None,
                     i,
                     i < len(d) and dkeys[i] or None)
                    for i in range(3)]

        f = open(TESTFN, "w", encoding="utf-8")
        try:
            for i in range(10):
                f.write("xy" * i + "\n") # line i has len 2*i+1
        finally:
            f.close()
        f = open(TESTFN, "r", encoding="utf-8")
        try:
            self.assertEqual(yp_list(yp_map(len, f)), yp_list(yp_range(1, 21, 2)))
        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass

    # Test zip()'s use of iterators.
    @yp_unittest.skip_zip
    def test_builtin_zip(self):
        self.assertEqual(yp_list(zip()), [])
        self.assertEqual(yp_list(zip(*yp_list())), [])
        self.assertEqual(yp_list(zip(*yp_list([(1, 2), 'ab']))), [(1, 'a'), (2, 'b')])

        self.assertRaises(TypeError, zip, None)
        self.assertRaises(TypeError, zip, yp_range(10), 42)
        self.assertRaises(TypeError, zip, yp_range(10), zip)

        self.assertEqual(yp_list(zip(IteratingSequenceClass(3))),
                         [(0,), (1,), (2,)])
        self.assertEqual(yp_list(zip(SequenceClass(3))),
                         [(0,), (1,), (2,)])

        d = yp_dict({"one": 1, "two": 2, "three": 3})
        self.assertEqual(yp_list(d.items()), yp_list(zip(d, d.values())))

        # Generate all ints starting at constructor arg.
        class IntsFrom:
            def __init__(self, start):
                self.i = start

            def __iter__(self):
                return self

            def __next__(self):
                i = self.i
                self.i = i+1
                return i

        f = open(TESTFN, "w", encoding="utf-8")
        try:
            f.write("a\n" "bbb\n" "cc\n")
        finally:
            f.close()
        f = open(TESTFN, "r", encoding="utf-8")
        try:
            self.assertEqual(yp_list(zip(IntsFrom(0), f, IntsFrom(-100))),
                             [(0, "a\n", -100),
                              (1, "bbb\n", -99),
                              (2, "cc\n", -98)])
        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass

        self.assertEqual(yp_list(zip(yp_range(5))), [(i,) for i in yp_range(5)])

        # Classes that lie about their lengths.
        class NoGuessLen5:
            def __getitem__(self, i):
                if i >= 5:
                    raise IndexError
                return i

        class Guess3Len5(NoGuessLen5):
            def __len__(self):
                return 3

        class Guess30Len5(NoGuessLen5):
            def __len__(self):
                return 30

        def lzip(*args):
            return yp_list(zip(*args))

        self.assertEqual(yp_len(Guess3Len5()), 3)
        self.assertEqual(yp_len(Guess30Len5()), 30)
        self.assertEqual(lzip(NoGuessLen5()), lzip(yp_range(5)))
        self.assertEqual(lzip(Guess3Len5()), lzip(yp_range(5)))
        self.assertEqual(lzip(Guess30Len5()), lzip(yp_range(5)))

        expected = [(i, i) for i in yp_range(5)]
        for x in NoGuessLen5(), Guess3Len5(), Guess30Len5():
            for y in NoGuessLen5(), Guess3Len5(), Guess30Len5():
                self.assertEqual(lzip(x, y), expected)

    @yp_unittest.skip_user_defined_types
    def test_unicode_join_endcase(self):

        # This class inserts a Unicode object into its argument's natural
        # iteration, in the 3rd position.
        class OhPhooey:
            def __init__(self, seq):
                self.it = yp_iter(seq)
                self.i = 0

            def __iter__(self):
                return self

            def __next__(self):
                i = self.i
                self.i = i+1
                if i == 2:
                    return yp_str("fooled you!")
                return next(self.it)

        f = open(TESTFN, "w", encoding="utf-8")
        try:
            f.write("a\n" + "b\n" + "c\n")
        finally:
            f.close()

        f = open(TESTFN, "r", encoding="utf-8")
        # Nasty:  string.join(s) can't know whether unicode.join() is needed
        # until it's seen all of s's elements.  But in this case, f's
        # iterator cannot be restarted.  So what we're testing here is
        # whether string.join() can manage to remember everything it's seen
        # and pass that on to unicode.join().
        try:
            got = yp_str(" - ").join(OhPhooey(f))
            self.assertEqual(got, "a\n - b\n - fooled you! - c\n")
        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass

    # Test iterators with 'x in y' and 'x not in y'.
    @yp_unittest.skip_user_defined_types
    def test_in_and_not_in_user_defined_types(self):
        for sc5 in IteratingSequenceClass(5), SequenceClass(5):
            for i in yp_range(5):
                self.assertIn(yp_int(i), sc5)
            for i in (yp_str("abc"), yp_int(-1), yp_int(5), yp_float(42.42), yp_tuple((3, 4)),
                    yp_list(), yp_dict({1: 1})):
                self.assertNotIn(i, sc5)

        self.assertIn(ALWAYS_EQ, IteratorProxyClass(yp_iter([1])))
        self.assertIn(ALWAYS_EQ, SequenceProxyClass([1]))
        self.assertNotIn(ALWAYS_EQ, IteratorProxyClass(yp_iter([NEVER_EQ])))
        self.assertNotIn(ALWAYS_EQ, SequenceProxyClass([NEVER_EQ]))
        self.assertIn(NEVER_EQ, IteratorProxyClass(yp_iter([ALWAYS_EQ])))
        self.assertIn(NEVER_EQ, SequenceProxyClass([ALWAYS_EQ]))

        self.assertRaises(TypeError, lambda: yp_int(3) in yp_int(12))
        self.assertRaises(TypeError, lambda: yp_int(3) not in yp_map)
        self.assertRaises(ZeroDivisionError, lambda: yp_int(3) in BadIterableClass())

    def test_in_and_not_in_unsupported(self):
        for x in yp_iter(()), yp_dict, yp_None, yp_True, yp_int(12), yp_float(12.2), yp_chr:
            self.assertRaises(TypeError, lambda: yp_int(3) in x)
            self.assertRaises(TypeError, lambda: yp_int(3) not in x)

    def test_in_and_not_in_dict_views(self):
        d = yp_dict({"one": 1, "two": 2, "three": 3, 1.1: 2.1})
        for k in d:
            self.assertIn(k, d)
            self.assertNotIn(k, d.values())
        for v in d.values():
            self.assertIn(v, d.values())
            self.assertNotIn(v, d)
        for k, v in d.items():
            self.assertIn(yp_tuple((k, v)), d.items())
            self.assertNotIn(yp_tuple((v, k)), d.items())

    @yp_unittest.skip_files
    def test_in_and_not_in_file(self):
        f = open(TESTFN, "w", encoding="utf-8")
        try:
            f.write("a\n" "b\n" "c\n")
        finally:
            f.close()
        f = open(TESTFN, "r", encoding="utf-8")
        try:
            for chunk in yp_str("abc"):
                f.seek(0, 0)
                self.assertNotIn(chunk, f)
                f.seek(0, 0)
                self.assertIn((chunk + "\n"), f)
        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass

    # Test iterators with operator.countOf (PySequence_Count).
    def test_countOf(self):
        from operator import countOf
        with self.nohtyPCheck(enabled=False):
            self.assertEqual(countOf(yp_list([1,2,2,3,2,5]), 2), 3)
            self.assertEqual(countOf(yp_tuple((1,2,2,3,2,5)), 2), 3)
            self.assertEqual(countOf(yp_str("122325"), "2"), 3)
            self.assertEqual(countOf(yp_str("122325"), "6"), 0)

            self.assertRaises(TypeError, countOf, yp_int(42), 1)
            self.assertRaises(TypeError, countOf, yp_func_chr, yp_func_chr)

            d = yp_dict({"one": 3, "two": 3, "three": 3, 1.1: 2.2})
            for k in d:
                self.assertEqual(countOf(d, k), 1)
            self.assertEqual(countOf(d.values(), 3), 3)
            self.assertEqual(countOf(d.values(), 2.2), 1)
            self.assertEqual(countOf(d.values(), 1.1), 0)

    @yp_unittest.skip_files
    def test_countOf_file(self):
        f = open(TESTFN, "w", encoding="utf-8")
        try:
            f.write("a\n" "b\n" "c\n" "b\n")
        finally:
            f.close()
        f = open(TESTFN, "r", encoding="utf-8")
        try:
            for letter, count in yp_tuple((("a", 1), ("b", 2), ("c", 1), ("d", 0))):
                f.seek(0, 0)
                self.assertEqual(countOf(f, letter + "\n"), count)
        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass

    # Test iterators with operator.indexOf (PySequence_Index).
    def test_indexOf(self):
        from operator import indexOf
        with self.nohtyPCheck(enabled=False):
            self.assertEqual(indexOf(yp_list([1,2,2,3,2,5]), 1), 0)
            self.assertEqual(indexOf(yp_tuple((1,2,2,3,2,5)), 2), 1)
            self.assertEqual(indexOf(yp_tuple((1,2,2,3,2,5)), 3), 3)
            self.assertEqual(indexOf(yp_tuple((1,2,2,3,2,5)), 5), 5)
            self.assertRaises(ValueError, indexOf, yp_tuple((1,2,2,3,2,5)), 0)
            self.assertRaises(ValueError, indexOf, yp_tuple((1,2,2,3,2,5)), 6)

            self.assertEqual(indexOf(yp_str("122325"), "2"), 1)
            self.assertEqual(indexOf(yp_str("122325"), "5"), 5)
            self.assertRaises(ValueError, indexOf, yp_str("122325"), "6")

        self.assertRaises(TypeError, indexOf, yp_int(42), 1)
        self.assertRaises(TypeError, indexOf, yp_func_chr, yp_func_chr)
        # TODO(skip_not_applicable) Fails if run with Python 3.8.0 (but not >=3.8.10).
        # self.assertRaises(ZeroDivisionError, indexOf, BadIterableClass(), 1)

    @yp_unittest.skip_files
    def test_indexOf_file(self):
        f = open(TESTFN, "w", encoding="utf-8")
        try:
            f.write("a\n" "b\n" "c\n" "d\n" "e\n")
        finally:
            f.close()
        f = open(TESTFN, "r", encoding="utf-8")
        try:
            fiter = yp_iter(f)
            self.assertEqual(indexOf(fiter, "b\n"), 1)
            self.assertEqual(indexOf(fiter, "d\n"), 1)
            self.assertEqual(indexOf(fiter, "e\n"), 0)
            self.assertRaises(ValueError, indexOf, fiter, "a\n")
        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass

    @yp_unittest.skip_user_defined_types
    def test_indexOf_class(self):
        iclass = IteratingSequenceClass(3)
        for i in yp_range(3):
            self.assertEqual(indexOf(iclass, i), i)
        self.assertRaises(ValueError, indexOf, iclass, -1)

    # Test iterators with file.writelines().
    @yp_unittest.skip_files
    def test_writelines(self):
        f = open(TESTFN, "w", encoding="utf-8")

        try:
            self.assertRaises(TypeError, f.writelines, None)
            self.assertRaises(TypeError, f.writelines, 42)

            f.writelines(yp_list(["1\n", "2\n"]))
            f.writelines(yp_tuple(("3\n", "4\n")))
            f.writelines(yp_dict({'5\n': None}))
            f.writelines(yp_dict())

            # Try a big chunk too.
            class Iterator:
                def __init__(self, start, finish):
                    self.start = start
                    self.finish = finish
                    self.i = self.start

                def __next__(self):
                    if self.i >= self.finish:
                        raise StopIteration
                    result = yp_str(self.i) + '\n'
                    self.i += 1
                    return result

                def __iter__(self):
                    return self

            class Whatever:
                def __init__(self, start, finish):
                    self.start = start
                    self.finish = finish

                def __iter__(self):
                    return Iterator(self.start, self.finish)

            f.writelines(Whatever(6, 6+2000))
            f.close()

            f = open(TESTFN, encoding="utf-8")
            expected = [yp_str(i) + "\n" for i in range(1, 2006)]
            self.assertEqual(yp_list(f), expected)

        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass


    # Test iterators on RHS of unpacking assignments.
    @yp_unittest.skip_unpack
    def test_unpack_iter(self):
        a, b = 1, 2
        self.assertEqual((a, b), (1, 2))

        a, b, c = IteratingSequenceClass(3)
        self.assertEqual((a, b, c), (0, 1, 2))

        try:    # too many values
            a, b = IteratingSequenceClass(3)
        except ValueError:
            pass
        else:
            self.fail("should have raised ValueError")

        try:    # not enough values
            a, b, c = IteratingSequenceClass(2)
        except ValueError:
            pass
        else:
            self.fail("should have raised ValueError")

        try:    # not iterable
            a, b, c = yp_len
        except TypeError:
            pass
        else:
            self.fail("should have raised TypeError")

        a, b, c = {1: 42, 2: 42, 3: 42}.values()
        self.assertEqual((a, b, c), (42, 42, 42))

        f = open(TESTFN, "w", encoding="utf-8")
        lines = ("a\n", "bb\n", "ccc\n")
        try:
            for line in lines:
                f.write(line)
        finally:
            f.close()
        f = open(TESTFN, "r", encoding="utf-8")
        try:
            a, b, c = f
            self.assertEqual((a, b, c), lines)
        finally:
            f.close()
            try:
                unlink(TESTFN)
            except OSError:
                pass

        (a, b), (c,) = IteratingSequenceClass(2), {42: 24}
        self.assertEqual((a, b, c), (0, 1, 42))


    @yp_unittest.skip_not_applicable
    @cpython_only
    def test_ref_counting_behavior(self):
        class C(object):
            count = 0
            def __new__(cls):
                cls.count += 1
                return object.__new__(cls)
            def __del__(self):
                cls = self.__class__
                assert cls.count > 0
                cls.count -= 1
        x = C()
        self.assertEqual(C.count, 1)
        del x
        self.assertEqual(C.count, 0)
        l = [C(), C(), C()]
        self.assertEqual(C.count, 3)
        try:
            a, b = yp_iter(l)
        except ValueError:
            pass
        del l
        self.assertEqual(C.count, 0)


    # Make sure StopIteration is a "sink state".
    # This tests various things that weren't sink states in Python 2.2.1,
    # plus various things that always were fine.

    def test_sinkstate_list(self):
        # This used to fail
        a = yp_list(yp_range(5))
        b = yp_iter(a)
        self.assertEqual(yp_list(b), yp_list(yp_range(5)))
        a.extend(yp_range(5, 10))
        self.assertEqual(yp_list(b), [])

    def test_sinkstate_tuple(self):
        a = yp_tuple((0, 1, 2, 3, 4))
        b = yp_iter(a)
        self.assertEqual(yp_list(b), yp_list(yp_range(5)))
        self.assertEqual(yp_list(b), [])

    def test_sinkstate_string(self):
        a = yp_str("abcde")
        b = yp_iter(a)
        self.assertEqual(yp_list(b), ['a', 'b', 'c', 'd', 'e'])
        self.assertEqual(yp_list(b), [])

    def test_sinkstate_sequence(self):
        # This used to fail
        a = SequenceClass(5)
        b = yp_iter(a)
        self.assertEqual(yp_list(b), yp_list(yp_range(5)))
        a.n = 10
        self.assertEqual(yp_list(b), [])

    def test_sinkstate_callable(self):
        # This used to fail
        def spam(state=[0]):
            i = state[0]
            state[0] = i+1
            if i == 10:
                raise AssertionError("shouldn't have gotten this far")
            return i
        b = yp_iter(spam, 5)
        self.assertEqual(yp_list(b), yp_list(yp_range(5)))
        self.assertEqual(yp_list(b), [])

    def test_sinkstate_dict(self):
        # XXX For a more thorough test, see towards the end of:
        # http://mail.python.org/pipermail/python-dev/2002-July/026512.html
        a = yp_dict({1:1, 2:2, 0:0, 4:4, 3:3})
        for b in yp_iter(a), a.keys(), a.items(), a.values():
            b = yp_iter(a)
            self.assertEqual(yp_len(yp_list(b)), 5)
            self.assertEqual(yp_list(b), [])

    def test_sinkstate_yield(self):
        def gen():
            for i in yp_range(5):
                yield i
        b = gen()
        self.assertEqual(yp_list(b), yp_list(yp_range(5)))
        self.assertEqual(yp_list(b), [])

    def test_sinkstate_range(self):
        a = yp_range(5)
        b = yp_iter(a)
        self.assertEqual(yp_list(b), yp_list(yp_range(5)))
        self.assertEqual(yp_list(b), [])

    def test_sinkstate_enumerate(self):
        a = yp_range(5)
        e = enumerate(a)
        b = yp_iter(e)
        self.assertEqual(yp_list(b), yp_list(zip(yp_range(5), yp_range(5))))
        self.assertEqual(yp_list(b), [])

    def test_3720(self):
        # Avoid a crash, when an iterator deletes its next() method.
        class BadIterator(object):
            def __iter__(self):
                return self
            def __next__(self):
                del BadIterator.__next__
                return 1

        try:
            for i in BadIterator() :
                pass
        except TypeError:
            pass

    def test_extending_list_with_iterator_does_not_segfault(self):
        # The code to extend a list with an iterator has a fair
        # amount of nontrivial logic in terms of guessing how
        # much memory to allocate in advance, "stealing" refs,
        # and then shrinking at the end.  This is a basic smoke
        # test for that scenario.
        def gen():
            for i in yp_range(500):
                yield i
        lst = yp_list([0]) * 500
        for i in yp_range(240):
            lst.pop(0)
        lst.extend(gen())
        self.assertEqual(yp_len(lst), 760)

    @yp_unittest.skip_pickling
    @cpython_only
    def test_iter_overflow(self):
        # Test for the issue 22939
        it = yp_iter(UnlimitedSequenceClass())
        # Manually set `it_index` to PY_SSIZE_T_MAX-2 without a loop
        it.__setstate__(sys.maxsize - 2)
        self.assertEqual(next(it), sys.maxsize - 2)
        self.assertEqual(next(it), sys.maxsize - 1)
        with self.assertRaises(OverflowError):
            next(it)
        # Check that Overflow error is always raised
        with self.assertRaises(OverflowError):
            next(it)

    @yp_unittest.skip_pickling
    def test_iter_neg_setstate(self):
        it = yp_iter(UnlimitedSequenceClass())
        it.__setstate__(-42)
        self.assertEqual(next(it), 0)
        self.assertEqual(next(it), 1)

    @yp_unittest.skip_user_defined_types
    def test_free_after_iterating(self):
        check_free_after_iterating(self, yp_iter, SequenceClass, (0,))

    def test_error_iter(self):
        for typ in (DefaultIterClass, NoIterClass):
            self.assertRaises(TypeError, yp_iter, typ())
        self.assertRaises(ZeroDivisionError, yp_iter, BadIterableClass())


if __name__ == "__main__":
    yp_unittest.main()
