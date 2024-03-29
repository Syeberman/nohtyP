from yp import *
from python_test import support
import random
from python_test import yp_unittest
from functools import cmp_to_key

verbose = support.verbose
nerrors = 0


def check(tag, expected, raw, compare=None):
    global nerrors

    if verbose:
        print("    checking", tag)

    orig = raw[:]   # save input in case of error
    if compare:
        raw.sort(key=cmp_to_key(compare))
    else:
        raw.sort()

    if len(expected) != len(raw):
        print("error in", tag)
        print("length mismatch;", len(expected), len(raw))
        print(expected)
        print(orig)
        print(raw)
        nerrors += 1
        return

    for i, good in enumerate(expected):
        maybe = raw[i]
        if good is not maybe:
            print("error in", tag)
            print("out of order at index", i, good, maybe)
            print(expected)
            print(orig)
            print(raw)
            nerrors += 1
            return

class TestBase(yp_unittest.TestCase):
    def testStressfully(self):
        # Try a variety of sizes at and around powers of 2, and at powers of 10.
        sizes = [0]
        for power in yp_range(1, 10):
            n = 2 ** power
            sizes.extend(yp_range(n-1, n+2))
        sizes.extend([10, 100, 1000])

        class Complains(object):
            maybe_complain = True

            def __init__(self, i):
                self.i = i

            def __lt__(self, other):
                if Complains.maybe_complain and random.random() < 0.001:
                    if verbose:
                        print("        complaining at", self, other)
                    raise RuntimeError
                return self.i < other.i

            def __repr__(self):
                return "Complains(%d)" % self.i

        class Stable(object):
            def __init__(self, key, i):
                self.key = key
                self.index = i

            def __lt__(self, other):
                return self.key < other.key

            def __repr__(self):
                return "Stable(%d, %d)" % (self.key, self.index)

        for n in sizes:
            x = yp_list(yp_range(n))
            if verbose:
                print("Testing size", n)

            s = x[:]
            check("identity", x, s)

            s = x[:]
            s.reverse()
            check("reversed", x, s)

            s = x[:]
            random.shuffle(s)
            check("random permutation", x, s)

            # TODO(skip_user_defined_types) nohtyP lists don't store user-defined types (cmp_to_key)
            # y = x[:]
            # y.reverse()
            # s = x[:]
            # check("reversed via function", y, s, lambda a, b: (b>a)-(b<a))

            # TODO(skip_user_defined_types) nohtyP lists don't store user-defined types (cmp_to_key)
            # if verbose:
            #     print("    Checking against an insane comparison function.")
            #     print("        If the implementation isn't careful, this may segfault.")
            # s = x[:]
            # s.sort(key=cmp_to_key(lambda a, b:  yp_int(random.random() * 3) - 1))
            # check("an insane function left some permutation", x, s)

            # TODO(skip_user_defined_types) nohtyP lists don't store user-defined types (bad_key)
            # if len(x) >= 2:
            #     def bad_key(x):
            #         raise RuntimeError
            #     s = x[:]
            #     self.assertRaises(RuntimeError, s.sort, key=bad_key)

            # TODO(skip_user_defined_types) nohtyP lists don't store user-defined types (Complains)
            # x = yp_list(Complains(i) for i in x)
            # s = x[:]
            # random.shuffle(s)
            # Complains.maybe_complain = True
            # it_complained = False
            # try:
            #     s.sort()
            # except RuntimeError:
            #     it_complained = True
            # if it_complained:
            #     Complains.maybe_complain = False
            #     check("exception during sort left some permutation", x, s)

            # TODO(skip_user_defined_types) nohtyP lists don't store user-defined types (Stable)
            # s = yp_list(Stable(random.randrange(10), i) for i in yp_range(n))
            # augmented = yp_list((e, e.index) for e in s)
            # augmented.sort()    # forced stable because ties broken by index
            # x = yp_list(e for e, i in augmented) # a stable sort of s
            # check("stability", x, s)

#==============================================================================

class TestBugs(yp_unittest.TestCase):

    @yp_unittest.skip_user_defined_types
    def test_bug453523(self):
        # bug 453523 -- list.sort() crasher.
        # If this fails, the most likely outcome is a core dump.
        # Mutations during a list sort should raise a ValueError.

        class C:
            def __lt__(self, other):
                if L and random.random() < 0.75:
                    L.pop()
                else:
                    L.append(3)
                return random.random() < 0.5

        L = yp_list(C() for i in yp_range(50))
        self.assertRaises(ValueError, L.sort)

    @yp_unittest.skip_user_defined_types
    def test_undetected_mutation(self):
        # Python 2.4a1 did not always detect mutation
        memorywaster = yp_list()
        for i in yp_range(20):
            def mutating_cmp(x, y):
                L.append(3)
                L.pop()
                return (x > y) - (x < y)
            L = yp_list((1,2))
            self.assertRaises(ValueError, L.sort, key=cmp_to_key(mutating_cmp))
            def mutating_cmp(x, y):
                L.append(3)
                del L[:]
                return (x > y) - (x < y)
            self.assertRaises(ValueError, L.sort, key=cmp_to_key(mutating_cmp))
            memorywaster = yp_list((memorywaster, ))

