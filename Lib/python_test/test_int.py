from yp import *
import sys
import random

from python_test import yp_unittest
from python_test import support
from python_test.test_grammar import (VALID_UNDERSCORE_LITERALS,
                               INVALID_UNDERSCORE_LITERALS)

# Extra assurance that we're not accidentally testing Python's int/etc
def int(*args, **kwargs): raise NotImplementedError("convert script to yp_int here")
def float(*args, **kwargs): raise NotImplementedError("convert script to yp_float here")

L = [
        ('0', yp_int(0)),
        ('1', yp_int(1)),
        ('9', yp_int(9)),
        ('10', yp_int(10)),
        ('99', yp_int(99)),
        ('100', yp_int(100)),
        ('314', yp_int(314)),
        (' 314', yp_int(314)),
        ('314 ', yp_int(314)),
        ('  \t\t  314  \t\t  ', yp_int(314)),
        (repr(yp_sys_maxint._asint()), yp_sys_maxint),
        ('  1x', ValueError),
        ('  1  ', yp_int(1)),
        ('  1\02  ', ValueError),
        ('', ValueError),
        (' ', ValueError),
        ('  \t\t  ', ValueError),
        # TODO(skip_int_fromunicode) Support full Unicode in nohtyP
        #("\u0200", ValueError)
]

class IntSubclass(yp_int):
    pass

