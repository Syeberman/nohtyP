from yp import *
import sys

import unittest
from yp_test import support

# TODO There may be tests in Python's test_long that are applicable to us as well

# Extra assurance that we're not accidentally testing Python's int
def int( *args, **kwargs ): raise NotImplementedError( "convert script to yp_int here" )

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
        # TODO Support full Unicode in nohtyP
        #("\u0200", ValueError)
]

class IntTestCases(unittest.TestCase):

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
        # TODO Support full Unicode in nohtyP
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
        x = yp_int(1e100)
        self.assertIsInstance(x, yp_int)
        x = yp_int(-1e100)
        self.assertIsInstance(x, yp_int)


        # SF bug 434186:  0x80000000/2 != 0x80000000>>1.
        # Worked by accident in Windows release build, but failed in debug build.
        # Failed in all Linux builds.
        x = -1-yp_sys_maxint
        self.assertEqual(x >> 1, x//2)

        self.assertRaises(ValueError, yp_int, '123\0')
        self.assertRaises(ValueError, yp_int, '53', 40)

        # SF bug 1545497: embedded NULs were not detected with
        # explicit base
        self.assertRaises(ValueError, yp_int, '123\0', 10)
        self.assertRaises(ValueError, yp_int, '123\x00 245', 20)

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

    @unittest.skip("TODO: Support full Unicode in nohtyP")
    def test_basic_unicode(self):
        self.assertEqual(yp_int("\N{EM SPACE}-3\N{EN SPACE}"), -3)

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
            for base, string in enumerate( strings, 2 ):
                self.assertRaises(OverflowError, yp_int, string, base)

    @unittest.skip("REWORK: nohtyP doesn't have user-defined types yet")
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

        class Foo1(object):
            def __int__(self):
                return 42

        class Foo2(yp_int):
            def __int__(self):
                return 42

        class Foo3(yp_int):
            def __int__(self):
                return self

        class Foo4(yp_int):
            def __int__(self):
                return 42

        class Foo5(yp_int):
            def __int__(self):
                return 42.

        self.assertEqual(yp_int(Foo0()), 42)
        self.assertEqual(yp_int(Foo1()), 42)
        self.assertEqual(yp_int(Foo2()), 42)
        self.assertEqual(yp_int(Foo3()), 0)
        self.assertEqual(yp_int(Foo4()), 42)
        self.assertRaises(TypeError, yp_int, Foo5())

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
                class Integral(trunc_result_base):
                    def __int__(self):
                        return 42

                class TruncReturnsNonInt(base):
                    def __trunc__(self):
                        return Integral()
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

    @unittest.skip("Not applicable to nohtyP")
    def test_error_message(self):
        testlist = ('\xbd', '123\xbd', '  123 456  ')
        for s in testlist:
            try:
                yp_int(s)
            except ValueError as e:
                self.assertIn(s.strip(), e.args[0])
            else:
                self.fail("Expected yp_int(%r) to raise a ValueError", s)

def test_main():
    support.run_unittest(IntTestCases)

if __name__ == "__main__":
    test_main()
