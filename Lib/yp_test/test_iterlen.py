""" Test Iterator Length Transparency

Some functions or methods which accept general iterable arguments have
optional, more efficient code paths if they know how many items to expect.
For instance, map(func, iterable), will pre-allocate the exact amount of
space required whenever the iterable can report its length.

The desired invariant is:  len(it)==len(list(it)).

A complication is that an iterable and iterator can be the same object. To
maintain the invariant, an iterator needs to dynamically update its length.
For instance, an iterable such as range(10) always reports its length as ten,
but it=iter(range(10)) starts at ten, and then goes to nine after next(it).
Having this capability means that map() can ignore the distinction between
map(func, iterable) and map(func, iter(iterable)).

When the iterable is immutable, the implementation can straight-forwardly
report the original length minus the cumulative number of calls to next().
This is the case for tuples, range objects, and itertools.repeat().

Some containers become temporarily immutable during iteration.  This includes
dicts, sets, and collections.deque.  Their implementation is equally simple
though they need to permanently set their length to zero whenever there is
an attempt to iterate after a length mutation.

The situation slightly more involved whenever an object allows length mutation
during iteration.  Lists and sequence iterators are dynamically updatable.
So, if a list is extended during iteration, the iterator will continue through
the new items.  If it shrinks to a point before the most recent iteration,
then no further items are available and the length is reported at zero.

Reversed objects can also be wrapped around mutable objects; however, any
appends after the current position are ignored.  Any other approach leads
to confusion and possibly returning the same item more than once.

The iterators not listed above, such as enumerate and the other itertools,
are not length transparent because they have no way to distinguish between
iterables that report static length and iterators whose length changes with
each call (i.e. the difference between enumerate('abc') and
enumerate(iter('abc')).

"""

from yp import *
from yp_test import yp_unittest
from itertools import repeat
from collections import deque
from operator import length_hint as _length_hint

def length_hint(obj, default=0):
    """Returns a yp_int instead of int, and ensures obj is from nohtyP"""
    assert isinstance(obj, ypObject)
    return yp_int(_length_hint(obj, default))

# Extra assurance that we're not accidentally testing Python's data types
def iter( *args, **kwargs ): raise NotImplementedError( "convert script to yp_iter here" )
def bytes( *args, **kwargs ): raise NotImplementedError( "convert script to yp_bytes here" )
def bytearray( *args, **kwargs ): raise NotImplementedError( "convert script to yp_bytearray here" )
def str( *args, **kwargs ): raise NotImplementedError( "convert script to yp_str here" )
def tuple( *args, **kwargs ): raise NotImplementedError( "convert script to yp_tuple here" )
def list( *args, **kwargs ): raise NotImplementedError( "convert script to yp_list here" )
def frozenset( *args, **kwargs ): raise NotImplementedError( "convert script to yp_frozenset here" )
def set( *args, **kwargs ): raise NotImplementedError( "convert script to yp_set here" )
def dict( *args, **kwargs ): raise NotImplementedError( "convert script to yp_dict here" )
def range( *args, **kwargs ): raise NotImplementedError( "convert script to yp_range here" )
def reversed( *args, **kwargs ): raise NotImplementedError( "convert script to yp_reversed here" )
# TODO same for yp_min, yp_max, etc
# TODO yp_iter(x) throws TypeError if x not a ypObject

n = 10


class TestInvariantWithoutMutations:

    def test_invariant(self):
        it = self.it
        for i in yp_reversed(yp_range(1, n+1)):
            self.assertEqual(length_hint(it), i)
            next(it)
        self.assertEqual(length_hint(it), 0)
        self.assertRaises(StopIteration, next, it)
        self.assertEqual(length_hint(it), 0)

class TestTemporarilyImmutable(TestInvariantWithoutMutations):

    def test_immutable_during_iteration(self):
        # objects such as deques, sets, and dictionaries enforce
        # length immutability  during iteration

        it = self.it
        self.assertEqual(length_hint(it), n)
        next(it)
        self.assertEqual(length_hint(it), n-1)
        self.mutate()
        self.assertRaises(RuntimeError, next, it)
        self.assertEqual(length_hint(it), 0)

## ------- Concrete Type Tests -------

@yp_unittest.skip_not_applicable
class TestRepeat(TestInvariantWithoutMutations, yp_unittest.TestCase):

    def setUp(self):
        self.it = repeat(None, n)

class TestXrange(TestInvariantWithoutMutations, yp_unittest.TestCase):

    def setUp(self):
        self.it = yp_iter(yp_range(n))

class TestXrangeCustomReversed(TestInvariantWithoutMutations, yp_unittest.TestCase):

    def setUp(self):
        self.it = yp_reversed(yp_range(n))

class TestTuple(TestInvariantWithoutMutations, yp_unittest.TestCase):

    def setUp(self):
        self.it = yp_iter(yp_tuple(yp_range(n)))

## ------- Types that should not be mutated during iteration -------

@yp_unittest.skip_not_applicable
class TestDeque(TestTemporarilyImmutable, yp_unittest.TestCase):

    def setUp(self):
        d = deque(yp_range(n))
        self.it = yp_iter(d)
        self.mutate = d.pop

@yp_unittest.skip_not_applicable
class TestDequeReversed(TestTemporarilyImmutable, yp_unittest.TestCase):

    def setUp(self):
        d = deque(yp_range(n))
        self.it = yp_reversed(d)
        self.mutate = d.pop

