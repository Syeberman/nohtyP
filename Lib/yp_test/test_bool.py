# Test properties of bool promised by PEP 285

from yp import *
from yp_test import yp_unittest
from yp_test import support

import os

# Extra assurance that we're not accidentally testing Python's bool; unfortunately we can't
# redefine True/False because they are keywords
def bool( *args, **kwargs ): raise NotImplementedError( "convert script to yp_bool here" )

class BoolTest(yp_unittest.TestCase):

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_subclass(self):
        try:
            class C(bool):
                pass
        except TypeError:
            pass
        else:
            self.fail("bool should not be subclassable")

        self.assertRaises(TypeError, int.__new__, bool, 0)

    def test_print(self):
        try:
            fo = open(support.TESTFN, "w")
            print(yp_False, yp_True, file=fo)
            fo.close()
            fo = open(support.TESTFN, "r")
            with self.nohtyPCheck(False):
                self.assertEqual(fo.read(), 'False True\n')
        finally:
            fo.close()
            os.remove(support.TESTFN)

    def test_repr(self):
        self.assertEqual(yp_repr(yp_False), 'False')
        self.assertEqual(yp_repr(yp_True), 'True')
        self.assertEqual(eval(repr(yp_False)), yp_False)
        self.assertEqual(eval(repr(yp_True)), yp_True)

    def test_str(self):
        self.assertEqual(yp_str(yp_False), 'False')
        self.assertEqual(yp_str(yp_True), 'True')

    def test_int(self):
        self.assertEqual(yp_int(yp_False), 0)
        self.assertIsNot(yp_int(yp_False), yp_False)
        self.assertEqual(yp_int(yp_True), 1)
        self.assertIsNot(yp_int(yp_True), yp_True)

    def test_float(self):
        self.assertEqual(yp_float(yp_False), 0.0)
        self.assertIsNot(yp_float(yp_False), yp_False)
        self.assertEqual(yp_float(yp_True), 1.0)
        self.assertIsNot(yp_float(yp_True), yp_True)

    def test_math(self):
        self.assertEqual(+yp_False, 0)
        self.assertIsNot(+yp_False, yp_False)
        self.assertEqual(-yp_False, 0)
        self.assertIsNot(-yp_False, yp_False)
        self.assertEqual(abs(yp_False), 0)
        self.assertIsNot(abs(yp_False), yp_False)
        self.assertEqual(+yp_True, 1)
        self.assertIsNot(+yp_True, yp_True)
        self.assertEqual(-yp_True, -1)
        self.assertEqual(abs(yp_True), 1)
        self.assertIsNot(abs(yp_True), yp_True)
        self.assertEqual(~yp_False, -1)
        self.assertEqual(~yp_True, -2)

        self.assertEqual(yp_False+2, 2)
        self.assertEqual(yp_True+2, 3)
        self.assertEqual(2+yp_False, 2)
        self.assertEqual(2+yp_True, 3)

        self.assertEqual(yp_False+yp_False, 0)
        self.assertIsNot(yp_False+yp_False, yp_False)
        self.assertEqual(yp_False+yp_True, 1)
        self.assertIsNot(yp_False+yp_True, yp_True)
        self.assertEqual(yp_True+yp_False, 1)
        self.assertIsNot(yp_True+yp_False, yp_True)
        self.assertEqual(yp_True+yp_True, 2)

        self.assertEqual(yp_True-yp_True, 0)
        self.assertIsNot(yp_True-yp_True, yp_False)
        self.assertEqual(yp_False-yp_False, 0)
        self.assertIsNot(yp_False-yp_False, yp_False)
        self.assertEqual(yp_True-yp_False, 1)
        self.assertIsNot(yp_True-yp_False, yp_True)
        self.assertEqual(yp_False-yp_True, -1)

        self.assertEqual(yp_True*1, 1)
        self.assertEqual(yp_False*1, 0)
        self.assertIsNot(yp_False*1, yp_False)

        self.assertEqual(yp_True/1, 1)
        self.assertIsNot(yp_True/1, yp_True)
        self.assertEqual(yp_False/1, 0)
        self.assertIsNot(yp_False/1, yp_False)

        for b in yp_False, yp_True:
            for i in 0, 1, 2:
                self.assertEqual(b**i, yp_int(b)**i)
                self.assertIsNot(b**i, yp_bool(yp_int(b)**i))

        for a in yp_False, yp_True:
            for b in yp_False, yp_True:
                self.assertIs(a&b, yp_bool(yp_int(a)&yp_int(b)))
                self.assertIs(a|b, yp_bool(yp_int(a)|yp_int(b)))
                self.assertIs(a^b, yp_bool(yp_int(a)^yp_int(b)))
                self.assertEqual(a&yp_int(b), yp_int(a)&yp_int(b))
                self.assertIsNot(a&yp_int(b), yp_bool(yp_int(a)&yp_int(b)))
                self.assertEqual(a|yp_int(b), yp_int(a)|yp_int(b))
                self.assertIsNot(a|yp_int(b), yp_bool(yp_int(a)|yp_int(b)))
                self.assertEqual(a^yp_int(b), yp_int(a)^yp_int(b))
                self.assertIsNot(a^yp_int(b), yp_bool(yp_int(a)^yp_int(b)))
                self.assertEqual(yp_int(a)&b, yp_int(a)&yp_int(b))
                self.assertIsNot(yp_int(a)&b, yp_bool(yp_int(a)&yp_int(b)))
                self.assertEqual(yp_int(a)|b, yp_int(a)|yp_int(b))
                self.assertIsNot(yp_int(a)|b, yp_bool(yp_int(a)|yp_int(b)))
                self.assertEqual(yp_int(a)^b, yp_int(a)^yp_int(b))
                self.assertIsNot(yp_int(a)^b, yp_bool(yp_int(a)^yp_int(b)))

        zero = yp_int( 0 )
        one = yp_int( 1 )

        self.assertIs(one==one, yp_True)
        self.assertIs(one==zero, yp_False)
        self.assertIs(zero<one, yp_True)
        self.assertIs(one<zero, yp_False)
        self.assertIs(zero<=zero, yp_True)
        self.assertIs(one<=zero, yp_False)
        self.assertIs(one>zero, yp_True)
        self.assertIs(one>one, yp_False)
        self.assertIs(one>=one, yp_True)
        self.assertIs(zero>=one, yp_False)
        self.assertIs(zero!=one, yp_True)
        self.assertIs(zero!=zero, yp_False)

        x = yp_list( [one] )
        # Not applicable to nohtyP
        #self.assertIs(x is x, yp_True)
        #self.assertIs(x is not x, yp_False)

        self.assertIs(x.__contains__( one ), yp_True)
        self.assertIs(x.__contains__( zero ), yp_False)
        # Not applicable to nohtyP
        #self.assertIs(one not in x, yp_False)
        #self.assertIs(zero not in x, yp_True)

        x = yp_dict( {one: yp_int( 2 )} )
        # Not applicable to nohtyP
        #self.assertIs(x is x, yp_True)
        #self.assertIs(x is not x, yp_False)

        self.assertIs(x.__contains__( one ), yp_True)
        self.assertIs(x.__contains__( zero ), yp_False)
        # Not applicable to nohtyP
        #self.assertIs(one not in x, yp_False)
        #self.assertIs(zero not in x, yp_True)

        # Not applicable to nohtyP
        #self.assertIs(not yp_True, yp_False)
        #self.assertIs(not yp_False, yp_True)

    def test_convert(self):
        self.assertRaises(TypeError, yp_bool, yp_int(42), yp_int(42))
        self.assertIs(yp_bool(yp_int(10)), yp_True)
        self.assertIs(yp_bool(yp_int(1)), yp_True)
        self.assertIs(yp_bool(yp_int(-1)), yp_True)
        self.assertIs(yp_bool(yp_int(0)), yp_False)
        self.assertIs(yp_bool(yp_str("hello")), yp_True)
        self.assertIs(yp_bool(yp_str("")), yp_False)
        self.assertIs(yp_bool(), yp_False)

    @yp_unittest.skip("TODO: Enable when formatting supported in nohtyP")
    def test_format(self):
        self.assertEqual("%d" % yp_False, "0")
        self.assertEqual("%d" % yp_True, "1")
        self.assertEqual("%x" % yp_False, "0")
        self.assertEqual("%x" % yp_True, "1")

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_hasattr(self):
        self.assertIs(hasattr([], "append"), yp_True)
        self.assertIs(hasattr([], "wobble"), yp_False)

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_callable(self):
        self.assertIs(callable(len), yp_True)
        self.assertIs(callable(1), yp_False)

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_isinstance(self):
        self.assertIs(isinstance(yp_True, yp_bool), yp_True)
        self.assertIs(isinstance(yp_False, yp_bool), yp_True)
        self.assertIs(isinstance(yp_True, yp_int), yp_True)
        self.assertIs(isinstance(yp_False, yp_int), yp_True)
        self.assertIs(isinstance(1, yp_bool), yp_False)
        self.assertIs(isinstance(0, yp_bool), yp_False)

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_issubclass(self):
        self.assertIs(issubclass(yp_bool, yp_int), yp_True)
        self.assertIs(issubclass(yp_int, yp_bool), yp_False)

    def test_contains(self):
        self.assertIs(yp_dict( {} ).__contains__( 1 ), yp_False)
        self.assertIs(yp_dict( {1:1} ).__contains__( 1 ), yp_True)

    @yp_unittest.skip("TODO: Enable when nohtyP has proper str type")
    def test_string(self):
        self.assertIs(yp_str( "xyz" ).endswith("z"), yp_True)
        self.assertIs(yp_str( "xyz" ).endswith("x"), yp_False)
        self.assertIs(yp_str( "xyz0123" ).isalnum(), yp_True)
        self.assertIs(yp_str( "@#$%" ).isalnum(), yp_False)
        self.assertIs(yp_str( "xyz" ).isalpha(), yp_True)
        self.assertIs(yp_str( "@#$%" ).isalpha(), yp_False)
        self.assertIs(yp_str( "0123" ).isdigit(), yp_True)
        self.assertIs(yp_str( "xyz" ).isdigit(), yp_False)
        self.assertIs(yp_str( "xyz" ).islower(), yp_True)
        self.assertIs(yp_str( "XYZ" ).islower(), yp_False)
        self.assertIs(yp_str( "0123" ).isdecimal(), yp_True)
        self.assertIs(yp_str( "xyz" ).isdecimal(), yp_False)
        self.assertIs(yp_str( "0123" ).isnumeric(), yp_True)
        self.assertIs(yp_str( "xyz" ).isnumeric(), yp_False)
        self.assertIs(yp_str( " " ).isspace(), yp_True)
        self.assertIs(yp_str( "\xa0" ).isspace(), yp_True)
        self.assertIs(yp_str( "\u3000" ).isspace(), yp_True)
        self.assertIs(yp_str( "XYZ" ).isspace(), yp_False)
        self.assertIs(yp_str( "X" ).istitle(), yp_True)
        self.assertIs(yp_str( "x" ).istitle(), yp_False)
        self.assertIs(yp_str( "XYZ" ).isupper(), yp_True)
        self.assertIs(yp_str( "xyz" ).isupper(), yp_False)
        self.assertIs(yp_str( "xyz" ).startswith("x"), yp_True)
        self.assertIs(yp_str( "xyz" ).startswith("z"), yp_False)

    def test_boolean(self):
        self.assertEqual(yp_True & 1, 1)
        self.assertNotIsInstance(yp_True & 1, yp_bool)
        self.assertIs(yp_True & yp_True, yp_True)

        self.assertEqual(yp_True | 1, 1)
        self.assertNotIsInstance(yp_True | 1, yp_bool)
        self.assertIs(yp_True | yp_True, yp_True)

        self.assertEqual(yp_True ^ 1, 0)
        self.assertNotIsInstance(yp_True ^ 1, yp_bool)
        self.assertIs(yp_True ^ yp_True, yp_False)

    @yp_unittest.skip("TODO: Implement files in nohtyP")
    def test_fileclosed(self):
        try:
            f = open(support.TESTFN, "w")
            self.assertIs(f.closed, yp_False)
            f.close()
            self.assertIs(f.closed, yp_True)
        finally:
            os.remove(support.TESTFN)

    def test_types(self):
        # types are always true.
        # TODO complex? object? other nohtyP types?
        for t in [yp_t_bool, yp_t_dict, yp_t_float, yp_t_int, yp_t_list,
                  yp_t_set, yp_t_str, yp_t_tuple, yp_t_type]:
            self.assertIs(yp_bool(t), yp_True)

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_operator(self):
        import operator
        self.assertIs(operator.truth(0), yp_False)
        self.assertIs(operator.truth(1), yp_True)
        self.assertIs(operator.not_(1), yp_False)
        self.assertIs(operator.not_(0), yp_True)
        self.assertIs(operator.contains([], 1), yp_False)
        self.assertIs(operator.contains([1], 1), yp_True)
        self.assertIs(operator.lt(0, 0), yp_False)
        self.assertIs(operator.lt(0, 1), yp_True)
        self.assertIs(operator.is_(yp_True, yp_True), yp_True)
        self.assertIs(operator.is_(yp_True, yp_False), yp_False)
        self.assertIs(operator.is_not(yp_True, yp_True), yp_False)
        self.assertIs(operator.is_not(yp_True, yp_False), yp_True)

    @yp_unittest.skip("TODO: Implement nohtyP pickling")
    def test_marshal(self):
        import marshal
        self.assertIs(marshal.loads(marshal.dumps(yp_True)), yp_True)
        self.assertIs(marshal.loads(marshal.dumps(yp_False)), yp_False)

    @yp_unittest.skip("TODO: Implement nohtyP pickling")
    def test_pickle(self):
        import pickle
        self.assertIs(pickle.loads(pickle.dumps(yp_True)), yp_True)
        self.assertIs(pickle.loads(pickle.dumps(yp_False)), yp_False)
        self.assertIs(pickle.loads(pickle.dumps(yp_True, yp_True)), yp_True)
        self.assertIs(pickle.loads(pickle.dumps(yp_False, yp_True)), yp_False)

    @yp_unittest.skip("TODO: Implement nohtyP pickling")
    def test_picklevalues(self):
        # Test for specific backwards-compatible pickle values
        import pickle
        self.assertEqual(pickle.dumps(yp_True, protocol=0), b"I01\n.")
        self.assertEqual(pickle.dumps(yp_False, protocol=0), b"I00\n.")
        self.assertEqual(pickle.dumps(yp_True, protocol=1), b"I01\n.")
        self.assertEqual(pickle.dumps(yp_False, protocol=1), b"I00\n.")
        self.assertEqual(pickle.dumps(yp_True, protocol=2), b'\x80\x02\x88.')
        self.assertEqual(pickle.dumps(yp_False, protocol=2), b'\x80\x02\x89.')

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_convert_to_bool(self):
        # Verify that TypeError occurs when bad things are returned
        # from __bool__().  This isn't really a bool test, but
        # it's related.
        check = lambda o: self.assertRaises(TypeError, yp_bool, o)
        class Foo(object):
            def __bool__(self):
                return self
        check(Foo())

        class Bar(object):
            def __bool__(self):
                return "Yes"
        check(Bar())

        class Baz(int):
            def __bool__(self):
                return self
        check(Baz())

        # __bool__() must return a bool not an int
        class Spam(int):
            def __bool__(self):
                return 1
        check(Spam())

        class Eggs:
            def __len__(self):
                return -1
        self.assertRaises(ValueError, yp_bool, Eggs())

    @yp_unittest.skip("Not applicable to nohtyP")
    def test_sane_len(self):
        # this test just tests our assumptions about __len__
        # this will start failing if __len__ changes assertions
        for badval in ['illegal', -1, 1 << 32]:
            class A:
                def __len__(self):
                    return badval
            try:
                yp_bool(A())
            except (Exception) as e_bool:
                try:
                    len(A())
                except (Exception) as e_len:
                    self.assertEqual(str(e_bool), str(e_len))

    @yp_unittest.skip("TODO: Implement real/imag/etc in nohtyP")
    def test_real_and_imag(self):
        self.assertEqual(yp_True.real, 1)
        self.assertEqual(yp_True.imag, 0)
        self.assertIs(type(yp_True.real), int)
        self.assertIs(type(yp_True.imag), int)
        self.assertEqual(yp_False.real, 0)
        self.assertEqual(yp_False.imag, 0)
        self.assertIs(type(yp_False.real), int)
        self.assertIs(type(yp_False.imag), int)

def test_main():
    support.run_unittest(BoolTest)

if __name__ == "__main__":
    test_main()
