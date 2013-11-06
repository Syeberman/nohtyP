from yp import *
import sys
from yp_test import support, list_tests
import pickle
from yp_test import yp_unittest

# Extra assurance that we're not accidentally testing Python's tuple and list
def tuple( *args, **kwargs ): raise NotImplementedError( "convert script to yp_tuple here" )
def list( *args, **kwargs ): raise NotImplementedError( "convert script to yp_list here" )


class ListTest(list_tests.CommonTest):
    type2test = yp_list

    def test_basic(self):
        self.assertEqual(yp_list([]), yp_list())
        self.assertEqual(len(yp_list()), 0)
        self.assertEqual(len(yp_list([])), 0)
        l0_3 = yp_list([0, 1, 2, 3])
        l0_3_bis = yp_list(l0_3)
        self.assertEqual(l0_3, l0_3_bis)
        self.assertTrue(l0_3 is not l0_3_bis)
        self.assertEqual(yp_list(()), yp_list())
        self.assertEqual(yp_list((0, 1, 2, 3)), yp_list([0, 1, 2, 3]))
        self.assertEqual(yp_list(''), yp_list())
        self.assertEqual(yp_list('spam'), yp_list(['s', 'p', 'a', 'm']))

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
            self.assertRaises(MemoryError, yp_list, range(sys.maxsize // 2))

        # This code used to segfault in Py2.4a3
        x = yp_list()
        x.extend(-y for y in x)
        self.assertEqual(x, yp_list())

    def test_truth(self):
        super().test_truth()
        self.assertTrue(not yp_list())
        self.assertTrue(yp_list([42]))

    def test_identity(self):
        self.assertTrue(yp_list() is not yp_list())

    def test_len(self):
        super().test_len()
        self.assertEqual(len(yp_list()), 0)
        self.assertEqual(len(yp_list([0])), 1)
        self.assertEqual(len(yp_list([0, 1, 2])), 3)

    def test_overflow(self):
        lst = yp_list([4, 5, 6, 7])
        n = int((sys.maxsize*2+2) // len(lst))
        def mul(a, b): return a * b
        def imul(a, b): a *= b
        self.assertRaises((MemoryError, OverflowError), mul, lst, n)
        self.assertRaises((MemoryError, OverflowError), imul, lst, n)

    @yp_unittest.skip("TODO re-enable (it just takes a long time)")
    def test_repr_large(self):
        # Check the repr of large list objects
        def check(n):
            l = yp_list([0]) * n
            s = repr(l)
            self.assertEqual(s,
                '[' + ', '.join(['0'] * n) + ']')
        check(10)       # check our checking code
        check(1000000)

    @yp_unittest.skip("TODO: Implement nohtyP pickling")
    def test_iterator_pickle(self):
        # Userlist iterators don't support pickling yet since
        # they are based on generators.
        data = self.type2test([4, 5, 6, 7])
        it = itorg = iter(data)
        d = pickle.dumps(it)
        it = pickle.loads(d)
        self.assertEqual(type(itorg), type(it))
        self.assertEqual(self.type2test(it), self.type2test(data))

        it = pickle.loads(d)
        next(it)
        d = pickle.dumps(it)
        self.assertEqual(self.type2test(it), self.type2test(data)[1:])

    @yp_unittest.skip("TODO: Implement nohtyP pickling")
    def test_reversed_pickle(self):
        data = self.type2test([4, 5, 6, 7])
        it = itorg = reversed(data)
        d = pickle.dumps(it)
        it = pickle.loads(d)
        self.assertEqual(type(itorg), type(it))
        self.assertEqual(self.type2test(it), self.type2test(reversed(data)))

        it = pickle.loads(d)
        next(it)
        d = pickle.dumps(it)
        self.assertEqual(self.type2test(it), self.type2test(reversed(data))[1:])

    def test_no_comdat_folding(self):
        # Issue 8847: In the PGO build, the MSVC linker's COMDAT folding
        # optimization causes failures in code that relies on distinct
        # function addresses.
        class L(yp_list): pass
        with self.assertRaises(TypeError):
            (3,) + L([1,2])

def test_main(verbose=None):
    support.run_unittest(ListTest)

    # verify reference counting
    import sys
    if verbose and hasattr(sys, "gettotalrefcount"):
        import gc
        counts = [None] * 5
        for i in range(len(counts)):
            support.run_unittest(ListTest)
            gc.collect()
            counts[i] = sys.gettotalrefcount()
        print(counts)


if __name__ == "__main__":
    test_main(verbose=True)