# XXX Unlike cpython, but allowed by the Python docs, nohtyP dicts and sets do not raise an error
# if mutated during iteration
# TODO Create tests like for list that verify this behaviour

#class TestDictKeys(TestTemporarilyImmutable, yp_unittest.TestCase):
class TestDictKeys(TestInvariantWithoutMutations, yp_unittest.TestCase):

    def setUp(self):
        d = yp_dict.fromkeys(yp_range(n))
        self.it = yp_iter(d)
        self.mutate = d.popitem

#class TestDictItems(TestTemporarilyImmutable, yp_unittest.TestCase):
class TestDictItems(TestInvariantWithoutMutations, yp_unittest.TestCase):

    def setUp(self):
        d = yp_dict.fromkeys(yp_range(n))
        self.it = yp_iter(d.items())
        self.mutate = d.popitem

#class TestDictValues(TestTemporarilyImmutable, yp_unittest.TestCase):
class TestDictValues(TestInvariantWithoutMutations, yp_unittest.TestCase):

    def setUp(self):
        d = yp_dict.fromkeys(yp_range(n))
        self.it = yp_iter(d.values())
        self.mutate = d.popitem

#class TestSet(TestTemporarilyImmutable, yp_unittest.TestCase):
class TestSet(TestInvariantWithoutMutations, yp_unittest.TestCase):

    def setUp(self):
        d = yp_set(yp_range(n))
        self.it = yp_iter(d)
        self.mutate = d.pop

## ------- Types that can mutate during iteration -------

class TestList(TestInvariantWithoutMutations, yp_unittest.TestCase):

    def setUp(self):
        self.it = yp_iter(yp_list(yp_range(n)))

    @yp_unittest.skip_list_mutating_iteration
    def test_mutation(self):
        d = yp_list(yp_range(n))
        it = yp_iter(d)
        next(it)
        next(it)
        self.assertEqual(length_hint(it), n - 2)
        d.append(n)
        self.assertEqual(length_hint(it), n - 1)  # grow with append
        d[1:] = []
        self.assertEqual(length_hint(it), 0)
        self.assertEqual(yp_list(it), [])
        d.extend(yp_range(20))
        self.assertEqual(length_hint(it), 0)

    def test_nohtyP_mutation(self):
        d = yp_list(yp_range(n))
        it = yp_iter(d)
        next(it)
        next(it)
        self.assertEqual(length_hint(it), n - 2)
        d.append(n)
        self.assertEqual(length_hint(it), n - 2)  # ignore append
        d[1:] = []
        self.assertEqual(length_hint(it), n - 2)  # ignore clear
        self.assertEqual(yp_list(it), [])
        d.extend(yp_range(20))
        self.assertEqual(length_hint(it), 0)

class TestListReversed(TestInvariantWithoutMutations, yp_unittest.TestCase):

    def setUp(self):
        self.it = yp_reversed(yp_list(yp_range(n)))

    @yp_unittest.skip_list_mutating_iteration
    def test_mutation(self):
        d = yp_list(yp_range(n))
        it = yp_reversed(d)
        next(it)
        next(it)
        self.assertEqual(length_hint(it), n - 2)
        d.append(n)
        self.assertEqual(length_hint(it), n - 2)  # ignore append
        d[1:] = []
        self.assertEqual(length_hint(it), 0)
        self.assertEqual(yp_list(it), [])  # confirm invariant
        d.extend(yp_range(20))
        self.assertEqual(length_hint(it), 0)

    def test_nohtyP_mutation(self):
        d = yp_list(yp_range(n))
        it = yp_reversed(d)
        next(it)
        next(it)
        self.assertEqual(length_hint(it), n - 2)
        d.append(n)
        self.assertEqual(length_hint(it), n - 2)  # ignore append
        d[1:] = []
        self.assertEqual(length_hint(it), n - 2)  # ignore clear
        self.assertEqual(yp_list(it), [])  # confirm invariant
        d.extend(yp_range(20))
        self.assertEqual(length_hint(it), 0)

## -- Check to make sure exceptions are not suppressed by __length_hint__()


class BadLen(object):
    def __iter__(self):
        return yp_iter(yp_range(10))

    def __len__(self):
        raise RuntimeError('hello')


class BadLengthHint(object):
    def __iter__(self):
        return yp_iter(yp_range(10))

    def __length_hint__(self):
        raise RuntimeError('hello')


class NoneLengthHint(object):
    def __iter__(self):
        return yp_iter(yp_range(10))

    def __length_hint__(self):
        return NotImplemented


class TestLengthHintExceptions(yp_unittest.TestCase):

    def test_issue1242657(self):
        self.assertRaises(RuntimeError, yp_list, BadLen())
        self.assertRaises(RuntimeError, yp_list, BadLengthHint())
        self.assertRaises(RuntimeError, yp_list().extend, BadLen())
        self.assertRaises(RuntimeError, yp_list().extend, BadLengthHint())
        b = yp_bytearray(yp_range(10))
        self.assertRaises(RuntimeError, b.extend, BadLen())
        self.assertRaises(RuntimeError, b.extend, BadLengthHint())

    def test_invalid_hint(self):
        # Make sure an invalid result doesn't muck-up the works
        self.assertEqual(yp_list(NoneLengthHint()), yp_list(yp_range(10)))


if __name__ == "__main__":
    yp_unittest.main()
