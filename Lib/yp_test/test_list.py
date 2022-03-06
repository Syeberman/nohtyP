from yp import *
import sys
from yp_test import list_tests
from yp_test.support import cpython_only
import pickle
from yp_test import yp_unittest

# Extra assurance that we're not accidentally testing Python's tuple and list
def tuple(*args, **kwargs): raise NotImplementedError("convert script to yp_tuple here")
def list(*args, **kwargs): raise NotImplementedError("convert script to yp_list here")

class ListTest(list_tests.CommonTest):
    type2test = yp_list

    def test_basic(self):
        self.assertEqual(yp_list([]), yp_list())
        self.assertEqual(yp_len(yp_list()), 0)
        self.assertEqual(yp_len(yp_list([])), 0)
        l0_3 = yp_list([0, 1, 2, 3])
        l0_3_bis = yp_list(l0_3)
        self.assertEqual(l0_3, l0_3_bis)
        self.assertIsNot(l0_3, l0_3_bis)
        self.assertEqual(yp_list(()), yp_list())
        self.assertEqual(yp_list((0, 1, 2, 3)), yp_list([0, 1, 2, 3]))
        self.assertEqual(yp_list(''), yp_list())
        self.assertEqual(yp_list('spam'), yp_list(['s', 'p', 'a', 'm']))
        self.assertEqual(yp_list(x for x in yp_range(10) if x % 2),
                         yp_list([1, 3, 5, 7, 9]))

        if sys.maxsize == 0x7fffffff:
            # This test can currently only work on 32-bit machines.
            # XXX If/when PySequence_Length() returns a ssize_t, it should be
            # XXX re-enabled.
            # Verify clearing of bug #556025.
            # This assumes that the max data size (sys.maxint) == max
            # address size this also assumes that the address size is at
            # least 4 bytes with 8 byte addresses, the bug is not well
            # tested
            #
            # Note: This test is expected to SEGV under Cygwin 1.3.12 or
            # earlier due to a newlib bug.  See the following mailing list
            # thread for the details:

            #     http://sources.redhat.com/ml/newlib/2002/msg00369.html
            self.assertRaises(MemoryError, yp_list, yp_range(sys.maxsize // 2))

        # This code used to segfault in Py2.4a3
        x = yp_list()
        x.extend(-y for y in x)
        self.assertEqual(x, yp_list())

    def test_keyword_args(self):
        with self.assertRaisesRegex(TypeError, 'keyword argument'):
            yp_list(sequence=[])

    def test_truth(self):
        super().test_truth()
        self.assertFalse(yp_list())
        self.assertTrue(yp_list([42]))

    def test_identity(self):
        self.assertIsNot(yp_list(), yp_list())

    def test_len(self):
        super().test_len()
        self.assertEqual(yp_len(yp_list()), 0)
        self.assertEqual(yp_len(yp_list([0])), 1)
        self.assertEqual(yp_len(yp_list([0, 1, 2])), 3)

    def test_overflow(self):
        lst = yp_list([4, 5, 6, 7])
        n = int((sys.maxsize*2+2) // len(lst))
        def mul(a, b): return a * b
        def imul(a, b): a *= b
        self.assertRaises((MemoryError, OverflowError), mul, lst, n)
        self.assertRaises((MemoryError, OverflowError), imul, lst, n)

    @yp_unittest.skip_str_repr
    def test_repr_large(self):
        # Check the repr of large list objects
        def check(n):
            l = yp_list([0]) * n
            s = yp_repr(l)
            self.assertEqual(s,
                '[' + ', '.join(['0'] * n) + ']')
        check(10)       # check our checking code
        check(1000000)

    @yp_unittest.skip_pickling
    def test_iterator_pickle(self):
        orig = self.type2test([4, 5, 6, 7])
        data = [10, 11, 12, 13, 14, 15]
        for proto in range(pickle.HIGHEST_PROTOCOL + 1):
            # initial iterator
            itorig = iter(orig)
            d = pickle.dumps((itorig, orig), proto)
            it, a = pickle.loads(d)
            a[:] = data
            self.assertEqual(type(it), type(itorig))
            self.assertEqual(yp_list(it), data)

            # running iterator
            next(itorig)
            d = pickle.dumps((itorig, orig), proto)
            it, a = pickle.loads(d)
            a[:] = data
            self.assertEqual(type(it), type(itorig))
            self.assertEqual(yp_list(it), data[1:])

            # empty iterator
            for i in range(1, len(orig)):
                next(itorig)
            d = pickle.dumps((itorig, orig), proto)
            it, a = pickle.loads(d)
            a[:] = data
            self.assertEqual(type(it), type(itorig))
            self.assertEqual(yp_list(it), data[len(orig):])

            # exhausted iterator
            self.assertRaises(StopIteration, next, itorig)
            d = pickle.dumps((itorig, orig), proto)
            it, a = pickle.loads(d)
            a[:] = data
            self.assertEqual(yp_list(it), [])

    @yp_unittest.skip_pickling
    def test_reversed_pickle(self):
        orig = self.type2test([4, 5, 6, 7])
        data = [10, 11, 12, 13, 14, 15]
        for proto in range(pickle.HIGHEST_PROTOCOL + 1):
            # initial iterator
            itorig = reversed(orig)
            d = pickle.dumps((itorig, orig), proto)
            it, a = pickle.loads(d)
            a[:] = data
            self.assertEqual(type(it), type(itorig))
            self.assertEqual(yp_list(it), data[len(orig)-1::-1])

            # running iterator
            next(itorig)
            d = pickle.dumps((itorig, orig), proto)
            it, a = pickle.loads(d)
            a[:] = data
            self.assertEqual(type(it), type(itorig))
            self.assertEqual(yp_list(it), data[len(orig)-2::-1])

            # empty iterator
            for i in range(1, len(orig)):
                next(itorig)
            d = pickle.dumps((itorig, orig), proto)
            it, a = pickle.loads(d)
            a[:] = data
            self.assertEqual(type(it), type(itorig))
            self.assertEqual(yp_list(it), [])

            # exhausted iterator
            self.assertRaises(StopIteration, next, itorig)
            d = pickle.dumps((itorig, orig), proto)
            it, a = pickle.loads(d)
            a[:] = data
            self.assertEqual(yp_list(it), [])

    def test_step_overflow(self):
        a = yp_list([0, 1, 2, 3, 4])
        a[1::sys.maxsize] = [0]
        self.assertEqual(a[3::sys.maxsize], [3])

    def test_no_comdat_folding(self):
        # Issue 8847: In the PGO build, the MSVC linker's COMDAT folding
        # optimization causes failures in code that relies on distinct
        # function addresses.
        class L(yp_list): pass
        with self.assertRaises(TypeError):
            (3,) + L([1,2])

    @yp_unittest.skip_user_defined_types
    def test_equal_operator_modifying_operand(self):
        # test fix for seg fault reported in bpo-38588 part 2.
        class X:
            def __eq__(self,other) :
                list2.clear()
                return NotImplemented

        class Y:
            def __eq__(self, other):
                list1.clear()
                return NotImplemented

        class Z:
            def __eq__(self, other):
                list3.clear()
                return NotImplemented

        list1 = [X()]
        list2 = [Y()]
        self.assertTrue(list1 == list2)

        list3 = [Z()]
        list4 = [1]
        self.assertFalse(list3 == list4)

    @yp_unittest.skip_sys_getsizeof
    @cpython_only
    def test_preallocation(self):
        iterable = [0] * 10
        iter_size = sys.getsizeof(iterable)

        self.assertEqual(iter_size, sys.getsizeof(yp_list([0] * 10)))
        self.assertEqual(iter_size, sys.getsizeof(yp_list(yp_range(10))))

    @yp_unittest.skip_user_defined_types
    def test_count_index_remove_crashes(self):
        # bpo-38610: The count(), index(), and remove() methods were not
        # holding strong references to list elements while calling
        # PyObject_RichCompareBool().
        class X:
            def __eq__(self, other):
                lst.clear()
                return NotImplemented

        lst = [X()]
        with self.assertRaises(ValueError):
            lst.index(lst)

        class L(yp_list):
            def __eq__(self, other):
                str(other)
                return NotImplemented

        lst = L([X()])
        lst.count(lst)

        lst = L([X()])
        with self.assertRaises(ValueError):
            lst.remove(lst)

        # bpo-39453: list.__contains__ was not holding strong references
        # to list elements while calling PyObject_RichCompareBool().
        lst = [X(), X()]
        3 in lst
        lst = [X(), X()]
        X() in lst


if __name__ == "__main__":
    yp_unittest.main()