#==============================================================================

class TestDecorateSortUndecorate(yp_unittest.TestCase):

    @yp_unittest.skip_user_defined_types
    def test_decorated(self):
        data = 'The quick Brown fox Jumped over The lazy Dog'.split()
        copy = data[:]
        random.shuffle(data)
        data.sort(key=yp_str.lower)
        def my_cmp(x, y):
            xlower, ylower = x.lower(), y.lower()
            return (xlower > ylower) - (xlower < ylower)
        copy.sort(key=cmp_to_key(my_cmp))

    def test_baddecorator(self):
        data = 'The quick Brown fox Jumped over The lazy Dog'.split()
        self.assertRaises(TypeError, data.sort, key=lambda x,y: 0)

    def test_stability(self):
        data = yp_list((random.randrange(100), i) for i in yp_range(200))
        copy = data[:]
        data.sort(key=lambda t: t[0])   # sort on the random first field
        copy.sort()                     # sort using both fields
        self.assertEqual(data, copy)    # should get the same result

    def test_key_with_exception(self):
        # Verify that the wrapper has been removed
        data = yp_list(yp_range(-2, 2))
        dup = data[:]
        self.assertRaises(ZeroDivisionError, data.sort, key=lambda x: 1/x)
        self.assertEqual(data, dup)

    def test_key_with_mutation(self):
        data = yp_list(yp_range(10))
        def k(x):
            del data[:]
            data[:] = yp_range(20)
            return x
        self.assertRaises(ValueError, data.sort, key=k)

    @yp_unittest.skip_user_defined_types
    def test_key_with_mutating_del(self):
        data = yp_list(yp_range(10))
        class SortKiller(object):
            def __init__(self, x):
                pass
            def __del__(self):
                del data[:]
                data[:] = yp_range(20)
            def __lt__(self, other):
                return id(self) < id(other)
        self.assertRaises(ValueError, data.sort, key=SortKiller)

    def test_key_with_mutating_del_and_exception(self):
        data = yp_list(yp_range(10))
        ## dup = data[:]
        class SortKiller(object):
            def __init__(self, x):
                if x > 2:
                    raise RuntimeError
            def __del__(self):
                del data[:]
                data[:] = yp_list(yp_range(20))
        self.assertRaises(RuntimeError, data.sort, key=SortKiller)
        ## major honking subtlety: we *can't* do:
        ##
        ## self.assertEqual(data, dup)
        ##
        ## because there is a reference to a SortKiller in the
        ## traceback and by the time it dies we're outside the call to
        ## .sort() and so the list protection gimmicks are out of
        ## date (this cost some brain cells to figure out...).

    def test_reverse(self):
        data = yp_list(yp_range(100))
        random.shuffle(data)
        data.sort(reverse=True)
        self.assertEqual(data, yp_list(yp_range(99,-1,-1)))

    @yp_unittest.skip_user_defined_types
    def test_reverse_stability(self):
        data = yp_list((random.randrange(100), i) for i in yp_range(200))
        copy1 = data[:]
        copy2 = data[:]
        def my_cmp(x, y):
            x0, y0 = x[0], y[0]
            return (x0 > y0) - (x0 < y0)
        def my_cmp_reversed(x, y):
            x0, y0 = x[0], y[0]
            return (y0 > x0) - (y0 < x0)
        data.sort(key=cmp_to_key(my_cmp), reverse=True)
        copy1.sort(key=cmp_to_key(my_cmp_reversed))
        self.assertEqual(data, copy1)
        copy2.sort(key=lambda x: x[0], reverse=True)
        self.assertEqual(data, copy2)