class IntTestCases(yp_unittest.TestCase):

    def test_basic(self):
        self.assertEqual(yp_int(314), 314)
        self.assertEqual(yp_int(3.14), 3)
        # Check that conversion from float truncates towards zero
        self.assertEqual(yp_int(-3.14), -3)
        self.assertEqual(yp_int(3.9), 3)
        self.assertEqual(yp_int(-3.9), -3)
        self.assertEqual(yp_int(3.5), 3)
        self.assertEqual(yp_int(-3.5), -3)
        self.assertEqual(yp_int("-3"), -3)
        self.assertEqual(yp_int(" -3 "), -3)
        # TODO(skip_int_fromunicode) Support full Unicode in nohtyP
        #self.assertEqual(yp_int("\N{EM SPACE}-3\N{EN SPACE}"), -3)
        # Different base:
        self.assertEqual(yp_int("10",16), 16)
        # Test conversion from strings and various anomalies
        for s, v in L:
            for sign in "", "+", "-":
                for prefix in "", " ", "\t", "  \t\t  ":
                    ss = prefix + sign + s
                    vv = v
                    if sign == "-" and v is not ValueError:
                        vv = -v
                    try:
                        self.assertEqual(yp_int(ss), vv)
                    except ValueError:
                        pass

        s = repr(-1-yp_sys_maxint._asint())
        x = yp_int(s)
        self.assertEqual(x+1, -yp_sys_maxint)
        self.assertIsInstance(x, yp_int)
        # should overflow
        self.assertRaises(OverflowError, yp_int, s[1:])

        # should return int
        # nohtyP doesn't support numbers this large
        #x = yp_int(1e100)
        #self.assertIsInstance(x, yp_int)
        #x = yp_int(-1e100)
        #self.assertIsInstance(x, yp_int)
        self.assertRaises(OverflowError, yp_int, 1e100)
        self.assertRaises(OverflowError, yp_int, -1e100)

        # SF bug 434186:  0x80000000/2 != 0x80000000>>1.
        # Worked by accident in Windows release build, but failed in debug build.
        # Failed in all Linux builds.
        x = -1-yp_sys_maxint
        self.assertEqual(x >> 1, x//2)

        #x = yp_int('1' * 600)
        #self.assertIsInstance(x, yp_int)
        self.assertRaises(OverflowError, yp_int, '1' * 600)


        self.assertRaises(TypeError, yp_int, 1, 12)

        self.assertEqual(yp_int('0o123', 0), 83)
        self.assertEqual(yp_int('0x123', 16), 291)

        # Bug 1679: "0x" is not a valid hex literal
        self.assertRaises(ValueError, yp_int, "0x", 16)
        self.assertRaises(ValueError, yp_int, "0x", 0)

        self.assertRaises(ValueError, yp_int, "0o", 8)
        self.assertRaises(ValueError, yp_int, "0o", 0)

        self.assertRaises(ValueError, yp_int, "0b", 2)
        self.assertRaises(ValueError, yp_int, "0b", 0)

        # SF bug 1334662: int(string, base) wrong answers
        # Various representations of 2**32 evaluated to 0
        # rather than 2**32 in previous versions

        self.assertEqual(yp_int('100000000000000000000000000000000', 2), 4294967296)
        self.assertEqual(yp_int('102002022201221111211', 3), 4294967296)
        self.assertEqual(yp_int('10000000000000000', 4), 4294967296)
        self.assertEqual(yp_int('32244002423141', 5), 4294967296)
        self.assertEqual(yp_int('1550104015504', 6), 4294967296)
        self.assertEqual(yp_int('211301422354', 7), 4294967296)
        self.assertEqual(yp_int('40000000000', 8), 4294967296)
        self.assertEqual(yp_int('12068657454', 9), 4294967296)
        self.assertEqual(yp_int('4294967296', 10), 4294967296)
        self.assertEqual(yp_int('1904440554', 11), 4294967296)
        self.assertEqual(yp_int('9ba461594', 12), 4294967296)
        self.assertEqual(yp_int('535a79889', 13), 4294967296)
        self.assertEqual(yp_int('2ca5b7464', 14), 4294967296)
        self.assertEqual(yp_int('1a20dcd81', 15), 4294967296)
        self.assertEqual(yp_int('100000000', 16), 4294967296)
        self.assertEqual(yp_int('a7ffda91', 17), 4294967296)
        self.assertEqual(yp_int('704he7g4', 18), 4294967296)
        self.assertEqual(yp_int('4f5aff66', 19), 4294967296)
        self.assertEqual(yp_int('3723ai4g', 20), 4294967296)
        self.assertEqual(yp_int('281d55i4', 21), 4294967296)
        self.assertEqual(yp_int('1fj8b184', 22), 4294967296)
        self.assertEqual(yp_int('1606k7ic', 23), 4294967296)
        self.assertEqual(yp_int('mb994ag', 24), 4294967296)
        self.assertEqual(yp_int('hek2mgl', 25), 4294967296)
        self.assertEqual(yp_int('dnchbnm', 26), 4294967296)
        self.assertEqual(yp_int('b28jpdm', 27), 4294967296)
        self.assertEqual(yp_int('8pfgih4', 28), 4294967296)
        self.assertEqual(yp_int('76beigg', 29), 4294967296)
        self.assertEqual(yp_int('5qmcpqg', 30), 4294967296)
        self.assertEqual(yp_int('4q0jto4', 31), 4294967296)
        self.assertEqual(yp_int('4000000', 32), 4294967296)
        self.assertEqual(yp_int('3aokq94', 33), 4294967296)
        self.assertEqual(yp_int('2qhxjli', 34), 4294967296)
        self.assertEqual(yp_int('2br45qb', 35), 4294967296)
        self.assertEqual(yp_int('1z141z4', 36), 4294967296)

        # tests with base 0
        # this fails on 3.0, but in 2.x the old octal syntax is allowed
        self.assertEqual(yp_int(' 0o123  ', 0), 83)
        self.assertEqual(yp_int(' 0o123  ', 0), 83)
        self.assertEqual(yp_int('000', 0), 0)
        self.assertEqual(yp_int('0o123', 0), 83)
        self.assertEqual(yp_int('0x123', 0), 291)
        self.assertEqual(yp_int('0b100', 0), 4)
        self.assertEqual(yp_int(' 0O123   ', 0), 83)
        self.assertEqual(yp_int(' 0X123  ', 0), 291)
        self.assertEqual(yp_int(' 0B100 ', 0), 4)

        # without base still base 10
        self.assertEqual(yp_int('0123'), 123)
        self.assertEqual(yp_int('0123', 10), 123)

        # tests with prefix and base != 0
        self.assertEqual(yp_int('0x123', 16), 291)
        self.assertEqual(yp_int('0o123', 8), 83)
        self.assertEqual(yp_int('0b100', 2), 4)
        self.assertEqual(yp_int('0X123', 16), 291)
        self.assertEqual(yp_int('0O123', 8), 83)
        self.assertEqual(yp_int('0B100', 2), 4)

        # the code has special checks for the first character after the
        #  type prefix
        self.assertRaises(ValueError, yp_int, '0b2', 2)
        self.assertRaises(ValueError, yp_int, '0b02', 2)
        self.assertRaises(ValueError, yp_int, '0B2', 2)
        self.assertRaises(ValueError, yp_int, '0B02', 2)
        self.assertRaises(ValueError, yp_int, '0o8', 8)
        self.assertRaises(ValueError, yp_int, '0o08', 8)
        self.assertRaises(ValueError, yp_int, '0O8', 8)
        self.assertRaises(ValueError, yp_int, '0O08', 8)
        self.assertRaises(ValueError, yp_int, '0xg', 16)
        self.assertRaises(ValueError, yp_int, '0x0g', 16)
        self.assertRaises(ValueError, yp_int, '0Xg', 16)
        self.assertRaises(ValueError, yp_int, '0X0g', 16)

        # SF bug 1334662: int(string, base) wrong answers
        # Checks for proper evaluation of 2**32 + 1
        self.assertEqual(yp_int('100000000000000000000000000000001', 2), 4294967297)
        self.assertEqual(yp_int('102002022201221111212', 3), 4294967297)
        self.assertEqual(yp_int('10000000000000001', 4), 4294967297)
        self.assertEqual(yp_int('32244002423142', 5), 4294967297)
        self.assertEqual(yp_int('1550104015505', 6), 4294967297)
        self.assertEqual(yp_int('211301422355', 7), 4294967297)
        self.assertEqual(yp_int('40000000001', 8), 4294967297)
        self.assertEqual(yp_int('12068657455', 9), 4294967297)
        self.assertEqual(yp_int('4294967297', 10), 4294967297)
        self.assertEqual(yp_int('1904440555', 11), 4294967297)
        self.assertEqual(yp_int('9ba461595', 12), 4294967297)
        self.assertEqual(yp_int('535a7988a', 13), 4294967297)
        self.assertEqual(yp_int('2ca5b7465', 14), 4294967297)
        self.assertEqual(yp_int('1a20dcd82', 15), 4294967297)
        self.assertEqual(yp_int('100000001', 16), 4294967297)
        self.assertEqual(yp_int('a7ffda92', 17), 4294967297)
        self.assertEqual(yp_int('704he7g5', 18), 4294967297)
        self.assertEqual(yp_int('4f5aff67', 19), 4294967297)
        self.assertEqual(yp_int('3723ai4h', 20), 4294967297)
        self.assertEqual(yp_int('281d55i5', 21), 4294967297)
        self.assertEqual(yp_int('1fj8b185', 22), 4294967297)
        self.assertEqual(yp_int('1606k7id', 23), 4294967297)
        self.assertEqual(yp_int('mb994ah', 24), 4294967297)
        self.assertEqual(yp_int('hek2mgm', 25), 4294967297)
        self.assertEqual(yp_int('dnchbnn', 26), 4294967297)
        self.assertEqual(yp_int('b28jpdn', 27), 4294967297)
        self.assertEqual(yp_int('8pfgih5', 28), 4294967297)
        self.assertEqual(yp_int('76beigh', 29), 4294967297)
        self.assertEqual(yp_int('5qmcpqh', 30), 4294967297)
        self.assertEqual(yp_int('4q0jto5', 31), 4294967297)
        self.assertEqual(yp_int('4000001', 32), 4294967297)
        self.assertEqual(yp_int('3aokq95', 33), 4294967297)
        self.assertEqual(yp_int('2qhxjlj', 34), 4294967297)
        self.assertEqual(yp_int('2br45qc', 35), 4294967297)
        self.assertEqual(yp_int('1z141z5', 36), 4294967297)

    @yp_unittest.skip_int_underscores
    def test_underscores(self):
        for lit in VALID_UNDERSCORE_LITERALS:
            if any(ch in lit for ch in '.eEjJ'):
                continue
            self.assertEqual(yp_int(lit, 0), eval(lit))
            self.assertEqual(yp_int(lit, 0), yp_int(lit.replace('_', ''), 0))
        for lit in INVALID_UNDERSCORE_LITERALS:
            if any(ch in lit for ch in '.eEjJ'):
                continue
            self.assertRaises(ValueError, yp_int, lit, 0)
        # Additional test cases with bases != 0, only for the constructor:
        self.assertEqual(yp_int("1_00", 3), 9)
        self.assertEqual(yp_int("0_100"), 100)  # not valid as a literal!
        self.assertEqual(yp_int(b"1_00"), 100)  # byte underscore
        self.assertRaises(ValueError, yp_int, "_100")
        self.assertRaises(ValueError, yp_int, "+_100")
        self.assertRaises(ValueError, yp_int, "1__00")
        self.assertRaises(ValueError, yp_int, "100_")

    @support.cpython_only
    def test_small_ints(self):
        # Bug #3236: Return small longs from PyLong_FromString
        self.assertIs(yp_int('10'), yp_int(10))
        self.assertIs(yp_int('-1'), yp_int(-1))
        self.assertIs(yp_int(b'10'), yp_int(10))
        self.assertIs(yp_int(b'-1'), yp_int(-1))

    def test_no_args(self):
        self.assertEqual(yp_int(), 0)

    def test_keyword_args(self):
        # Test invoking int() using keyword arguments.
        self.assertEqual(yp_int('100', base=2), 4)
        with self.assertRaisesRegex(TypeError, 'keyword argument'):
            yp_int(x=1.2)
        with self.assertRaisesRegex(TypeError, 'keyword argument'):
            yp_int(x='100', base=2)
        self.assertRaises(TypeError, yp_int, base=10)
        self.assertRaises(TypeError, yp_int, base=0)

    def test_int_base_limits(self):
        """Testing the supported limits of the int() base parameter."""
        self.assertEqual(yp_int('0', 5), 0)
        with self.assertRaises(ValueError):
            yp_int('0', 1)
        with self.assertRaises(ValueError):
            yp_int('0', 37)
        with self.assertRaises(ValueError):
            yp_int('0', -909)  # An old magic value base from Python 2.

    @yp_unittest.skip_long_ints
    def test_int_base_limits_long(self):
        with self.assertRaises(ValueError):
            yp_int('0', base=0-(2**234))
        with self.assertRaises(ValueError):
            yp_int('0', base=2**234)

    def test_int_base_limits_valid(self):
        # Bases 2 through 36 are supported.
        for base in range(2,37):
            self.assertEqual(yp_int('0', base=base), 0)

    def test_int_base_bad_types(self):
        """Not integer types are not valid bases; issue16772."""
        with self.assertRaises(TypeError):
            yp_int('0', 5.5)
        with self.assertRaises(TypeError):
            yp_int('0', 5.0)

    @yp_unittest.skip_user_defined_types
    def test_int_base_indexable(self):
        class MyIndexable(object):
            def __init__(self, value):
                self.value = value
            def __index__(self):
                return self.value

        # Check out of range bases.
        for base in 2**100, -2**100, 1, 37:
            with self.assertRaises(ValueError):
                # TODO Shouldn't CPython use MyIndexable here?
                yp_int('43', base)

        # Check in-range bases.
        self.assertEqual(yp_int('101', base=MyIndexable(2)), 5)
        self.assertEqual(yp_int('101', base=MyIndexable(10)), 101)
        self.assertEqual(yp_int('101', base=MyIndexable(36)), 1 + 36**2)

    def test_non_numeric_input_types(self):
        # Test possible non-numeric types for the argument x, including
        # subclasses of the explicitly documented accepted types.
        class CustomStr(str): pass
        class CustomBytes(bytes): pass
        class CustomByteArray(bytearray): pass

        factories = [
            yp_bytes,
            yp_bytearray,
            # TODO(skip_user_defined_types)
            # lambda b: CustomStr(b.decode()),
            # CustomBytes,
            # CustomByteArray,
            # TODO(skip_memoryview)
            # memoryview,
        ]
        # TODO(skip_array)
        # try:
        #     from array import array
        # except ImportError:
        #     pass
        # else:
        #     factories.append(lambda b: array('B', b))

        for f in factories:
            x = f(b'100')
            with self.subTest(type(x)):
                self.assertEqual(yp_int(x), 100)
                if isinstance(x, (yp_str, yp_bytes, yp_bytearray)):
                    self.assertEqual(yp_int(x, 2), 4)
                else:
                    msg = "can't convert non-string"
                    with self.assertRaisesRegex(TypeError, msg):
                        yp_int(x, 2)
                with self.assertRaisesRegex(ValueError, 'invalid literal'):
                    yp_int(f(b'A' * 0x10))

    @yp_unittest.skip_memoryview
    def test_int_memoryview(self):
        self.assertEqual(yp_int(memoryview(b'123')[1:3]), 23)
        self.assertEqual(yp_int(memoryview(b'123\x00')[1:3]), 23)
        self.assertEqual(yp_int(memoryview(b'123 ')[1:3]), 23)
        self.assertEqual(yp_int(memoryview(b'123A')[1:3]), 23)
        self.assertEqual(yp_int(memoryview(b'1234')[1:3]), 23)

    def test_string_float(self):
        self.assertRaises(ValueError, yp_int, '1.2')

    def test_yp_int_parse_boundaries(self):
        inrange = (
            (0x7ffffffffffffffe, (
                '111111111111111111111111111111111111111111111111111111111111110', # base=2
                '2021110011022210012102010021220101220220', # base=3
                '13333333333333333333333333333332', # base=4
                '1104332401304422434310311211', # base=5
                '1540241003031030222122210', # base=6
                '22341010611245052052266', # base=7
                '777777777777777777776', # base=8
                '67404283172107811826', # base=9
                '9223372036854775806', # base=10
                '1728002635214590696', # base=11
                '41a792678515120366', # base=12
                '10b269549075433c36', # base=13
                '4340724c6c71dc7a6', # base=14
                '160e2ad3246366806', # base=15
                '7ffffffffffffffe', # base=16
                '33d3d8307b214007', # base=17
                '16agh595df825fa6', # base=18
                'ba643dci0ffeehg', # base=19
                '5cbfjia3fh26ja6', # base=20
                '2heiciiie82dh96', # base=21
                '1adaibb21dckfa6', # base=22
                'i6k448cf4192c1', # base=23
                'acd772jnc9l0l6', # base=24
                '64ie1focnn5g76', # base=25
                '3igoecjbmca686', # base=26
                '27c48l5b37oaoo', # base=27
                '1bk39f3ah3dmq6', # base=28
                'q1se8f0m04isa', # base=29
                'hajppbc1fc206', # base=30
                'bm03i95hia436', # base=31
                '7vvvvvvvvvvvu', # base=32
                '5hg4ck9jd4u36', # base=33
                '3tdtk1v8j6tpo', # base=34
                '2pijmikexrxp6', # base=35
                '1y2p0ij32e8e6', # base=36
            )),
            (-0x7fffffffffffffff, (
                '-111111111111111111111111111111111111111111111111111111111111111', # base=2
                '-2021110011022210012102010021220101220221', # base=3
                '-13333333333333333333333333333333', # base=4
                '-1104332401304422434310311212', # base=5
                '-1540241003031030222122211', # base=6
                '-22341010611245052052300', # base=7
                '-777777777777777777777', # base=8
                '-67404283172107811827', # base=9
                '-9223372036854775807', # base=10
                '-1728002635214590697', # base=11
                '-41a792678515120367', # base=12
                '-10b269549075433c37', # base=13
                '-4340724c6c71dc7a7', # base=14
                '-160e2ad3246366807', # base=15
                '-7fffffffffffffff', # base=16
                '-33d3d8307b214008', # base=17
                '-16agh595df825fa7', # base=18
                '-ba643dci0ffeehh', # base=19
                '-5cbfjia3fh26ja7', # base=20
                '-2heiciiie82dh97', # base=21
                '-1adaibb21dckfa7', # base=22
                '-i6k448cf4192c2', # base=23
                '-acd772jnc9l0l7', # base=24
                '-64ie1focnn5g77', # base=25
                '-3igoecjbmca687', # base=26
                '-27c48l5b37oaop', # base=27
                '-1bk39f3ah3dmq7', # base=28
                '-q1se8f0m04isb', # base=29
                '-hajppbc1fc207', # base=30
                '-bm03i95hia437', # base=31
                '-7vvvvvvvvvvvv', # base=32
                '-5hg4ck9jd4u37', # base=33
                '-3tdtk1v8j6tpp', # base=34
                '-2pijmikexrxp7', # base=35
                '-1y2p0ij32e8e7', # base=36
            )),
            (0x7fffffffffffffff, (
                '111111111111111111111111111111111111111111111111111111111111111', # base=2
                '2021110011022210012102010021220101220221', # base=3
                '13333333333333333333333333333333', # base=4
                '1104332401304422434310311212', # base=5
                '1540241003031030222122211', # base=6
                '22341010611245052052300', # base=7
                '777777777777777777777', # base=8
                '67404283172107811827', # base=9
                '9223372036854775807', # base=10
                '1728002635214590697', # base=11
                '41a792678515120367', # base=12
                '10b269549075433c37', # base=13
                '4340724c6c71dc7a7', # base=14
                '160e2ad3246366807', # base=15
                '7fffffffffffffff', # base=16
                '33d3d8307b214008', # base=17
                '16agh595df825fa7', # base=18
                'ba643dci0ffeehh', # base=19
                '5cbfjia3fh26ja7', # base=20
                '2heiciiie82dh97', # base=21
                '1adaibb21dckfa7', # base=22
                'i6k448cf4192c2', # base=23
                'acd772jnc9l0l7', # base=24
                '64ie1focnn5g77', # base=25
                '3igoecjbmca687', # base=26
                '27c48l5b37oaop', # base=27
                '1bk39f3ah3dmq7', # base=28
                'q1se8f0m04isb', # base=29
                'hajppbc1fc207', # base=30
                'bm03i95hia437', # base=31
                '7vvvvvvvvvvvv', # base=32
                '5hg4ck9jd4u37', # base=33
                '3tdtk1v8j6tpp', # base=34
                '2pijmikexrxp7', # base=35
                '1y2p0ij32e8e7', # base=36
            )),
            (-0x8000000000000000, (
                '-1000000000000000000000000000000000000000000000000000000000000000', # base=2
                '-2021110011022210012102010021220101220222', # base=3
                '-20000000000000000000000000000000', # base=4
                '-1104332401304422434310311213', # base=5
                '-1540241003031030222122212', # base=6
                '-22341010611245052052301', # base=7
                '-1000000000000000000000', # base=8
                '-67404283172107811828', # base=9
                '-9223372036854775808', # base=10
                '-1728002635214590698', # base=11
                '-41a792678515120368', # base=12
                '-10b269549075433c38', # base=13
                '-4340724c6c71dc7a8', # base=14
                '-160e2ad3246366808', # base=15
                '-8000000000000000', # base=16
                '-33d3d8307b214009', # base=17
                '-16agh595df825fa8', # base=18
                '-ba643dci0ffeehi', # base=19
                '-5cbfjia3fh26ja8', # base=20
                '-2heiciiie82dh98', # base=21
                '-1adaibb21dckfa8', # base=22
                '-i6k448cf4192c3', # base=23
                '-acd772jnc9l0l8', # base=24
                '-64ie1focnn5g78', # base=25
                '-3igoecjbmca688', # base=26
                '-27c48l5b37oaoq', # base=27
                '-1bk39f3ah3dmq8', # base=28
                '-q1se8f0m04isc', # base=29
                '-hajppbc1fc208', # base=30
                '-bm03i95hia438', # base=31
                '-8000000000000', # base=32
                '-5hg4ck9jd4u38', # base=33
                '-3tdtk1v8j6tpq', # base=34
                '-2pijmikexrxp8', # base=35
                '-1y2p0ij32e8e8', # base=36
            )),
        )
        for value, strings in inrange:
            for base, string in enumerate( strings, 2 ):
                self.assertEqual(yp_int(string, base), value)

        outofrange = (
            (0x8000000000000000, (
                '1000000000000000000000000000000000000000000000000000000000000000', # base=2
                '2021110011022210012102010021220101220222', # base=3
                '20000000000000000000000000000000', # base=4
                '1104332401304422434310311213', # base=5
                '1540241003031030222122212', # base=6
                '22341010611245052052301', # base=7
                '1000000000000000000000', # base=8
                '67404283172107811828', # base=9
                '9223372036854775808', # base=10
                '1728002635214590698', # base=11
                '41a792678515120368', # base=12
                '10b269549075433c38', # base=13
                '4340724c6c71dc7a8', # base=14
                '160e2ad3246366808', # base=15
                '8000000000000000', # base=16
                '33d3d8307b214009', # base=17
                '16agh595df825fa8', # base=18
                'ba643dci0ffeehi', # base=19
                '5cbfjia3fh26ja8', # base=20
                '2heiciiie82dh98', # base=21
                '1adaibb21dckfa8', # base=22
                'i6k448cf4192c3', # base=23
                'acd772jnc9l0l8', # base=24
                '64ie1focnn5g78', # base=25
                '3igoecjbmca688', # base=26
                '27c48l5b37oaoq', # base=27
                '1bk39f3ah3dmq8', # base=28
                'q1se8f0m04isc', # base=29
                'hajppbc1fc208', # base=30
                'bm03i95hia438', # base=31
                '8000000000000', # base=32
                '5hg4ck9jd4u38', # base=33
                '3tdtk1v8j6tpq', # base=34
                '2pijmikexrxp8', # base=35
                '1y2p0ij32e8e8', # base=36
            )),
            (-0x8000000000000001, (
                '-1000000000000000000000000000000000000000000000000000000000000001', # base=2
                '-2021110011022210012102010021220101221000', # base=3
                '-20000000000000000000000000000001', # base=4
                '-1104332401304422434310311214', # base=5
                '-1540241003031030222122213', # base=6
                '-22341010611245052052302', # base=7
                '-1000000000000000000001', # base=8
                '-67404283172107811830', # base=9
                '-9223372036854775809', # base=10
                '-1728002635214590699', # base=11
                '-41a792678515120369', # base=12
                '-10b269549075433c39', # base=13
                '-4340724c6c71dc7a9', # base=14
                '-160e2ad3246366809', # base=15
                '-8000000000000001', # base=16
                '-33d3d8307b21400a', # base=17
                '-16agh595df825fa9', # base=18
                '-ba643dci0ffeei0', # base=19
                '-5cbfjia3fh26ja9', # base=20
                '-2heiciiie82dh99', # base=21
                '-1adaibb21dckfa9', # base=22
                '-i6k448cf4192c4', # base=23
                '-acd772jnc9l0l9', # base=24
                '-64ie1focnn5g79', # base=25
                '-3igoecjbmca689', # base=26
                '-27c48l5b37oap0', # base=27
                '-1bk39f3ah3dmq9', # base=28
                '-q1se8f0m04isd', # base=29
                '-hajppbc1fc209', # base=30
                '-bm03i95hia439', # base=31
                '-8000000000001', # base=32
                '-5hg4ck9jd4u39', # base=33
                '-3tdtk1v8j6tpr', # base=34
                '-2pijmikexrxp9', # base=35
                '-1y2p0ij32e8e9', # base=36
            )),
        )
        for value, strings in outofrange:
            for base, string in enumerate(strings, 2):
                self.assertRaises(OverflowError, yp_int, string, base)

    def test_yp_int_mul_minint(self):
        minint = yp_sys_minint._asint()

        # Run through all possible expressions, and some overflows, with minint as an operand
        self.assertEqual(yp_sys_minint*yp_int(0), minint*0)
        self.assertEqual(yp_int(0)*yp_sys_minint, minint*0)
        self.assertEqual(yp_sys_minint*yp_int(1), minint*1)
        self.assertEqual(yp_int(1)*yp_sys_minint, minint*1)
        self.assertRaises(OverflowError, lambda: yp_sys_minint*yp_int(-1))
        self.assertRaises(OverflowError, lambda: yp_int(-1)*yp_sys_minint)
        self.assertRaises(OverflowError, lambda: yp_sys_minint*yp_int(2))
        self.assertRaises(OverflowError, lambda: yp_int(2)*yp_sys_minint)

        # Run through all possible expressions with minint minint as a result, skipping:
        #   - i==0 would make y==1 and -1, which is tested above
        #   - i==bit_length-1 would make x==1 and -1
        #   - i==bit_length would overflow yp_int_t
        for i in range(1, minint.bit_length()-1):
            for sign in (1, -1):
                y = sign * (1 << i)
                x = minint // y
                self.assertEqual(yp_int(x) * yp_int(y), minint)

    def _yp_int_against_python(self, op, *py_args):
        # Keep the second operand small for certain operations
        if op in (operator.lshift, operator.rshift, operator.pow):
            py_args = (py_args[0], py_args[1]%128)

        msg = "operator.%s(%s)" % (op.__name__, ", ".join(repr(x) for x in py_args))
        yp_args = tuple(yp_int(x) for x in py_args)

        # Any exception raised by Python should be raised by us
        try: py_result = op(*py_args)
        except BaseException as e:
            self.assertRaises(type(e), op, *yp_args)
            return

        # Additionally, if the result doesn't fit a yp_int_t, we expect an overflow
        try: yp_result = yp_int(py_result)
        except:
            self.assertRaises(OverflowError, op, *yp_args)
            return

        # Finally, check that we calculate the same result
        self.assertEqual(op(*yp_args), yp_result, msg=msg)

    def test_yp_int_against_python(self):
        maxint = yp_sys_maxint._asint()
        minint = yp_sys_minint._asint()

        # TODO Support operator.truediv in nohtyP
        unaryOps = (operator.abs, operator.inv, operator.neg, operator.pos)
        binaryOps = (operator.add, operator.and_, operator.floordiv, operator.lshift,
            operator.mod, operator.mul, operator.or_, operator.pow, operator.rshift,
            operator.sub, operator.xor)
        for _ in range(250):
            x = random.randrange(minint, maxint+1)
            y = random.randrange(minint, maxint+1)
            for op in unaryOps:
                self._yp_int_against_python(op, x)
            for op in binaryOps:
                self._yp_int_against_python(op, x, y)

    @yp_unittest.skip_user_defined_types
    def test_intconversion(self):
        # Test __int__()
        class ClassicMissingMethods:
            pass
        self.assertRaises(TypeError, yp_int, ClassicMissingMethods())

        class MissingMethods(object):
            pass
        self.assertRaises(TypeError, yp_int, MissingMethods())

        class Foo0:
            def __int__(self):
                return 42

        self.assertEqual(yp_int(Foo0()), 42)

        class Classic:
            pass
        for base in (object, Classic):
            class IntOverridesTrunc(base):
                def __int__(self):
                    return 42
                def __trunc__(self):
                    return -12
            self.assertEqual(yp_int(IntOverridesTrunc()), 42)

            class JustTrunc(base):
                def __trunc__(self):
                    return 42
            self.assertEqual(yp_int(JustTrunc()), 42)

            class ExceptionalTrunc(base):
                def __trunc__(self):
                    1 / 0
            with self.assertRaises(ZeroDivisionError):
                yp_int(ExceptionalTrunc())

            for trunc_result_base in (object, Classic):
                class Index(trunc_result_base):
                    def __index__(self):
                        return 42

                class TruncReturnsNonInt(base):
                    def __trunc__(self):
                        return Integral()
                self.assertEqual(yp_int(TruncReturnsNonInt()), 42)

                class Intable(trunc_result_base):
                    def __int__(self):
                        return 42

                class TruncReturnsNonIndex(base):
                    def __trunc__(self):
                        return Intable()
                self.assertEqual(yp_int(TruncReturnsNonInt()), 42)

                class NonIntegral(trunc_result_base):
                    def __trunc__(self):
                        # Check that we avoid infinite recursion.
                        return NonIntegral()

                class TruncReturnsNonIntegral(base):
                    def __trunc__(self):
                        return NonIntegral()
                try:
                    yp_int(TruncReturnsNonIntegral())
                except TypeError as e:
                    self.assertEqual(str(e),
                                      "__trunc__ returned non-Integral"
                                      " (type NonIntegral)")
                else:
                    self.fail("Failed to raise TypeError with %s" %
                              ((base, trunc_result_base),))

                # Regression test for bugs.python.org/issue16060.
                class BadInt(trunc_result_base):
                    def __int__(self):
                        return 42.0

                class TruncReturnsBadInt(base):
                    def __trunc__(self):
                        return BadInt()

                with self.assertRaises(TypeError):
                    yp_int(TruncReturnsBadInt())

    @yp_unittest.skip_user_defined_types
    def test_int_subclass_with_index(self):
        class MyIndex(yp_int):
            def __index__(self):
                return 42

        class BadIndex(yp_int):
            def __index__(self):
                return 42.0

        my_int = MyIndex(7)
        self.assertEqual(my_int, 7)
        self.assertEqual(yp_int(my_int), 7)

        self.assertEqual(yp_int(BadIndex()), 0)

    @yp_unittest.skip_user_defined_types
    def test_int_subclass_with_int(self):
        class MyInt(yp_int):
            def __int__(self):
                return 42

        class BadInt(yp_int):
            def __int__(self):
                return 42.0

        my_int = MyInt(7)
        self.assertEqual(my_int, 7)
        self.assertEqual(yp_int(my_int), 42)

        my_int = BadInt(7)
        self.assertEqual(my_int, 7)
        self.assertRaises(TypeError, yp_int, my_int)

    @yp_unittest.skip_user_defined_types
    def test_int_returns_int_subclass(self):
        class BadIndex:
            def __index__(self):
                return True

        class BadIndex2(yp_int):
            def __index__(self):
                return True

        class BadInt:
            def __int__(self):
                return True

        class BadInt2(yp_int):
            def __int__(self):
                return True

        class TruncReturnsBadIndex:
            def __trunc__(self):
                return BadIndex()

        class TruncReturnsBadInt:
            def __trunc__(self):
                return BadInt()

        class TruncReturnsIntSubclass:
            def __trunc__(self):
                return True

        bad_int = BadIndex()
        with self.assertWarns(DeprecationWarning):
            n = yp_int(bad_int)
        self.assertEqual(n, 1)
        self.assertIs(type(n), yp_int)

        bad_int = BadIndex2()
        n = yp_int(bad_int)
        self.assertEqual(n, 0)
        self.assertIs(type(n), yp_int)

        bad_int = BadInt()
        with self.assertWarns(DeprecationWarning):
            n = yp_int(bad_int)
        self.assertEqual(n, 1)
        self.assertIs(type(n), yp_int)

        bad_int = BadInt2()
        with self.assertWarns(DeprecationWarning):
            n = yp_int(bad_int)
        self.assertEqual(n, 1)
        self.assertIs(type(n), yp_int)

        bad_int = TruncReturnsBadIndex()
        with self.assertWarns(DeprecationWarning):
            n = yp_int(bad_int)
        self.assertEqual(n, 1)
        self.assertIs(type(n), yp_int)

        bad_int = TruncReturnsBadInt()
        self.assertRaises(TypeError, yp_int, bad_int)

        good_int = TruncReturnsIntSubclass()
        n = yp_int(good_int)
        self.assertEqual(n, 1)
        self.assertIs(type(n), yp_int)
        n = IntSubclass(good_int)
        self.assertEqual(n, 1)
        self.assertIs(type(n), IntSubclass)

    def test_error_message(self):
        def check(s, base=None):
            with self.assertRaises(ValueError,
                                   msg="int(%r, %r)" % (s, base)) as cm:
                if base is None:
                    yp_int(s)
                else:
                    yp_int(s, base)
            # TODO(skip_exception_messages)
            # self.assertEqual(cm.exception.args[0],
            #     "invalid literal for int() with base %d: %r" %
            #     (10 if base is None else base, s))

        check('\xbd')
        check('123\xbd')
        check('  123 456  ')

        check('123\x00')
        # SF bug 1545497: embedded NULs were not detected with explicit base
        check('123\x00', 10)
        check('123\x00 245', 20)
        check('123\x00 245', 16)
        check('123\x00245', 20)
        check('123\x00245', 16)
        # byte string with embedded NUL
        check(b'123\x00')
        check(b'123\x00', 10)
        # non-UTF-8 byte string
        check(b'123\xbd')
        check(b'123\xbd', 10)
        # lone surrogate in Unicode string
        check('123\ud800')
        check('123\ud800', 10)

    @yp_unittest.skip_int_underscores
    def test_issue31619(self):
        self.assertEqual(yp_int('1_0_1_0_1_0_1_0_1_0_1_0_1_0_1_0_1_0_1_0_1_0_1_0_1_0_1_0_1_0_1', 2),
                         0b1010101010101010101010101010101)
        self.assertEqual(yp_int('1_2_3_4_5_6_7_0_1_2_3', 8), 0o12345670123)
        self.assertEqual(yp_int('1_2_3_4_5_6_7_8_9', 16), 0x123456789)
        self.assertEqual(yp_int('1_2_3_4_5_6_7', 32), 1144132807)


if __name__ == "__main__":
    yp_unittest.main()
