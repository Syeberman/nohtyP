from yp import *
from yp_test import support, seq_tests

import gc
import pickle
from yp_test import yp_unittest

# Extra assurance that we're not accidentally testing Python's tuple and list
def tuple( *args, **kwargs ): raise NotImplementedError( "convert script to yp_tuple here" )
_list = list # ...because we actually need Python's list in a few places
def list( *args, **kwargs ): raise NotImplementedError( "convert script to yp_list here" )


class TupleTest(seq_tests.CommonTest):
    type2test = yp_tuple

    def test_constructors(self):
        super().test_constructors()
        # calling built-in types without argument must return empty
        self.assertIs(yp_tuple(), yp_tuple())
        self.assertEqual(yp_len(yp_tuple()), 0)
        t0_3 = yp_tuple((0, 1, 2, 3))
        t0_3_bis = yp_tuple(t0_3)
        self.assertIs(t0_3, t0_3_bis)
        self.assertEqual(yp_tuple([]), yp_tuple())
        self.assertEqual(yp_tuple([0, 1, 2, 3]), yp_tuple((0, 1, 2, 3)))
        self.assertEqual(yp_tuple(''), yp_tuple())
        self.assertEqual(yp_tuple('spam'), yp_tuple(('s', 'p', 'a', 'm')))

    def test_truth(self):
        super().test_truth()
        self.assertFalse(yp_tuple())
        self.assertTrue(yp_tuple((42, )))

    def test_len(self):
        super().test_len()
        self.assertEqual(yp_len(yp_tuple()), 0)
        self.assertEqual(yp_len(yp_tuple((0,))), 1)
        self.assertEqual(yp_len(yp_tuple((0, 1, 2))), 3)

    def test_iadd(self):
        super().test_iadd()
        u = yp_tuple((0, 1))
        u2 = u
        u += yp_tuple((2, 3))
        self.assertIsNot(u, u2)

    def test_imul(self):
        super().test_imul()
        u = yp_tuple((0, 1))
        u2 = u
        u *= 3
        self.assertIsNot(u, u2)

    def test_tupleresizebug(self):
        # Check that a specific bug in _PyTuple_Resize() is squashed.
        def f():
            for i in range(1000):
                yield i
        self.assertEqual(yp_list(yp_tuple(f())), yp_list(range(1000)))

    @yp_unittest.skip("TODO re-enable (it just takes a long time)")
    def test_hash(self):
        # See SF bug 942952:  Weakness in tuple hash
        # The hash should:
        #      be non-commutative
        #      should spread-out closely spaced values
        #      should not exhibit cancellation in tuples like (x,(x,y))
        #      should be distinct from element hashes:  hash(x)!=hash((x,))
        # This test exercises those cases.
        # For a pure random hash and N=50, the expected number of occupied
        #      buckets when tossing 252,600 balls into 2**32 buckets
        #      is 252,592.6, or about 7.4 expected collisions.  The
        #      standard deviation is 2.73.  On a box with 64-bit hash
        #      codes, no collisions are expected.  Here we accept no
        #      more than 15 collisions.  Any worse and the hash function
        #      is sorely suspect.

        N=50
        base = _list(yp_int(x) for x in range(N))
        xp = [yp_tuple((i, j)) for i in base for j in base]
        inps = base + [yp_tuple((i, j)) for i in base for j in xp] + \
                     [yp_tuple((i, j)) for i in xp for j in base] + xp + _list(zip(base))
        collisions = len(inps) - len(set(map(hash, inps)))
        self.assertTrue(collisions <= 15)

    def test_repr(self):
        l0 = yp_tuple()
        l2 = yp_tuple((0, 1, 2))
        a0 = self.type2test(l0)
        a2 = self.type2test(l2)

        self.assertEqual(yp_str(a0), yp_repr(l0))
        self.assertEqual(yp_str(a2), yp_repr(l2))
        self.assertEqual(yp_repr(a0), "()")
        self.assertEqual(yp_repr(a2), "(0, 1, 2)")

    def _not_tracked(self, t):
        # Nested tuples can take several collections to untrack
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
        # Test GC-optimization of tuple literals
        x, y, z = 1.5, "a", []

        self._not_tracked(yp_tuple())
        self._not_tracked(yp_tuple((1,)))
        self._not_tracked(yp_tuple((1, 2)))
        self._not_tracked(yp_tuple((1, 2, "a")))
        self._not_tracked(yp_tuple((1, 2, yp_tuple(None, True, False, yp_tuple()), int)))
        self._not_tracked(yp_tuple((object(),)))
        self._not_tracked(yp_tuple((yp_tuple((1, x)), y, yp_tuple((2, 3)))))

        # Tuples with mutable elements are always tracked, even if those
        # elements are not tracked right now.
        self._tracked(yp_tuple(([],)))
        self._tracked(yp_tuple(([1],)))
        self._tracked(yp_tuple(({},)))
        self._tracked(yp_tuple((set(),)))
        self._tracked(yp_tuple((x, y, z)))

    @yp_unittest.skip("Not applicable to nohtyP")
    def check_track_dynamic(self, tp, always_track):
        x, y, z = 1.5, "a", []

        check = self._tracked if always_track else self._not_tracked
        check(tp())
        check(tp([]))
        check(tp(set()))
        check(tp([1, x, y]))
        check(tp(obj for obj in [1, x, y]))
        check(tp(set([1, x, y])))
        check(tp(yp_tuple([obj]) for obj in [1, x, y]))
        check(yp_tuple(tp([obj]) for obj in [1, x, y]))

        self._tracked(tp([z]))
        self._tracked(tp([[x, y]]))
        self._tracked(tp([{x: y}]))
        self._tracked(tp(obj for obj in [x, y, z]))
        self._tracked(tp(yp_tuple([obj]) for obj in [x, y, z]))
        self._tracked(yp_tuple(tp([obj]) for obj in [x, y, z]))

    @yp_unittest.skip("Not applicable to nohtyP")
    @support.cpython_only
    def test_track_dynamic(self):
        # Test GC-optimization of dynamically constructed tuples.
        self.check_track_dynamic(yp_tuple, False)

    @yp_unittest.skip("Not applicable to nohtyP")
    @support.cpython_only
    def test_track_subtypes(self):
        # Tuple subtypes must always be tracked
        class MyTuple(yp_tuple):
            pass
        self.check_track_dynamic(MyTuple, True)

    @yp_unittest.skip("Not applicable to nohtyP")
    @support.cpython_only
    def test_bug7466(self):
        # Trying to untrack an unfinished tuple could crash Python
        self._not_tracked(yp_tuple(gc.collect() for i in range(101)))

    @yp_unittest.skip("TODO re-enable (it just takes a long time)")
    def test_repr_large(self):
        # Check the repr of large list objects
        def check(n):
            l = yp_tuple((0,)) * n
            s = yp_repr(l)
            self.assertEqual(s,
                '(' + ', '.join(['0'] * n) + ')')
        check(10)       # check our checking code
        check(1000000)

    @yp_unittest.skip("TODO: Implement nohtyP pickling")
    def test_iterator_pickle(self):
        # Userlist iterators don't support pickling yet since
        # they are based on generators.
        data = self.type2test([4, 5, 6, 7])
        itorg = iter(data)
        d = pickle.dumps(itorg)
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
        itorg = reversed(data)
        d = pickle.dumps(itorg)
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
        class T(yp_tuple): pass
        with self.assertRaises(TypeError):
            [3,] + T((1,2))

def test_main():
    support.run_unittest(TupleTest)

if __name__=="__main__":
    test_main()