#==============================================================================
def check_against_PyObject_RichCompareBool(self, L):
    ## The idea here is to exploit the fact that unsafe_tuple_compare uses
    ## PyObject_RichCompareBool for the second elements of tuples. So we have,
    ## for (most) L, sorted(L) == [y[1] for y in sorted([(0,x) for x in L])]
    ## This will work as long as __eq__ => not __lt__ for all the objects in L,
    ## which holds for all the types used below.
    ##
    ## Testing this way ensures that the optimized implementation remains consistent
    ## with the naive implementation, even if changes are made to any of the
    ## richcompares.
    ##
    ## This function tests sorting for three lists (it randomly shuffles each one):
    ##                        1. L
    ##                        2. [(x,) for x in L]
    ##                        3. [((x,),) for x in L]

    random.seed(0)
    random.shuffle(L)
    L_1 = L[:]
    L_2 = yp_list((x,) for x in L)
    L_3 = yp_list(((x,),) for x in L)
    for L in [L_1, L_2, L_3]:
        optimized = yp_sorted(L)
        reference = yp_list(y[1] for y in yp_sorted(yp_list((0,x) for x in L)))
        for (opt, ref) in zip(optimized, reference):
            self.assertIs(opt, ref)
            #note: not assertEqual! We want to ensure *identical* behavior.

class TestOptimizedCompares(yp_unittest.TestCase):
    def test_safe_object_compare(self):
        heterogeneous_lists = [yp_list([0, 'foo']),
                               yp_list([0.0, 'foo']),
                               yp_list([('foo',), 'foo'])]
        for L in heterogeneous_lists:
            self.assertRaises(TypeError, L.sort)
            self.assertRaises(TypeError, yp_list((x,) for x in L).sort)
            self.assertRaises(TypeError, yp_list(((x,),) for x in L).sort)

        float_int_lists = [yp_list([1,1.1]),
                           yp_list([1<<62,1.1]), # TODO(skip_long_ints)
                           yp_list([1.1,1]),
                           yp_list([1.1,1<<62])] # TODO(skip_long_ints)
        for L in float_int_lists:
            check_against_PyObject_RichCompareBool(self, L)

    @yp_unittest.skip_user_defined_types
    def test_unsafe_object_compare(self):

        # This test is by ppperry. It ensures that unsafe_object_compare is
        # verifying ms->key_richcompare == tp->richcompare before comparing.

        class WackyComparator(yp_int):
            def __lt__(self, other):
                elem.__class__ = WackyList2
                return yp_int.__lt__(self, other)

        class WackyList1(yp_list):
            pass

        class WackyList2(yp_list):
            def __lt__(self, other):
                raise ValueError

        L = [WackyList1([WackyComparator(i), i]) for i in yp_range(10)]
        elem = L[-1]
        with self.assertRaises(ValueError):
            L.sort()

        L = [WackyList1([WackyComparator(i), i]) for i in yp_range(10)]
        elem = L[-1]
        with self.assertRaises(ValueError):
            [(x,) for x in L].sort()

        # The following test is also by ppperry. It ensures that
        # unsafe_object_compare handles Py_NotImplemented appropriately.
        class PointlessComparator:
            def __lt__(self, other):
                return NotImplemented
        L = [PointlessComparator(), PointlessComparator()]
        self.assertRaises(TypeError, L.sort)
        self.assertRaises(TypeError, [(x,) for x in L].sort)

        # The following tests go through various types that would trigger
        # ms->key_compare = unsafe_object_compare
        lists = [yp_list(yp_range(100)) + [(1<<70)],
                 [yp_str(x) for x in yp_range(100)] + ['\uffff'],
                 [yp_bytes(x) for x in yp_range(100)],
                 [cmp_to_key(lambda x,y: x<y)(x) for x in yp_range(100)]]
        for L in lists:
            check_against_PyObject_RichCompareBool(self, L)

    def test_unsafe_latin_compare(self):
        check_against_PyObject_RichCompareBool(self, yp_list(yp_str(x) for
                                                      x in yp_range(100)))

    def test_unsafe_long_compare(self):
        check_against_PyObject_RichCompareBool(self, yp_list(x for
                                                      x in yp_range(100)))

    def test_unsafe_float_compare(self):
        check_against_PyObject_RichCompareBool(self, yp_list(yp_float(x) for
                                                      x in yp_range(100)))

    @yp_unittest.skip_floats
    def test_unsafe_tuple_compare(self):
        # This test was suggested by Tim Peters. It verifies that the tuple
        # comparison respects the current tuple compare semantics, which do not
        # guarantee that x < x <=> (x,) < (x,)
        #
        # Note that we don't have to put anything in tuples here, because
        # the check function does a tuple test automatically.

        check_against_PyObject_RichCompareBool(self, yp_list([float('nan')])*100)
        check_against_PyObject_RichCompareBool(self, yp_list(float('nan') for
                                                      _ in yp_range(100)))

    def test_not_all_tuples(self):
        self.assertRaises(TypeError, [(1.0, 1.0), (False, "A"), 6].sort)
        self.assertRaises(TypeError, [('a', 1), (1, 'a')].sort)
        self.assertRaises(TypeError, [(1, 'a'), ('a', 1)].sort)
#==============================================================================

if __name__ == "__main__":
    yp_unittest.main()
