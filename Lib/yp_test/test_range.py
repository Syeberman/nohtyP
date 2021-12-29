# Python test set -- built-in functions

from yp import *
from yp_test import yp_unittest
import sys
import pickle
import itertools
from test.support import ALWAYS_EQ

# Extra assurance that we're not accidentally testing Python's range
def range( *args, **kwargs ): raise NotImplementedError( "convert script to yp_range here" )

# pure Python implementations (3 args only), for comparison
def pyrange(start, stop, step):
    if (start - stop) // step < 0:
        # replace stop with next element in the sequence of integers
        # that are congruent to start modulo step.
        stop += (start - stop) % step
        while start != stop:
            yield start
            start += step

def pyrange_reversed(start, stop, step):
    stop += (start - stop) % step
    return pyrange(stop - step, start - step, -step)

# pure Python implementations (3 args only), for comparison
def pyrange_calc_len(start, stop, step):
    if step < 0:
        if stop >= start: return 0
        return 1 + (start - 1 - stop) // -step
    else:
        if start >= stop: return 0
        return 1 + (stop - 1 - start) // step

class RangeTest(yp_unittest.TestCase):
    def assert_iterators_equal(self, xs, ys, test_id, limit=None):
        # check that an iterator xs matches the expected results ys,
        # up to a given limit.
        if limit is not None:
            xs = itertools.islice(xs, limit)
            ys = itertools.islice(ys, limit)
        sentinel = object()
        pairs = itertools.zip_longest(xs, ys, fillvalue=sentinel)
        for i, (x, y) in enumerate(pairs):
            if x == y:
                continue
            elif x == sentinel:
                self.fail('{}: iterator ended unexpectedly '
                          'at position {}; expected {}'.format(test_id, i, y))
            elif y == sentinel:
                self.fail('{}: unexpected excess element {} at '
                          'position {}'.format(test_id, x, i))
            else:
                self.fail('{}: wrong element at position {}; '
                          'expected {}, got {}'.format(test_id, i, y, x))

    def test_range(self):
        self.assertEqual(yp_list(yp_range(3)), [0, 1, 2])
        self.assertEqual(yp_list(yp_range(1, 5)), [1, 2, 3, 4])
        self.assertEqual(yp_list(yp_range(0)), [])
        self.assertEqual(yp_list(yp_range(-3)), [])
        self.assertEqual(yp_list(yp_range(1, 10, 3)), [1, 4, 7])
        self.assertEqual(yp_list(yp_range(5, -5, -3)), [5, 2, -1, -4])

        a = 10
        b = 100
        c = 50

        self.assertEqual(yp_list(yp_range(a, a+2)), [a, a+1])
        self.assertEqual(yp_list(yp_range(a+2, a, -1)), [a+2, a+1])
        self.assertEqual(yp_list(yp_range(a+4, a, -2)), [a+4, a+2])

        seq = yp_list(yp_range(a, b, c))
        self.assertIn(a, seq)
        self.assertNotIn(b, seq)
        self.assertEqual(yp_len(seq), 2)

        seq = yp_list(yp_range(b, a, -c))
        self.assertIn(b, seq)
        self.assertNotIn(a, seq)
        self.assertEqual(yp_len(seq), 2)

        seq = yp_list(yp_range(-a, -b, -c))
        self.assertIn(-a, seq)
        self.assertNotIn(-b, seq)
        self.assertEqual(yp_len(seq), 2)

        self.assertRaises(TypeError, yp_range)
        self.assertRaises(TypeError, yp_range, 1, 2, 3, 4)
        self.assertRaises(ValueError, yp_range, 1, 2, 0)

        self.assertRaises(TypeError, yp_range, 0.0, 2, 1)
        self.assertRaises(TypeError, yp_range, 1, 2.0, 1)
        self.assertRaises(TypeError, yp_range, 1, 2, 1.0)
        self.assertRaises(TypeError, yp_range, 1e100, 1e101, 1e101)

        self.assertRaises(TypeError, yp_range, 0, "spam")
        self.assertRaises(TypeError, yp_range, 0, 42, "spam")

        self.assertEqual(yp_len(yp_range(0, sys.maxsize, sys.maxsize-1)), 2)

        # nohtyP has limitations on the length of the range and values of step
        #r = yp_range(-sys.maxsize, sys.maxsize, 2)
        #self.assertEqual(yp_len(r), sys.maxsize)
        self.assertEqual(yp_len(yp_range(0, ypObject_LEN_MAX)), ypObject_LEN_MAX)
        self.assertRaises(SystemError, yp_range, 0, ypObject_LEN_MAX+1)
        self.assertRaises(SystemError, yp_range, 0, 1, yp_sys_minint)

    def test_large_operands(self):
        x = yp_range(10**20, 10**20+10, 3)
        self.assertEqual(len(x), 4)
        self.assertEqual(len(list(x)), 4)

        x = yp_range(10**20+10, 10**20, 3)
        self.assertEqual(len(x), 0)
        self.assertEqual(len(list(x)), 0)
        self.assertFalse(x)

        x = yp_range(10**20, 10**20+10, -3)
        self.assertEqual(len(x), 0)
        self.assertEqual(len(list(x)), 0)
        self.assertFalse(x)

        x = yp_range(10**20+10, 10**20, -3)
        self.assertEqual(len(x), 4)
        self.assertEqual(len(list(x)), 4)
        self.assertTrue(x)

        # Now test yp_range() with longs
        for x in [yp_range(-2**100),
                  yp_range(0, -2**100),
                  yp_range(0, 2**100, -1)]:
            self.assertEqual(yp_list(x), yp_list())
            self.assertFalse(x)

        a = int(10 * sys.maxsize)
        b = int(100 * sys.maxsize)
        c = int(50 * sys.maxsize)

        self.assertEqual(list(yp_range(a, a+2)), [a, a+1])
        self.assertEqual(list(yp_range(a+2, a, -1)), [a+2, a+1])
        self.assertEqual(list(yp_range(a+4, a, -2)), [a+4, a+2])

        seq = list(yp_range(a, b, c))
        self.assertIn(a, seq)
        self.assertNotIn(b, seq)
        self.assertEqual(len(seq), 2)
        self.assertEqual(seq[0], a)
        self.assertEqual(seq[-1], a+c)

        seq = list(yp_range(b, a, -c))
        self.assertIn(b, seq)
        self.assertNotIn(a, seq)
        self.assertEqual(len(seq), 2)
        self.assertEqual(seq[0], b)
        self.assertEqual(seq[-1], b-c)

        seq = list(yp_range(-a, -b, -c))
        self.assertIn(-a, seq)
        self.assertNotIn(-b, seq)
        self.assertEqual(len(seq), 2)
        self.assertEqual(seq[0], -a)
        self.assertEqual(seq[-1], -a-c)

    @yp_unittest.skip_long_ints
    def test_large_range(self):
        # Check long ranges (len > sys.maxsize)
        # len() is expected to fail due to limitations of the __len__ protocol
        def _range_len(x):
            try:
                length = len(x)
            except OverflowError:
                step = x[1] - x[0]
                length = 1 + ((x[-1] - x[0]) // step)
            return length

        a = -sys.maxsize
        b = sys.maxsize
        expected_len = b - a
        x = yp_range(a, b)
        self.assertIn(a, x)
        self.assertNotIn(b, x)
        self.assertRaises(OverflowError, len, x)
        self.assertTrue(x)
        self.assertEqual(_range_len(x), expected_len)
        self.assertEqual(x[0], a)
        idx = sys.maxsize+1
        self.assertEqual(x[idx], a+idx)
        self.assertEqual(x[idx:idx+1][0], a+idx)
        with self.assertRaises(IndexError):
            x[-expected_len-1]
        with self.assertRaises(IndexError):
            x[expected_len]

        a = 0
        b = 2 * sys.maxsize
        expected_len = b - a
        x = yp_range(a, b)
        self.assertIn(a, x)
        self.assertNotIn(b, x)
        self.assertRaises(OverflowError, len, x)
        self.assertTrue(x)
        self.assertEqual(_range_len(x), expected_len)
        self.assertEqual(x[0], a)
        idx = sys.maxsize+1
        self.assertEqual(x[idx], a+idx)
        self.assertEqual(x[idx:idx+1][0], a+idx)
        with self.assertRaises(IndexError):
            x[-expected_len-1]
        with self.assertRaises(IndexError):
            x[expected_len]

        a = 0
        b = sys.maxsize**10
        c = 2*sys.maxsize
        expected_len = 1 + (b - a) // c
        x = yp_range(a, b, c)
        self.assertIn(a, x)
        self.assertNotIn(b, x)
        self.assertRaises(OverflowError, len, x)
        self.assertTrue(x)
        self.assertEqual(_range_len(x), expected_len)
        self.assertEqual(x[0], a)
        idx = sys.maxsize+1
        self.assertEqual(x[idx], a+(idx*c))
        self.assertEqual(x[idx:idx+1][0], a+(idx*c))
        with self.assertRaises(IndexError):
            x[-expected_len-1]
        with self.assertRaises(IndexError):
            x[expected_len]

        a = sys.maxsize**10
        b = 0
        c = -2*sys.maxsize
        expected_len = 1 + (b - a) // c
        x = yp_range(a, b, c)
        self.assertIn(a, x)
        self.assertNotIn(b, x)
        self.assertRaises(OverflowError, len, x)
        self.assertTrue(x)
        self.assertEqual(_range_len(x), expected_len)
        self.assertEqual(x[0], a)
        idx = sys.maxsize+1
        self.assertEqual(x[idx], a+(idx*c))
        self.assertEqual(x[idx:idx+1][0], a+(idx*c))
        with self.assertRaises(IndexError):
            x[-expected_len-1]
        with self.assertRaises(IndexError):
            x[expected_len]

    def test_invalid_invocation(self):
        self.assertRaises(TypeError, yp_range)
        self.assertRaises(TypeError, yp_range, 1, 2, 3, 4)
        self.assertRaises(ValueError, yp_range, 1, 2, 0)
        # nohtyP doesn't support values this high
        #a = int(10 * sys.maxsize)
        #self.assertRaises(ValueError, yp_range, a, a + 1, int(0))
        self.assertRaises(TypeError, yp_range, 1., 1., 1.)
        # nohtyP doesn't support values this high
        #self.assertRaises(TypeError, yp_range, 1e100, 1e101, 1e101)
        self.assertRaises(TypeError, yp_range, 0, "spam")
        self.assertRaises(TypeError, yp_range, 0, 42, "spam")
        # Exercise various combinations of bad arguments, to check
        # refcounting logic
        self.assertRaises(TypeError, yp_range, 0.0)
        self.assertRaises(TypeError, yp_range, 0, 0.0)
        self.assertRaises(TypeError, yp_range, 0.0, 0)
        self.assertRaises(TypeError, yp_range, 0.0, 0.0)
        self.assertRaises(TypeError, yp_range, 0, 0, 1.0)
        self.assertRaises(TypeError, yp_range, 0, 0.0, 1)
        self.assertRaises(TypeError, yp_range, 0, 0.0, 1.0)
        self.assertRaises(TypeError, yp_range, 0.0, 0, 1)
        self.assertRaises(TypeError, yp_range, 0.0, 0, 1.0)
        self.assertRaises(TypeError, yp_range, 0.0, 0.0, 1)
        self.assertRaises(TypeError, yp_range, 0.0, 0.0, 1.0)

    def test_index_1(self):
        u = yp_range(2)
        self.assertEqual(u.index(0), 0)
        self.assertEqual(u.index(1), 1)
        self.assertRaises(ValueError, u.index, 2)

        u = yp_range(-2, 3)
        self.assertEqual(u.count(0), 1)
        self.assertEqual(u.index(0), 2)
        self.assertRaises(TypeError, u.index)

        class BadExc(Exception):
            pass

    @yp_unittest.skip_user_defined_types
    def test_index_badcmp(self):
        class BadCmp:
            def __eq__(self, other):
                if other == 2:
                    raise BadExc()
                return False

        a = yp_range(4)
        self.assertRaises(BadExc, a.index, BadCmp())

    def test_index_2(self):
        a = yp_range(-2, 3)
        self.assertEqual(a.index(0), 2)
        self.assertEqual(yp_range(1, 10, 3).index(4), 1)
        self.assertEqual(yp_range(1, -10, -3).index(-5), 2)

    @yp_unittest.skip_long_ints
    def test_index_long_ints(self):
        self.assertEqual(yp_range(10**20).index(1), 1)
        self.assertEqual(yp_range(10**20).index(10**20 - 1), 10**20 - 1)

        self.assertRaises(ValueError, yp_range(1, 2**100, 2).index, 2**87)
        self.assertEqual(yp_range(1, 2**100, 2).index(2**87+1), 2**86)

    @yp_unittest.skip_user_defined_types
    def test_index_always_equal(self):
        self.assertEqual(range(10).index(ALWAYS_EQ), 0)

    @yp_unittest.skip_user_defined_types
    def test_user_index_method(self):
        bignum = 2*sys.maxsize
        smallnum = 42

        # User-defined class with an __index__ method
        class I:
            def __init__(self, n):
                self.n = int(n)
            def __index__(self):
                return self.n
        self.assertEqual(list(yp_range(I(bignum), I(bignum + 1))), [bignum])
        self.assertEqual(list(yp_range(I(smallnum), I(smallnum + 1))), [smallnum])

        # User-defined class with a failing __index__ method
        class IX:
            def __index__(self):
                raise RuntimeError
        self.assertRaises(RuntimeError, yp_range, IX())

        # User-defined class with an invalid __index__ method
        class IN:
            def __index__(self):
                return "not a number"

        self.assertRaises(TypeError, yp_range, IN())

        # Test use of user-defined classes in slice indices.
        self.assertEqual(yp_range(10)[:I(5)], yp_range(5))

        with self.assertRaises(RuntimeError):
            yp_range(0, 10)[:IX()]

        with self.assertRaises(TypeError):
            yp_range(0, 10)[:IN()]

    def test_count(self):
        self.assertEqual(yp_range(3).count(-1), 0)
        self.assertEqual(yp_range(3).count(0), 1)
        self.assertEqual(yp_range(3).count(1), 1)
        self.assertEqual(yp_range(3).count(2), 1)
        self.assertEqual(yp_range(3).count(3), 0)
        # Not applicable to nohtyP
        #self.assertIs(type(yp_range(3).count(-1)), int)
        #self.assertIs(type(yp_range(3).count(1)), int)

    @yp_unittest.skip_user_defined_types
    def test_count_always_equal(self):
        self.assertEqual(range(10).count(ALWAYS_EQ), 10)

        # ? Why is this here...
        self.assertEqual(len(yp_range(sys.maxsize, sys.maxsize+10)), 10)

    def test_repr(self):
        self.assertEqual(yp_repr(yp_range(1)), 'range(0, 1)')
        self.assertEqual(yp_repr(yp_range(1, 2)), 'range(1, 2)')
        # nohtyP normalizes start/end/step depending on the length of the range
        #self.assertEqual(yp_repr(yp_range(1, 2, 3)), 'range(1, 2, 3)')
        self.assertEqual(yp_repr(yp_range(1, 7, 3)), 'range(1, 7, 3)')

    @yp_unittest.skip_pickling
    def test_pickling(self):
        testcases = [(13,), (0, 11), (-22, 10), (20, 3, -1),
                     (13, 21, 3), (-2, 2, 2), (2**65, 2**65+2)]
        for proto in yp_range(pickle.HIGHEST_PROTOCOL + 1):
            for t in testcases:
                with self.subTest(proto=proto, test=t):
                    r = yp_range(*t)
                    self.assertEqual(list(pickle.loads(pickle.dumps(r, proto))),
                                     list(r))

    @yp_unittest.skip_pickling
    def test_iterator_pickling(self):
        testcases = [(13,), (0, 11), (-22, 10), (20, 3, -1), (13, 21, 3),
                     (-2, 2, 2)]
        for M in 2**31, 2**63:
            testcases += [
                (M-3, M-1), (4*M, 4*M+2),
                (M-2, M-1, 2), (-M+1, -M, -2),
                (1, 2, M-1), (-1, -2, -M),
                (1, M-1, M-1), (-1, -M, -M),
            ]
        for proto in range(pickle.HIGHEST_PROTOCOL + 1):
            for t in testcases:
                with self.subTest(proto=proto, t=t):
                    it = itorg = iter(range(*t))
                    data = list(range(*t))

                    d = pickle.dumps(it, proto)
                    it = pickle.loads(d)
                    self.assertEqual(type(itorg), type(it))
                    self.assertEqual(list(it), data)

                    it = pickle.loads(d)
                    try:
                        next(it)
                    except StopIteration:
                        continue
                    d = pickle.dumps(it, proto)
                    it = pickle.loads(d)
                    self.assertEqual(list(it), data[1:])

    @yp_unittest.skip_pickling
    def test_iterator_pickling_overflowing_index(self):
        for proto in range(pickle.HIGHEST_PROTOCOL + 1):
            with self.subTest(proto=proto):
                it = iter(range(2**32 + 2))
                _, _, idx = it.__reduce__()
                self.assertEqual(idx, 0)
                it.__setstate__(2**32 + 1)  # undocumented way to set r->index
                _, _, idx = it.__reduce__()
                self.assertEqual(idx, 2**32 + 1)
                d = pickle.dumps(it, proto)
                it = pickle.loads(d)
                self.assertEqual(next(it), 2**32 + 1)

    @yp_unittest.skip_pickling
    def test_exhausted_iterator_pickling(self):
        for proto in range(pickle.HIGHEST_PROTOCOL + 1):
            r = range(2**65, 2**65+2)
            i = iter(r)
            while True:
                r = next(i)
                if r == 2**65+1:
                    break
            d = pickle.dumps(i, proto)
            i2 = pickle.loads(d)
            self.assertEqual(list(i), [])
            self.assertEqual(list(i2), [])

    @yp_unittest.skip_pickling
    def test_large_exhausted_iterator_pickling(self):
        for proto in range(pickle.HIGHEST_PROTOCOL + 1):
            r = range(20)
            i = iter(r)
            while True:
                r = next(i)
                if r == 19:
                    break
            d = pickle.dumps(i, proto)
            i2 = pickle.loads(d)
            self.assertEqual(list(i), [])
            self.assertEqual(list(i2), [])

    def test_odd_bug(self):
        # This used to raise a "SystemError: NULL result without error"
        # because the range validation step was eating the exception
        # before NULL was returned.
        with self.assertRaises(TypeError):
            yp_range([], 1, -1)

    def test_types(self):
        # Non-integer objects *equal* to any of the range's items are supposed
        # to be contained in the range.
        self.assertIn(1.0, yp_range(3))
        self.assertIn(True, yp_range(3))

        self.assertIn(ALWAYS_EQ, range(3))

        # Objects are never coerced into other types for comparison.
        class C2:
            def __int__(self): return 1
            def __index__(self): return 1
        self.assertNotIn(C2(), yp_range(3))
        # ..except if explicitly told so.
        self.assertIn(int(C2()), yp_range(3))

        # Check that the yp_range.__contains__ optimization is only
        # used for ints, not for instances of subclasses of int.
        class C3(int):
            def __eq__(self, other): return True
        self.assertIn(C3(11), yp_range(10))
        self.assertIn(C3(11), list(yp_range(10)))

    def test_strided_limits(self):
        r = yp_range(0, 101, 2)
        self.assertIn(0, r)
        self.assertNotIn(1, r)
        self.assertIn(2, r)
        self.assertNotIn(99, r)
        self.assertIn(100, r)
        self.assertNotIn(101, r)

        r = yp_range(0, -20, -1)
        self.assertIn(0, r)
        self.assertIn(-1, r)
        self.assertIn(-19, r)
        self.assertNotIn(-20, r)

        r = yp_range(0, -20, -2)
        self.assertIn(-18, r)
        self.assertNotIn(-19, r)
        self.assertNotIn(-20, r)

    def test_empty(self):
        r = yp_range(0)
        self.assertNotIn(0, r)
        self.assertNotIn(1, r)

        r = yp_range(0, -10)
        self.assertNotIn(0, r)
        self.assertNotIn(-1, r)
        self.assertNotIn(1, r)

    @yp_test.support.requires_resource('cpu')
    def test_range_iterators(self):
        # exercise 'fast' iterators, that use a rangeiterobject internally.
        # see issue 7298
        limits = [base + jiggle
                  for M in (2**32, 2**64)
                  for base in (-M, -M//2, 0, M//2, M)
                  for jiggle in (-2, -1, 0, 1, 2)]
        test_ranges = [(start, end, step)
                       for start in limits
                       for end in limits
                       for step in (-2**63, -2**31, -2, -1, 1, 2)]

        for start, end, step in test_ranges:
            if step == yp_sys_minint or \
                    pyrange_calc_len(start, end, step) > ypObject_LEN_MAX:
                self.assertRaises((SystemError, OverflowError), yp_range, start, end, step)
                continue
            try: iter1 = yp_range(start, end, step)
            except OverflowError: continue
            iter2 = pyrange(start, end, step)
            test_id = "range({}, {}, {})".format(start, end, step)
            # check first 100 entries
            self.assert_iterators_equal(iter1, iter2, test_id, limit=100)

            iter1 = reversed(yp_range(start, end, step))
            iter2 = pyrange_reversed(start, end, step)
            test_id = "reversed(range({}, {}, {}))".format(start, end, step)
            self.assert_iterators_equal(iter1, iter2, test_id, limit=100)

    def test_range_iterators_invocation(self):
        # verify range iterators instances cannot be created by
        # calling their type
        rangeiter_type = type(iter(range(0)))
        self.assertRaises(TypeError, rangeiter_type, 1, 3, 1)
        long_rangeiter_type = type(iter(range(1 << 1000)))
        self.assertRaises(TypeError, long_rangeiter_type, 1, 3, 1)

    def test_slice(self):
        def check(start, stop, step=None):
            i = slice(start, stop, step)
            self.assertEqual(yp_list(r[i]), yp_list(r)[i])
            self.assertEqual(yp_len(r[i]), yp_len(yp_list(r)[i]))
        for r in [yp_range(10),
                  yp_range(0),
                  yp_range(1, 9, 3),
                  yp_range(8, 0, -3),
                  # nohtyP can't handle values that large
                  #yp_range(sys.maxsize+1, sys.maxsize+10),
                  yp_range(yp_sys_maxint-10, yp_sys_maxint),
                  ]:
            check(0, 2)
            check(0, 20)
            check(1, 2)
            check(20, 30)
            check(-30, -20)
            check(-1, 100, 2)
            check(0, -1)
            check(-1, -3, -1)

    def test_contains(self):
        r = yp_range(10)
        self.assertIn(0, r)
        self.assertIn(1, r)
        self.assertIn(5.0, r)
        self.assertNotIn(5.1, r)
        self.assertNotIn(-1, r)
        self.assertNotIn(10, r)
        self.assertNotIn("", r)
        r = yp_range(9, -1, -1)
        self.assertIn(0, r)
        self.assertIn(1, r)
        self.assertIn(5.0, r)
        self.assertNotIn(5.1, r)
        self.assertNotIn(-1, r)
        self.assertNotIn(10, r)
        self.assertNotIn("", r)
        r = yp_range(0, 10, 2)
        self.assertIn(0, r)
        self.assertNotIn(1, r)
        self.assertNotIn(5.0, r)
        self.assertNotIn(5.1, r)
        self.assertNotIn(-1, r)
        self.assertNotIn(10, r)
        self.assertNotIn("", r)
        r = yp_range(9, -1, -2)
        self.assertNotIn(0, r)
        self.assertIn(1, r)
        self.assertIn(5.0, r)
        self.assertNotIn(5.1, r)
        self.assertNotIn(-1, r)
        self.assertNotIn(10, r)
        self.assertNotIn("", r)

    def test_reverse_iteration(self):
        for r in [yp_range(10),
                  yp_range(0),
                  yp_range(1, 9, 3),
                  yp_range(8, 0, -3),
                  # nohtyP can't handle values that large
                  #yp_range(sys.maxsize+1, sys.maxsize+10),
                  yp_range(yp_sys_maxint-10, yp_sys_maxint),
                  ]:
            self.assertEqual(yp_list(yp_reversed(r)), yp_list(r)[::-1])

    def test_issue11845(self):
        r = yp_range(*slice(1, 18, 2).indices(20))
        values = {None, 0, 1, -1, 2, -2, 5, -5, 19, -19,
                  20, -20, 21, -21, 30, -30, 99, -99}
        for i in values:
            for j in values:
                for k in values - {0}:
                    r[i:j:k]

    def test_comparison(self):
        test_ranges = yp_list( [yp_range(0), yp_range(0, -1), yp_range(1, 1, 3),
                       yp_range(1), yp_range(5, 6), yp_range(5, 6, 2),
                       yp_range(5, 7, 2), yp_range(2), yp_range(0, 4, 2),
                       yp_range(0, 5, 2), yp_range(0, 6, 2)] )
        test_tuples = yp_list(map(yp_tuple, test_ranges))

        # Check that equality of ranges matches equality of the corresponding
        # tuples for each pair from the test lists above.
        ranges_eq = yp_list( a == b for a in test_ranges for b in test_ranges )
        tuples_eq = yp_list( a == b for a in test_tuples for b in test_tuples )
        self.assertSequenceEqual(ranges_eq, tuples_eq)

        # Check that != correctly gives the logical negation of ==
        ranges_ne = yp_list( a != b for a in test_ranges for b in test_ranges )
        self.assertSequenceEqual(ranges_ne, yp_list( not x for x in ranges_eq ))

        # Equal ranges should have equal hashes.
        for a in test_ranges:
            for b in test_ranges:
                if a == b:
                    self.assertEqual(yp_hash(a), yp_hash(b))

        # Ranges are unequal to other types (even sequence types)
        self.assertFalse(yp_range(0) == ())
        self.assertFalse(() == yp_range(0))
        self.assertFalse(yp_range(2) == [0, 1])

    @yp_unittest.skip_long_ints
    def test_comparison_long_ints(self):
        # Huge integers aren't a problem.
        self.assertEqual(yp_range(0, 2**100 - 1, 2),
                         yp_range(0, 2**100, 2))
        self.assertEqual(hash(yp_range(0, 2**100 - 1, 2)),
                         hash(yp_range(0, 2**100, 2)))
        self.assertNotEqual(yp_range(0, 2**100, 2),
                            yp_range(0, 2**100 + 1, 2))
        self.assertEqual(yp_range(2**200, 2**201 - 2**99, 2**100),
                         yp_range(2**200, 2**201, 2**100))
        self.assertEqual(hash(yp_range(2**200, 2**201 - 2**99, 2**100)),
                         hash(yp_range(2**200, 2**201, 2**100)))
        self.assertNotEqual(yp_range(2**200, 2**201, 2**100),
                            yp_range(2**200, 2**201 + 1, 2**100))

    def test_comparison_relative(self):
        # Order comparisons are not implemented for ranges.
        with self.assertRaises(TypeError):
            yp_range(0) < yp_range(0)
        with self.assertRaises(TypeError):
            yp_range(0) > yp_range(0)
        with self.assertRaises(TypeError):
            yp_range(0) <= yp_range(0)
        with self.assertRaises(TypeError):
            yp_range(0) >= yp_range(0)


    @yp_unittest.skip_range_attributes
    def test_attributes(self):
        # test the start, stop and step attributes of range objects
        self.assert_attrs(yp_range(0), 0, 0, 1)
        self.assert_attrs(yp_range(10), 0, 10, 1)
        self.assert_attrs(yp_range(-10), 0, -10, 1)
        self.assert_attrs(yp_range(0, 10, 1), 0, 10, 1)
        self.assert_attrs(yp_range(0, 10, 3), 0, 10, 3)
        self.assert_attrs(yp_range(10, 0, -1), 10, 0, -1)
        self.assert_attrs(yp_range(10, 0, -3), 10, 0, -3)
        self.assert_attrs(yp_range(True), 0, 1, 1)
        self.assert_attrs(yp_range(False, True), 0, 1, 1)
        self.assert_attrs(yp_range(False, True, True), 0, 1, 1)

    def assert_attrs(self, rangeobj, start, stop, step):
        self.assertEqual(rangeobj.start, start)
        self.assertEqual(rangeobj.stop, stop)
        self.assertEqual(rangeobj.step, step)
        self.assertIs(type(rangeobj.start), int)
        self.assertIs(type(rangeobj.stop), int)
        self.assertIs(type(rangeobj.step), int)

        with self.assertRaises(AttributeError):
            rangeobj.start = 0
        with self.assertRaises(AttributeError):
            rangeobj.stop = 10
        with self.assertRaises(AttributeError):
            rangeobj.step = 1

        with self.assertRaises(AttributeError):
            del rangeobj.start
        with self.assertRaises(AttributeError):
            del rangeobj.stop
        with self.assertRaises(AttributeError):
            del rangeobj.step

def test_main():
    test.support.run_unittest(RangeTest)

if __name__ == "__main__":
    yp_unittest.main()
