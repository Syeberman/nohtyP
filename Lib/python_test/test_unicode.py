""" Test script for the Unicode implementation.

Written by Marc-Andre Lemburg (mal@lemburg.com).

(c) Copyright CNRI, All Rights Reserved. NO WARRANTY.

"""
from yp import *
import _string
import codecs
import itertools
import operator
import struct
import sys
import textwrap
import unicodedata
from python_test import yp_unittest
import warnings
from python_test.support import import_helper
from python_test.support import warnings_helper
from python_test import support, string_tests
from python_test.support.script_helper import assert_python_failure

# Extra assurance that we're not accidentally testing Python's str
def chr(*args, **kwargs): raise NotImplementedError("convert script to yp_chr here")
def str(*args, **kwargs): raise NotImplementedError("convert script to yp_str here")
def repr(*args, **kwargs): raise NotImplementedError("convert script to yp_repr here")
# TODO ord
def bytes(*args, **kwargs): raise NotImplementedError("convert script to yp_bytes here")
def bytearray(*args, **kwargs): raise NotImplementedError("convert script to yp_bytearray here")

# Error handling (bad decoder return)
def search_function(encoding):
    def decode1(input, errors="strict"):
        return 42 # not a tuple
    def encode1(input, errors="strict"):
        return 42 # not a tuple
    def encode2(input, errors="strict"):
        return (42, 42) # no unicode
    def decode2(input, errors="strict"):
        return (42, 42) # no unicode
    if encoding=="test.unicode1":
        return (encode1, decode1, None, None)
    elif encoding=="test.unicode2":
        return (encode2, decode2, None, None)
    else:
        return None

def duplicate_string(text):
    """
    Try to get a fresh clone of the specified text:
    new object with a reference count of 1.

    This is a best-effort: latin1 single letters and the empty
    string ('') are singletons and cannot be cloned.
    """
    return text.encode().decode()

class StrSubclass(yp_str):
    pass

class UnicodeTest(string_tests.CommonTest,
        string_tests.MixinStrUnicodeUserStringTest,
        string_tests.MixinStrUnicodeTest,
        yp_unittest.TestCase):

    type2test = yp_str

    def setUp(self):
        codecs.register(search_function)
        # self.addCleanup(codecs.unregister, search_function)

    def checkequalnofix(self, result, object, methodname, *args):
        method = getattr(object, methodname)
        realresult = method(*args)
        self.assertEqual(realresult, result)
        self.assertIs(type(realresult), type(result))

        # if the original is returned make sure that
        # this doesn't happen with subclasses
        if realresult is object:
            class usub(yp_str):
                def __repr__(self):
                    return 'usub(%r)' % yp_str.__repr__(self)
            object = usub(object)
            method = getattr(object, methodname)
            realresult = method(*args)
            self.assertEqual(realresult, result)
            self.assertTrue(object is not realresult)

    @yp_unittest.skip_not_applicable
    def test_literals(self):
        self.assertEqual('\xff', '\u00ff')
        self.assertEqual('\uffff', '\U0000ffff')
        self.assertRaises(SyntaxError, eval, '\'\\Ufffffffe\'')
        self.assertRaises(SyntaxError, eval, '\'\\Uffffffff\'')
        self.assertRaises(SyntaxError, eval, '\'\\U%08x\'' % 0x110000)
        # raw strings should not have unicode escapes
        self.assertNotEqual(r"\u0020", " ")

    @yp_unittest.skip_func_ascii
    def test_ascii(self):
        if not sys.platform.startswith('java'):
            # Test basic sanity of yp_repr()
            self.assertEqual(ascii('abc'), "'abc'")
            self.assertEqual(ascii('ab\\c'), "'ab\\\\c'")
            self.assertEqual(ascii('ab\\'), "'ab\\\\'")
            self.assertEqual(ascii('\\c'), "'\\\\c'")
            self.assertEqual(ascii('\\'), "'\\\\'")
            self.assertEqual(ascii('\n'), "'\\n'")
            self.assertEqual(ascii('\r'), "'\\r'")
            self.assertEqual(ascii('\t'), "'\\t'")
            self.assertEqual(ascii('\b'), "'\\x08'")
            self.assertEqual(ascii("'\""), """'\\'"'""")
            self.assertEqual(ascii("'\""), """'\\'"'""")
            self.assertEqual(ascii("'"), '''"'"''')
            self.assertEqual(ascii('"'), """'"'""")
            latin1repr = (
                "'\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\x08\\t\\n\\x0b\\x0c\\r"
                "\\x0e\\x0f\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1a"
                "\\x1b\\x1c\\x1d\\x1e\\x1f !\"#$%&\\'()*+,-./0123456789:;<=>?@ABCDEFGHI"
                "JKLMNOPQRSTUVWXYZ[\\\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\\x7f"
                "\\x80\\x81\\x82\\x83\\x84\\x85\\x86\\x87\\x88\\x89\\x8a\\x8b\\x8c\\x8d"
                "\\x8e\\x8f\\x90\\x91\\x92\\x93\\x94\\x95\\x96\\x97\\x98\\x99\\x9a\\x9b"
                "\\x9c\\x9d\\x9e\\x9f\\xa0\\xa1\\xa2\\xa3\\xa4\\xa5\\xa6\\xa7\\xa8\\xa9"
                "\\xaa\\xab\\xac\\xad\\xae\\xaf\\xb0\\xb1\\xb2\\xb3\\xb4\\xb5\\xb6\\xb7"
                "\\xb8\\xb9\\xba\\xbb\\xbc\\xbd\\xbe\\xbf\\xc0\\xc1\\xc2\\xc3\\xc4\\xc5"
                "\\xc6\\xc7\\xc8\\xc9\\xca\\xcb\\xcc\\xcd\\xce\\xcf\\xd0\\xd1\\xd2\\xd3"
                "\\xd4\\xd5\\xd6\\xd7\\xd8\\xd9\\xda\\xdb\\xdc\\xdd\\xde\\xdf\\xe0\\xe1"
                "\\xe2\\xe3\\xe4\\xe5\\xe6\\xe7\\xe8\\xe9\\xea\\xeb\\xec\\xed\\xee\\xef"
                "\\xf0\\xf1\\xf2\\xf3\\xf4\\xf5\\xf6\\xf7\\xf8\\xf9\\xfa\\xfb\\xfc\\xfd"
                "\\xfe\\xff'")
            testrepr = ascii(''.join(map(chr, range(256))))
            self.assertEqual(testrepr, latin1repr)
            # Test ascii works on wide unicode escapes without overflow.
            self.assertEqual(ascii("\U00010000" * 39 + "\uffff" * 4096),
                             ascii("\U00010000" * 39 + "\uffff" * 4096))

            class WrongRepr:
                def __repr__(self):
                    return b'byte-repr'
            self.assertRaises(TypeError, ascii, WrongRepr())

    def test_repr(self):
        if not sys.platform.startswith('java'):
            # Test basic sanity of yp_repr()
            self.assertEqual(yp_repr('abc'), "'abc'")
            self.assertEqual(yp_repr('ab\\c'), "'ab\\\\c'")
            self.assertEqual(yp_repr('ab\\'), "'ab\\\\'")
            self.assertEqual(yp_repr('\\c'), "'\\\\c'")
            self.assertEqual(yp_repr('\\'), "'\\\\'")
            self.assertEqual(yp_repr('\n'), "'\\n'")
            self.assertEqual(yp_repr('\r'), "'\\r'")
            self.assertEqual(yp_repr('\t'), "'\\t'")
            self.assertEqual(yp_repr('\b'), "'\\x08'")
            self.assertEqual(yp_repr("'\""), """'\\'"'""")
            self.assertEqual(yp_repr("'\""), """'\\'"'""")
            self.assertEqual(yp_repr("'"), '''"'"''')
            self.assertEqual(yp_repr('"'), """'"'""")
            latin1repr = yp_str(
                "'\\x00\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\x08\\t\\n\\x0b\\x0c\\r"
                "\\x0e\\x0f\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1a"
                "\\x1b\\x1c\\x1d\\x1e\\x1f !\"#$%&\\'()*+,-./0123456789:;<=>?@ABCDEFGHI"
                "JKLMNOPQRSTUVWXYZ[\\\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\\x7f"
                "\\x80\\x81\\x82\\x83\\x84\\x85\\x86\\x87\\x88\\x89\\x8a\\x8b\\x8c\\x8d"
                "\\x8e\\x8f\\x90\\x91\\x92\\x93\\x94\\x95\\x96\\x97\\x98\\x99\\x9a\\x9b"
                "\\x9c\\x9d\\x9e\\x9f\\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9"
                "\xaa\xab\xac\\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7"
                "\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5"
                "\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3"
                "\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1"
                "\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"
                "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd"
                "\xfe\xff'")
            testrepr = yp_repr(yp_str('').join(map(yp_chr, yp_range(256))))
            self.assertEqual(testrepr, latin1repr)
            # Test yp_repr works on wide unicode escapes without overflow.
            self.assertEqual(yp_repr(yp_str("\U00010000") * 39 + yp_str("\uffff") * 4096),
                             yp_repr(yp_str("\U00010000") * 39 + yp_str("\uffff") * 4096))

            class WrongRepr:
                def __repr__(self):
                    return b'byte-repr'
            self.assertRaises(TypeError, yp_repr, WrongRepr())

    def test_iterators(self):
        # Make sure unicode objects have an __iter__ method
        it = yp_str("\u1111\u2222\u3333").__iter__()
        self.assertEqual(next(it), "\u1111")
        self.assertEqual(next(it), "\u2222")
        self.assertEqual(next(it), "\u3333")
        self.assertRaises(StopIteration, next, it)

    def test_count(self):
        string_tests.CommonTest.test_count(self)
        # check mixed argument types
        self.checkequalnofix(yp_int(3),  yp_str('aaa'), 'count', 'a')
        self.checkequalnofix(yp_int(0),  yp_str('aaa'), 'count', 'b')
        self.checkequalnofix(yp_int(3), yp_str('aaa'), 'count',  'a')
        self.checkequalnofix(yp_int(0), yp_str('aaa'), 'count',  'b')
        self.checkequalnofix(yp_int(0), yp_str('aaa'), 'count',  'b')
        self.checkequalnofix(yp_int(1), yp_str('aaa'), 'count',  'a', -1)
        self.checkequalnofix(yp_int(3), yp_str('aaa'), 'count',  'a', -10)
        self.checkequalnofix(yp_int(2), yp_str('aaa'), 'count',  'a', 0, -1)
        self.checkequalnofix(yp_int(0), yp_str('aaa'), 'count',  'a', 0, -10)
        # test mixed kinds
        self.checkequal(10, '\u0102' + 'a' * 10, 'count', 'a')
        self.checkequal(10, '\U00100304' + 'a' * 10, 'count', 'a')
        self.checkequal(10, '\U00100304' + '\u0102' * 10, 'count', '\u0102')
        self.checkequal(0, 'a' * 10, 'count', '\u0102')
        self.checkequal(0, 'a' * 10, 'count', '\U00100304')
        self.checkequal(0, '\u0102' * 10, 'count', '\U00100304')
        self.checkequal(10, '\u0102' + 'a_' * 10, 'count', 'a_')
        self.checkequal(10, '\U00100304' + 'a_' * 10, 'count', 'a_')
        self.checkequal(10, '\U00100304' + '\u0102_' * 10, 'count', '\u0102_')
        self.checkequal(0, 'a' * 10, 'count', 'a\u0102')
        self.checkequal(0, 'a' * 10, 'count', 'a\U00100304')
        self.checkequal(0, '\u0102' * 10, 'count', '\u0102\U00100304')

    def test_find(self):
        string_tests.CommonTest.test_find(self)
        # test implementation details of the memchr fast path
        self.checkequal(100, 'a' * 100 + '\u0102', 'find', '\u0102')
        self.checkequal(-1, 'a' * 100 + '\u0102', 'find', '\u0201')
        self.checkequal(-1, 'a' * 100 + '\u0102', 'find', '\u0120')
        self.checkequal(-1, 'a' * 100 + '\u0102', 'find', '\u0220')
        self.checkequal(100, 'a' * 100 + '\U00100304', 'find', '\U00100304')
        self.checkequal(-1, 'a' * 100 + '\U00100304', 'find', '\U00100204')
        self.checkequal(-1, 'a' * 100 + '\U00100304', 'find', '\U00102004')
        # check mixed argument types
        self.checkequalnofix(yp_int(0),  yp_str('abcdefghiabc'), 'find', 'abc')
        self.checkequalnofix(yp_int(9),  yp_str('abcdefghiabc'), 'find', 'abc', 1)
        self.checkequalnofix(yp_int(-1), yp_str('abcdefghiabc'), 'find', 'def', 4)

        self.assertRaises(TypeError, yp_str('hello').find)
        self.assertRaises(TypeError, yp_str('hello').find, 42)
        # test mixed kinds
        self.checkequal(100, '\u0102' * 100 + 'a', 'find', 'a')
        self.checkequal(100, '\U00100304' * 100 + 'a', 'find', 'a')
        self.checkequal(100, '\U00100304' * 100 + '\u0102', 'find', '\u0102')
        self.checkequal(-1, 'a' * 100, 'find', '\u0102')
        self.checkequal(-1, 'a' * 100, 'find', '\U00100304')
        self.checkequal(-1, '\u0102' * 100, 'find', '\U00100304')
        self.checkequal(100, '\u0102' * 100 + 'a_', 'find', 'a_')
        self.checkequal(100, '\U00100304' * 100 + 'a_', 'find', 'a_')
        self.checkequal(100, '\U00100304' * 100 + '\u0102_', 'find', '\u0102_')
        self.checkequal(-1, 'a' * 100, 'find', 'a\u0102')
        self.checkequal(-1, 'a' * 100, 'find', 'a\U00100304')
        self.checkequal(-1, '\u0102' * 100, 'find', '\u0102\U00100304')

    def test_rfind(self):
        string_tests.CommonTest.test_rfind(self)
        # test implementation details of the memrchr fast path
        self.checkequal(0, '\u0102' + 'a' * 100 , 'rfind', '\u0102')
        self.checkequal(-1, '\u0102' + 'a' * 100 , 'rfind', '\u0201')
        self.checkequal(-1, '\u0102' + 'a' * 100 , 'rfind', '\u0120')
        self.checkequal(-1, '\u0102' + 'a' * 100 , 'rfind', '\u0220')
        self.checkequal(0, '\U00100304' + 'a' * 100, 'rfind', '\U00100304')
        self.checkequal(-1, '\U00100304' + 'a' * 100, 'rfind', '\U00100204')
        self.checkequal(-1, '\U00100304' + 'a' * 100, 'rfind', '\U00102004')
        # check mixed argument types
        self.checkequalnofix(yp_int(9),   yp_str('abcdefghiabc'), 'rfind', 'abc')
        self.checkequalnofix(yp_int(12),  yp_str('abcdefghiabc'), 'rfind', '')
        self.checkequalnofix(yp_int(12), yp_str('abcdefghiabc'), 'rfind',  '')
        # test mixed kinds
        self.checkequal(0, 'a' + '\u0102' * 100, 'rfind', 'a')
        self.checkequal(0, 'a' + '\U00100304' * 100, 'rfind', 'a')
        self.checkequal(0, '\u0102' + '\U00100304' * 100, 'rfind', '\u0102')
        self.checkequal(-1, 'a' * 100, 'rfind', '\u0102')
        self.checkequal(-1, 'a' * 100, 'rfind', '\U00100304')
        self.checkequal(-1, '\u0102' * 100, 'rfind', '\U00100304')
        self.checkequal(0, '_a' + '\u0102' * 100, 'rfind', '_a')
        self.checkequal(0, '_a' + '\U00100304' * 100, 'rfind', '_a')
        self.checkequal(0, '_\u0102' + '\U00100304' * 100, 'rfind', '_\u0102')
        self.checkequal(-1, 'a' * 100, 'rfind', '\u0102a')
        self.checkequal(-1, 'a' * 100, 'rfind', '\U00100304a')
        self.checkequal(-1, '\u0102' * 100, 'rfind', '\U00100304\u0102')

    def test_index(self):
        string_tests.CommonTest.test_index(self)
        self.checkequalnofix(yp_int(0), yp_str('abcdefghiabc'), 'index',  '')
        self.checkequalnofix(yp_int(3), yp_str('abcdefghiabc'), 'index',  'def')
        self.checkequalnofix(yp_int(0), yp_str('abcdefghiabc'), 'index',  'abc')
        self.checkequalnofix(yp_int(9), yp_str('abcdefghiabc'), 'index',  'abc', 1)
        self.assertRaises(ValueError, yp_str('abcdefghiabc').index, 'hib')
        self.assertRaises(ValueError, yp_str('abcdefghiab').index,  'abc', 1)
        self.assertRaises(ValueError, yp_str('abcdefghi').index,  'ghi', 8)
        self.assertRaises(ValueError, yp_str('abcdefghi').index,  'ghi', -1)
        # test mixed kinds
        self.checkequal(100, '\u0102' * 100 + 'a', 'index', 'a')
        self.checkequal(100, '\U00100304' * 100 + 'a', 'index', 'a')
        self.checkequal(100, '\U00100304' * 100 + '\u0102', 'index', '\u0102')
        self.assertRaises(ValueError, yp_str('a' * 100).index, '\u0102')
        self.assertRaises(ValueError, yp_str('a' * 100).index, '\U00100304')
        self.assertRaises(ValueError, yp_str('\u0102' * 100).index, '\U00100304')
        self.checkequal(100, '\u0102' * 100 + 'a_', 'index', 'a_')
        self.checkequal(100, '\U00100304' * 100 + 'a_', 'index', 'a_')
        self.checkequal(100, '\U00100304' * 100 + '\u0102_', 'index', '\u0102_')
        self.assertRaises(ValueError, yp_str('a' * 100).index, 'a\u0102')
        self.assertRaises(ValueError, yp_str('a' * 100).index, 'a\U00100304')
        self.assertRaises(ValueError, yp_str('\u0102' * 100).index, '\u0102\U00100304')

    def test_rindex(self):
        string_tests.CommonTest.test_rindex(self)
        self.checkequalnofix(yp_int(12), yp_str('abcdefghiabc'), 'rindex',  '')
        self.checkequalnofix(yp_int(3),  yp_str('abcdefghiabc'), 'rindex',  'def')
        self.checkequalnofix(yp_int(9),  yp_str('abcdefghiabc'), 'rindex',  'abc')
        self.checkequalnofix(yp_int(0),  yp_str('abcdefghiabc'), 'rindex',  'abc', 0, -1)

        self.assertRaises(ValueError, yp_str('abcdefghiabc').rindex,  'hib')
        self.assertRaises(ValueError, yp_str('defghiabc').rindex,  'def', 1)
        self.assertRaises(ValueError, yp_str('defghiabc').rindex,  'abc', 0, -1)
        self.assertRaises(ValueError, yp_str('abcdefghi').rindex,  'ghi', 0, 8)
        self.assertRaises(ValueError, yp_str('abcdefghi').rindex,  'ghi', 0, -1)
        # test mixed kinds
        self.checkequal(0, 'a' + '\u0102' * 100, 'rindex', 'a')
        self.checkequal(0, 'a' + '\U00100304' * 100, 'rindex', 'a')
        self.checkequal(0, '\u0102' + '\U00100304' * 100, 'rindex', '\u0102')
        self.assertRaises(ValueError, yp_str('a' * 100).rindex, '\u0102')
        self.assertRaises(ValueError, yp_str('a' * 100).rindex, '\U00100304')
        self.assertRaises(ValueError, yp_str('\u0102' * 100).rindex, '\U00100304')
        self.checkequal(0, '_a' + '\u0102' * 100, 'rindex', '_a')
        self.checkequal(0, '_a' + '\U00100304' * 100, 'rindex', '_a')
        self.checkequal(0, '_\u0102' + '\U00100304' * 100, 'rindex', '_\u0102')
        self.assertRaises(ValueError, yp_str('a' * 100).rindex, '\u0102a')
        self.assertRaises(ValueError, yp_str('a' * 100).rindex, '\U00100304a')
        self.assertRaises(ValueError, yp_str('\u0102' * 100).rindex, '\U00100304\u0102')

    @yp_unittest.skip_str_replace
    def test_maketrans_translate(self):
        # these work with plain translate()
        self.checkequalnofix('bbbc', 'abababc', 'translate',
                             {ord('a'): None})
        self.checkequalnofix('iiic', 'abababc', 'translate',
                             {ord('a'): None, ord('b'): ord('i')})
        self.checkequalnofix('iiix', 'abababc', 'translate',
                             {ord('a'): None, ord('b'): ord('i'), ord('c'): 'x'})
        self.checkequalnofix('c', 'abababc', 'translate',
                             {ord('a'): None, ord('b'): ''})
        self.checkequalnofix('xyyx', 'xzx', 'translate',
                             {ord('z'): 'yy'})

        # this needs maketrans()
        self.checkequalnofix('abababc', 'abababc', 'translate',
                             {'b': '<i>'})
        tbl = self.type2test.maketrans({'a': None, 'b': '<i>'})
        self.checkequalnofix('<i><i><i>c', 'abababc', 'translate', tbl)
        # test alternative way of calling maketrans()
        tbl = self.type2test.maketrans('abc', 'xyz', 'd')
        self.checkequalnofix('xyzzy', 'abdcdcbdddd', 'translate', tbl)

        # various tests switching from ASCII to latin1 or the opposite;
        # same length, remove a letter, or replace with a longer string.
        self.assertEqual("[a]".translate(yp_str.maketrans('a', 'X')),
                         "[X]")
        self.assertEqual("[a]".translate(yp_str.maketrans({'a': 'X'})),
                         "[X]")
        self.assertEqual("[a]".translate(yp_str.maketrans({'a': None})),
                         "[]")
        self.assertEqual("[a]".translate(yp_str.maketrans({'a': 'XXX'})),
                         "[XXX]")
        self.assertEqual("[a]".translate(yp_str.maketrans({'a': '\xe9'})),
                         "[\xe9]")
        self.assertEqual('axb'.translate(yp_str.maketrans({'a': None, 'b': '123'})),
                         "x123")
        self.assertEqual('axb'.translate(yp_str.maketrans({'a': None, 'b': '\xe9'})),
                         "x\xe9")

        # test non-ASCII (don't take the fast-path)
        self.assertEqual("[a]".translate(yp_str.maketrans({'a': '<\xe9>'})),
                         "[<\xe9>]")
        self.assertEqual("[\xe9]".translate(yp_str.maketrans({'\xe9': 'a'})),
                         "[a]")
        self.assertEqual("[\xe9]".translate(yp_str.maketrans({'\xe9': None})),
                         "[]")
        self.assertEqual("[\xe9]".translate(yp_str.maketrans({'\xe9': '123'})),
                         "[123]")
        self.assertEqual("[a\xe9]".translate(yp_str.maketrans({'a': '<\u20ac>'})),
                         "[<\u20ac>\xe9]")

        # invalid Unicode characters
        invalid_char = 0x10ffff+1
        for before in "a\xe9\u20ac\U0010ffff":
            mapping = yp_str.maketrans({before: invalid_char})
            text = "[%s]" % before
            self.assertRaises(ValueError, text.translate, mapping)

        # errors
        self.assertRaises(TypeError, self.type2test.maketrans)
        self.assertRaises(ValueError, self.type2test.maketrans, 'abc', 'defg')
        self.assertRaises(TypeError, self.type2test.maketrans, 2, 'def')
        self.assertRaises(TypeError, self.type2test.maketrans, 'abc', 2)
        self.assertRaises(TypeError, self.type2test.maketrans, 'abc', 'def', 2)
        self.assertRaises(ValueError, self.type2test.maketrans, {'xy': 2})
        self.assertRaises(TypeError, self.type2test.maketrans, {(1,): 2})

        self.assertRaises(TypeError, 'hello'.translate)
        self.assertRaises(TypeError, 'abababc'.translate, 'abc', 'xyz')

    @yp_unittest.skip_str_split
    def test_split(self):
        string_tests.CommonTest.test_split(self)

        # test mixed kinds
        for left, right in ('ba', '\u0101\u0100', '\U00010301\U00010300'):
            left *= 9
            right *= 9
            for delim in ('c', '\u0102', '\U00010302'):
                self.checkequal([left + right],
                                left + right, 'split', delim)
                self.checkequal([left, right],
                                left + delim + right, 'split', delim)
                self.checkequal([left + right],
                                left + right, 'split', delim * 2)
                self.checkequal([left, right],
                                left + delim * 2 + right, 'split', delim *2)

    def test_rsplit(self):
        string_tests.CommonTest.test_rsplit(self)
        # test mixed kinds
        for left, right in ('ba', '\u0101\u0100', '\U00010301\U00010300'):
            left *= 9
            right *= 9
            for delim in ('c', '\u0102', '\U00010302'):
                self.checkequal([left + right],
                                left + right, 'rsplit', delim)
                self.checkequal([left, right],
                                left + delim + right, 'rsplit', delim)
                self.checkequal([left + right],
                                left + right, 'rsplit', delim * 2)
                self.checkequal([left, right],
                                left + delim * 2 + right, 'rsplit', delim *2)

    def test_partition(self):
        string_tests.MixinStrUnicodeUserStringTest.test_partition(self)
        # test mixed kinds
        self.checkequal(('ABCDEFGH', '', ''), 'ABCDEFGH', 'partition', '\u4200')
        for left, right in ('ba', '\u0101\u0100', '\U00010301\U00010300'):
            left *= 9
            right *= 9
            for delim in ('c', '\u0102', '\U00010302'):
                self.checkequal((left + right, '', ''),
                                left + right, 'partition', delim)
                self.checkequal((left, delim, right),
                                left + delim + right, 'partition', delim)
                self.checkequal((left + right, '', ''),
                                left + right, 'partition', delim * 2)
                self.checkequal((left, delim * 2, right),
                                left + delim * 2 + right, 'partition', delim * 2)

    def test_rpartition(self):
        string_tests.MixinStrUnicodeUserStringTest.test_rpartition(self)
        # test mixed kinds
        self.checkequal(('', '', 'ABCDEFGH'), 'ABCDEFGH', 'rpartition', '\u4200')
        for left, right in ('ba', '\u0101\u0100', '\U00010301\U00010300'):
            left *= 9
            right *= 9
            for delim in ('c', '\u0102', '\U00010302'):
                self.checkequal(('', '', left + right),
                                left + right, 'rpartition', delim)
                self.checkequal((left, delim, right),
                                left + delim + right, 'rpartition', delim)
                self.checkequal(('', '', left + right),
                                left + right, 'rpartition', delim * 2)
                self.checkequal((left, delim * 2, right),
                                left + delim * 2 + right, 'rpartition', delim * 2)

    def test_join(self):
        string_tests.MixinStrUnicodeUserStringTest.test_join(self)

        class MyWrapper:
            def __init__(self, sval): self.sval = sval
            def __str__(self): return self.sval

        # mixed arguments
        self.checkequalnofix(yp_str('a b c d'),  yp_str(' '), 'join', yp_list(['a', 'b', 'c', 'd']))
        self.checkequalnofix(yp_str('abcd'),     yp_str(''),  'join', yp_tuple(('a', 'b', 'c', 'd')))
        self.checkequalnofix(yp_str('w x y z'),  yp_str(' '), 'join', string_tests.Sequence('wxyz'))
        self.checkequalnofix(yp_str('a b c d'),  yp_str(' '), 'join', yp_list(['a', 'b', 'c', 'd']))
        self.checkequalnofix(yp_str('a b c d'),  yp_str(' '), 'join', yp_list(['a', 'b', 'c', 'd']))
        self.checkequalnofix(yp_str('abcd'),     yp_str(''),  'join', yp_tuple(('a', 'b', 'c', 'd')))
        self.checkequalnofix(yp_str('w x y z'),  yp_str(' '), 'join', string_tests.Sequence('wxyz'))
        # TODO(skip_user_defined_types) No subclasses in nohtyP (yet)
        #self.checkraises(TypeError, yp_str(' '), 'join', yp_list(['1', '2', MyWrapper('foo')]))
        self.checkraises(TypeError, yp_str(' '), 'join', yp_list(['1', '2', '3', yp_bytes()]))
        self.checkraises(TypeError, yp_str(' '), 'join', yp_list([1, 2, 3]))
        self.checkraises(TypeError, yp_str(' '), 'join', yp_list(['1', '2', 3]))

    @yp_unittest.skipIf(sys.maxsize > 2**32,
        'needs too much memory on a 64-bit platform')
    def test_join_overflow(self):
        size = int(sys.maxsize**0.5) + 1
        seq = ('A' * size,) * size
        self.assertRaises(OverflowError, ''.join, seq)

    def test_replace(self):
        string_tests.CommonTest.test_replace(self)

        # method call forwarded from yp_str implementation because of unicode argument
        self.checkequalnofix('one@two!three!', 'one!two!three!', 'replace', '!', '@', 1)
        self.assertRaises(TypeError, 'replace'.replace, "r", 42)
        # test mixed kinds
        for left, right in ('ba', '\u0101\u0100', '\U00010301\U00010300'):
            left *= 9
            right *= 9
            for delim in ('c', '\u0102', '\U00010302'):
                for repl in ('d', '\u0103', '\U00010303'):
                    self.checkequal(left + right,
                                    left + right, 'replace', delim, repl)
                    self.checkequal(left + repl + right,
                                    left + delim + right,
                                    'replace', delim, repl)
                    self.checkequal(left + right,
                                    left + right, 'replace', delim * 2, repl)
                    self.checkequal(left + repl + right,
                                    left + delim * 2 + right,
                                    'replace', delim * 2, repl)

    @support.cpython_only
    @yp_unittest.skip_str_replace
    def test_replace_id(self):
        pattern = yp_str('abc')
        text = yp_str('abc def')
        self.assertIs(text.replace(pattern, pattern), text)

    def test_repeat_id_preserving(self):
        a = yp_str('123abc1@')
        b = yp_str('456zyx-+')
        with self.nohtyPCheck(enabled=False):
            self.assertEqual(id(a), id(a))
            self.assertNotEqual(id(a), id(b))
            self.assertNotEqual(id(a), id(a * -4))
            self.assertNotEqual(id(a), id(a * 0))
            self.assertEqual(id(a), id(a * 1))
            self.assertEqual(id(a), id(1 * a))
            self.assertNotEqual(id(a), id(a * 2))

    @yp_unittest.skip_user_defined_types
    def test_repeat_id_preserving_user_defined_types(self):
        class SubStr(yp_str):
            pass

        s = SubStr('qwerty()')
        self.assertEqual(id(s), id(s))
        self.assertNotEqual(id(s), id(s * -4))
        self.assertNotEqual(id(s), id(s * 0))
        self.assertNotEqual(id(s), id(s * 1))
        self.assertNotEqual(id(s), id(1 * s))
        self.assertNotEqual(id(s), id(s * 2))

    def test_bytes_comparison(self):
        with warnings_helper.check_warnings():
            warnings.simplefilter('ignore', BytesWarning)
            self.assertEqual(yp_str('abc') == yp_bytes(b'abc'), yp_False)
            self.assertEqual(yp_str('abc') != yp_bytes(b'abc'), yp_True)
            self.assertEqual(yp_str('abc') == yp_bytearray(b'abc'), yp_False)
            self.assertEqual(yp_str('abc') != yp_bytearray(b'abc'), yp_True)

    def test_comparison(self):
        # Comparisons:
        self.assertEqual(yp_str('abc'), yp_str('abc'))
        self.assertTrue(yp_str('abcd') > yp_str('abc'))
        self.assertTrue(yp_str('abc') < yp_str('abcd'))

    @yp_unittest.skip_str_big_chars
    def test_comparison_big_chars(self):
        if 0:
            # Move these tests to a Unicode collation module test...
            # Testing UTF-16 code point order comparisons...

            # No surrogates, no fixup required.
            self.assertTrue(yp_str('\u0061') < yp_str('\u20ac'))
            # Non surrogate below surrogate value, no fixup required
            self.assertTrue(yp_str('\u0061') < yp_str('\ud800\udc02'))

            # Non surrogate above surrogate value, fixup required
            def test_lecmp(s, s2):
                self.assertTrue(s < s2)

            def test_fixup(s):
                s2 = yp_str('\ud800\udc01')
                test_lecmp(s, s2)
                s2 = yp_str('\ud900\udc01')
                test_lecmp(s, s2)
                s2 = yp_str('\uda00\udc01')
                test_lecmp(s, s2)
                s2 = yp_str('\udb00\udc01')
                test_lecmp(s, s2)
                s2 = yp_str('\ud800\udd01')
                test_lecmp(s, s2)
                s2 = yp_str('\ud900\udd01')
                test_lecmp(s, s2)
                s2 = yp_str('\uda00\udd01')
                test_lecmp(s, s2)
                s2 = yp_str('\udb00\udd01')
                test_lecmp(s, s2)
                s2 = yp_str('\ud800\ude01')
                test_lecmp(s, s2)
                s2 = yp_str('\ud900\ude01')
                test_lecmp(s, s2)
                s2 = yp_str('\uda00\ude01')
                test_lecmp(s, s2)
                s2 = yp_str('\udb00\ude01')
                test_lecmp(s, s2)
                s2 = yp_str('\ud800\udfff')
                test_lecmp(s, s2)
                s2 = yp_str('\ud900\udfff')
                test_lecmp(s, s2)
                s2 = yp_str('\uda00\udfff')
                test_lecmp(s, s2)
                s2 = yp_str('\udb00\udfff')
                test_lecmp(s, s2)

            # TODO CPython has an indentation bug here, contribute back
            test_fixup(yp_str('\ue000'))
            test_fixup(yp_str('\uff61'))

        # Surrogates on both sides, no fixup required
        self.assertTrue(yp_str('\ud800\udc02') < yp_str('\ud84d\udc56'))

    @yp_unittest.skip_str_case
    def test_islower(self):
        super().test_islower()
        self.checkequalnofix(yp_False, '\u1FFc', 'islower')
        self.assertFalse('\u2167'.islower())
        self.assertTrue('\u2177'.islower())
        # non-BMP, uppercase
        self.assertFalse('\U00010401'.islower())
        self.assertFalse('\U00010427'.islower())
        # non-BMP, lowercase
        self.assertTrue('\U00010429'.islower())
        self.assertTrue('\U0001044E'.islower())
        # non-BMP, non-cased
        self.assertFalse('\U0001F40D'.islower())
        self.assertFalse('\U0001F46F'.islower())

    @yp_unittest.skip_str_case
    def test_isupper(self):
        super().test_isupper()
        if not sys.platform.startswith('java'):
            self.checkequalnofix(yp_False, '\u1FFc', 'isupper')
        self.assertTrue('\u2167'.isupper())
        self.assertFalse('\u2177'.isupper())
        # non-BMP, uppercase
        self.assertTrue('\U00010401'.isupper())
        self.assertTrue('\U00010427'.isupper())
        # non-BMP, lowercase
        self.assertFalse('\U00010429'.isupper())
        self.assertFalse('\U0001044E'.isupper())
        # non-BMP, non-cased
        self.assertFalse('\U0001F40D'.isupper())
        self.assertFalse('\U0001F46F'.isupper())

    @yp_unittest.skip_str_case
    def test_istitle(self):
        super().test_istitle()
        self.checkequalnofix(yp_True, '\u1FFc', 'istitle')
        self.checkequalnofix(yp_True, 'Greek \u1FFcitlecases ...', 'istitle')

        # non-BMP, uppercase + lowercase
        self.assertTrue('\U00010401\U00010429'.istitle())
        self.assertTrue('\U00010427\U0001044E'.istitle())
        # apparently there are no titlecased (Lt) non-BMP chars in Unicode 6
        for ch in ['\U00010429', '\U0001044E', '\U0001F40D', '\U0001F46F']:
            self.assertFalse(ch.istitle(), '{!a} is not title'.format(ch))

    @yp_unittest.skip_str_space
    def test_isspace(self):
        super().test_isspace()
        self.checkequalnofix(yp_True, '\u2000', 'isspace')
        self.checkequalnofix(yp_True, '\u200a', 'isspace')
        self.checkequalnofix(yp_False, '\u2014', 'isspace')
        # There are no non-BMP whitespace chars as of Unicode 12.
        for ch in ['\U00010401', '\U00010427', '\U00010429', '\U0001044E',
                   '\U0001F40D', '\U0001F46F']:
            self.assertFalse(ch.isspace(), '{!a} is not space.'.format(ch))

    @support.requires_resource('cpu')
    def test_isspace_invariant(self):
        for codepoint in range(sys.maxunicode + 1):
            char = chr(codepoint)
            bidirectional = unicodedata.bidirectional(char)
            category = unicodedata.category(char)
            self.assertEqual(char.isspace(),
                             (bidirectional in ('WS', 'B', 'S')
                              or category == 'Zs'))

    @yp_unittest.skip_str_unicode_db
    def test_isalnum(self):
        super().test_isalnum()
        for ch in ['\U00010401', '\U00010427', '\U00010429', '\U0001044E',
                   '\U0001D7F6', '\U00011066', '\U000104A0', '\U0001F107']:
            self.assertTrue(ch.isalnum(), '{!a} is alnum.'.format(ch))

    @yp_unittest.skip_str_unicode_db
    def test_isalpha(self):
        super().test_isalpha()
        self.checkequalnofix(yp_True, '\u1FFc', 'isalpha')
        # non-BMP, cased
        self.assertTrue('\U00010401'.isalpha())
        self.assertTrue('\U00010427'.isalpha())
        self.assertTrue('\U00010429'.isalpha())
        self.assertTrue('\U0001044E'.isalpha())
        # non-BMP, non-cased
        self.assertFalse('\U0001F40D'.isalpha())
        self.assertFalse('\U0001F46F'.isalpha())

    @yp_unittest.skip_str_unicode_db
    def test_isascii(self):
        super().test_isascii()
        self.assertFalse("\u20ac".isascii())
        self.assertFalse("\U0010ffff".isascii())

    @yp_unittest.skip_str_unicode_db
    def test_isdecimal(self):
        self.checkequalnofix(yp_False, '', 'isdecimal')
        self.checkequalnofix(yp_False, 'a', 'isdecimal')
        self.checkequalnofix(yp_True, '0', 'isdecimal')
        self.checkequalnofix(yp_False, '\u2460', 'isdecimal') # CIRCLED DIGIT ONE
        self.checkequalnofix(yp_False, '\xbc', 'isdecimal') # VULGAR FRACTION ONE QUARTER
        self.checkequalnofix(yp_True, '\u0660', 'isdecimal') # ARABIC-INDIC DIGIT ZERO
        self.checkequalnofix(yp_True, '0123456789', 'isdecimal')
        self.checkequalnofix(yp_False, '0123456789a', 'isdecimal')

        self.checkraises(TypeError, 'abc', 'isdecimal', 42)

        for ch in ['\U00010401', '\U00010427', '\U00010429', '\U0001044E',
                   '\U0001F40D', '\U0001F46F', '\U00011065', '\U0001F107']:
            self.assertFalse(ch.isdecimal(), '{!a} is not decimal.'.format(ch))
        for ch in ['\U0001D7F6', '\U00011066', '\U000104A0']:
            self.assertTrue(ch.isdecimal(), '{!a} is decimal.'.format(ch))

    @yp_unittest.skip_str_unicode_db
    def test_isdigit(self):
        super().test_isdigit()
        self.checkequalnofix(yp_True, '\u2460', 'isdigit')
        self.checkequalnofix(yp_False, '\xbc', 'isdigit')
        self.checkequalnofix(yp_True, '\u0660', 'isdigit')

        for ch in ['\U00010401', '\U00010427', '\U00010429', '\U0001044E',
                   '\U0001F40D', '\U0001F46F', '\U00011065']:
            self.assertFalse(ch.isdigit(), '{!a} is not a digit.'.format(ch))
        for ch in ['\U0001D7F6', '\U00011066', '\U000104A0', '\U0001F107']:
            self.assertTrue(ch.isdigit(), '{!a} is a digit.'.format(ch))

    @yp_unittest.skip_str_unicode_db
    def test_isnumeric(self):
        self.checkequalnofix(yp_False, '', 'isnumeric')
        self.checkequalnofix(yp_False, 'a', 'isnumeric')
        self.checkequalnofix(yp_True, '0', 'isnumeric')
        self.checkequalnofix(yp_True, '\u2460', 'isnumeric')
        self.checkequalnofix(yp_True, '\xbc', 'isnumeric')
        self.checkequalnofix(yp_True, '\u0660', 'isnumeric')
        self.checkequalnofix(yp_True, '0123456789', 'isnumeric')
        self.checkequalnofix(yp_False, '0123456789a', 'isnumeric')

        self.assertRaises(TypeError, "abc".isnumeric, 42)

        for ch in ['\U00010401', '\U00010427', '\U00010429', '\U0001044E',
                   '\U0001F40D', '\U0001F46F']:
            self.assertFalse(ch.isnumeric(), '{!a} is not numeric.'.format(ch))
        for ch in ['\U00011065', '\U0001D7F6', '\U00011066',
                   '\U000104A0', '\U0001F107']:
            self.assertTrue(ch.isnumeric(), '{!a} is numeric.'.format(ch))

    @yp_unittest.skip_str_unicode_db
    def test_isidentifier(self):
        self.assertTrue("a".isidentifier())
        self.assertTrue("Z".isidentifier())
        self.assertTrue("_".isidentifier())
        self.assertTrue("b0".isidentifier())
        self.assertTrue("bc".isidentifier())
        self.assertTrue("b_".isidentifier())
        self.assertTrue("µ".isidentifier())
        self.assertTrue("𝔘𝔫𝔦𝔠𝔬𝔡𝔢".isidentifier())

        self.assertFalse(" ".isidentifier())
        self.assertFalse("[".isidentifier())
        self.assertFalse("©".isidentifier())
        self.assertFalse("0".isidentifier())

    @support.cpython_only
    @support.requires_legacy_unicode_capi
    @yp_unittest.skip_not_applicable
    def test_isidentifier_legacy(self):
        import _testcapi
        u = '𝖀𝖓𝖎𝖈𝖔𝖉𝖊'
        self.assertTrue(u.isidentifier())
        with warnings_helper.check_warnings():
            warnings.simplefilter('ignore', DeprecationWarning)
            self.assertTrue(_testcapi.unicode_legacy_string(u).isidentifier())

    @yp_unittest.skip_str_unicode_db
    def test_isprintable(self):
        self.assertTrue("".isprintable())
        self.assertTrue(" ".isprintable())
        self.assertTrue("abcdefg".isprintable())
        self.assertFalse("abcdefg\n".isprintable())
        # some defined Unicode character
        self.assertTrue("\u0374".isprintable())
        # undefined character
        self.assertFalse("\u0378".isprintable())
        # single surrogate character
        self.assertFalse("\ud800".isprintable())

        self.assertTrue('\U0001F46F'.isprintable())
        self.assertFalse('\U000E0020'.isprintable())

    @yp_unittest.skip_str_case
    def test_surrogates(self):
        for s in (yp_str('a\uD800b\uDFFF'), yp_str('a\uDFFFb\uD800'),
                  yp_str('a\uD800b\uDFFFa'), yp_str('a\uDFFFb\uD800a')):
            self.assertTrue(s.islower())
            self.assertFalse(s.isupper())
            self.assertFalse(s.istitle())
        for s in (yp_str('A\uD800B\uDFFF'), yp_str('A\uDFFFB\uD800'),
                  yp_str('A\uD800B\uDFFFA'), yp_str('A\uDFFFB\uD800A')):
            self.assertFalse(s.islower())
            self.assertTrue(s.isupper())
            self.assertTrue(s.istitle())

        for meth_name in ('islower', 'isupper', 'istitle'):
            meth = getattr(yp_str, meth_name)
            for s in (yp_str('\uD800'), yp_str('\uDFFF'), yp_str('\uD800\uD800'), yp_str('\uDFFF\uDFFF')):
                self.assertFalse(meth(s), '%a.%s() is yp_False' % (s, meth_name))

        for meth_name in ('isalpha', 'isalnum', 'isdigit', 'isspace',
                          'isdecimal', 'isnumeric',
                          'isidentifier', 'isprintable'):
            meth = getattr(yp_str, meth_name)
            for s in (yp_str('\uD800'), yp_str('\uDFFF'), yp_str('\uD800\uD800'), yp_str('\uDFFF\uDFFF'),
                      yp_str('a\uD800b\uDFFF'), yp_str('a\uDFFFb\uD800'),
                      yp_str('a\uD800b\uDFFFa'), yp_str('a\uDFFFb\uD800a')):
                self.assertFalse(meth(s), '%a.%s() is yp_False' % (s, meth_name))


    @yp_unittest.skip_str_case
    def test_lower(self):
        string_tests.CommonTest.test_lower(self)
        self.assertEqual('\U00010427'.lower(), '\U0001044F')
        self.assertEqual('\U00010427\U00010427'.lower(),
                         '\U0001044F\U0001044F')
        self.assertEqual('\U00010427\U0001044F'.lower(),
                         '\U0001044F\U0001044F')
        self.assertEqual('X\U00010427x\U0001044F'.lower(),
                         'x\U0001044Fx\U0001044F')
        self.assertEqual('ﬁ'.lower(), 'ﬁ')
        self.assertEqual('\u0130'.lower(), '\u0069\u0307')
        # Special case for GREEK CAPITAL LETTER SIGMA U+03A3
        self.assertEqual('\u03a3'.lower(), '\u03c3')
        self.assertEqual('\u0345\u03a3'.lower(), '\u0345\u03c3')
        self.assertEqual('A\u0345\u03a3'.lower(), 'a\u0345\u03c2')
        self.assertEqual('A\u0345\u03a3a'.lower(), 'a\u0345\u03c3a')
        self.assertEqual('A\u0345\u03a3'.lower(), 'a\u0345\u03c2')
        self.assertEqual('A\u03a3\u0345'.lower(), 'a\u03c2\u0345')
        self.assertEqual('\u03a3\u0345 '.lower(), '\u03c3\u0345 ')
        self.assertEqual('\U0008fffe'.lower(), '\U0008fffe')
        self.assertEqual('\u2177'.lower(), '\u2177')

    @yp_unittest.skip_str_case
    def test_casefold(self):
        self.assertEqual('hello'.casefold(), 'hello')
        self.assertEqual('hELlo'.casefold(), 'hello')
        self.assertEqual('ß'.casefold(), 'ss')
        self.assertEqual('ﬁ'.casefold(), 'fi')
        self.assertEqual('\u03a3'.casefold(), '\u03c3')
        self.assertEqual('A\u0345\u03a3'.casefold(), 'a\u03b9\u03c3')
        self.assertEqual('\u00b5'.casefold(), '\u03bc')

    @yp_unittest.skip_str_case
    def test_upper(self):
        string_tests.CommonTest.test_upper(self)
        self.assertEqual('\U0001044F'.upper(), '\U00010427')
        self.assertEqual('\U0001044F\U0001044F'.upper(),
                         '\U00010427\U00010427')
        self.assertEqual('\U00010427\U0001044F'.upper(),
                         '\U00010427\U00010427')
        self.assertEqual('X\U00010427x\U0001044F'.upper(),
                         'X\U00010427X\U00010427')
        self.assertEqual('ﬁ'.upper(), 'FI')
        self.assertEqual('\u0130'.upper(), '\u0130')
        self.assertEqual('\u03a3'.upper(), '\u03a3')
        self.assertEqual('ß'.upper(), 'SS')
        self.assertEqual('\u1fd2'.upper(), '\u0399\u0308\u0300')
        self.assertEqual('\U0008fffe'.upper(), '\U0008fffe')
        self.assertEqual('\u2177'.upper(), '\u2167')

    @yp_unittest.skip_str_case
    def test_capitalize(self):
        string_tests.CommonTest.test_capitalize(self)
        self.assertEqual('\U0001044F'.capitalize(), '\U00010427')
        self.assertEqual('\U0001044F\U0001044F'.capitalize(),
                         '\U00010427\U0001044F')
        self.assertEqual('\U00010427\U0001044F'.capitalize(),
                         '\U00010427\U0001044F')
        self.assertEqual('\U0001044F\U00010427'.capitalize(),
                         '\U00010427\U0001044F')
        self.assertEqual('X\U00010427x\U0001044F'.capitalize(),
                         'X\U0001044Fx\U0001044F')
        self.assertEqual('h\u0130'.capitalize(), 'H\u0069\u0307')
        exp = '\u0399\u0308\u0300\u0069\u0307'
        self.assertEqual('\u1fd2\u0130'.capitalize(), exp)
        self.assertEqual('ﬁnnish'.capitalize(), 'Finnish')
        self.assertEqual('A\u0345\u03a3'.capitalize(), 'A\u0345\u03c2')

    @yp_unittest.skip_str_case
    def test_title(self):
        super().test_title()
        self.assertEqual('\U0001044F'.title(), '\U00010427')
        self.assertEqual('\U0001044F\U0001044F'.title(),
                         '\U00010427\U0001044F')
        self.assertEqual('\U0001044F\U0001044F \U0001044F\U0001044F'.title(),
                         '\U00010427\U0001044F \U00010427\U0001044F')
        self.assertEqual('\U00010427\U0001044F \U00010427\U0001044F'.title(),
                         '\U00010427\U0001044F \U00010427\U0001044F')
        self.assertEqual('\U0001044F\U00010427 \U0001044F\U00010427'.title(),
                         '\U00010427\U0001044F \U00010427\U0001044F')
        self.assertEqual('X\U00010427x\U0001044F X\U00010427x\U0001044F'.title(),
                         'X\U0001044Fx\U0001044F X\U0001044Fx\U0001044F')
        self.assertEqual('ﬁNNISH'.title(), 'Finnish')
        self.assertEqual('A\u03a3 \u1fa1xy'.title(), 'A\u03c2 \u1fa9xy')
        self.assertEqual('A\u03a3A'.title(), 'A\u03c3a')

    @yp_unittest.skip_str_case
    def test_swapcase(self):
        string_tests.CommonTest.test_swapcase(self)
        self.assertEqual('\U0001044F'.swapcase(), '\U00010427')
        self.assertEqual('\U00010427'.swapcase(), '\U0001044F')
        self.assertEqual('\U0001044F\U0001044F'.swapcase(),
                         '\U00010427\U00010427')
        self.assertEqual('\U00010427\U0001044F'.swapcase(),
                         '\U0001044F\U00010427')
        self.assertEqual('\U0001044F\U00010427'.swapcase(),
                         '\U00010427\U0001044F')
        self.assertEqual('X\U00010427x\U0001044F'.swapcase(),
                         'x\U0001044FX\U00010427')
        self.assertEqual('ﬁ'.swapcase(), 'FI')
        self.assertEqual('\u0130'.swapcase(), '\u0069\u0307')
        # Special case for GREEK CAPITAL LETTER SIGMA U+03A3
        self.assertEqual('\u03a3'.swapcase(), '\u03c3')
        self.assertEqual('\u0345\u03a3'.swapcase(), '\u0399\u03c3')
        self.assertEqual('A\u0345\u03a3'.swapcase(), 'a\u0399\u03c2')
        self.assertEqual('A\u0345\u03a3a'.swapcase(), 'a\u0399\u03c3A')
        self.assertEqual('A\u0345\u03a3'.swapcase(), 'a\u0399\u03c2')
        self.assertEqual('A\u03a3\u0345'.swapcase(), 'a\u03c2\u0399')
        self.assertEqual('\u03a3\u0345 '.swapcase(), '\u03c3\u0399 ')
        self.assertEqual('\u03a3'.swapcase(), '\u03c3')
        self.assertEqual('ß'.swapcase(), 'SS')
        self.assertEqual('\u1fd2'.swapcase(), '\u0399\u0308\u0300')

    @yp_unittest.skip_str_space
    def test_center(self):
        string_tests.CommonTest.test_center(self)
        self.assertEqual('x'.center(2, '\U0010FFFF'),
                         'x\U0010FFFF')
        self.assertEqual('x'.center(3, '\U0010FFFF'),
                         '\U0010FFFFx\U0010FFFF')
        self.assertEqual('x'.center(4, '\U0010FFFF'),
                         '\U0010FFFFx\U0010FFFF\U0010FFFF')

    @yp_unittest.skipUnless(sys.maxsize == 2**31 - 1, "requires 32-bit system")
    @yp_unittest.skip_str_case
    @support.cpython_only
    def test_case_operation_overflow(self):
        # Issue #22643
        size = 2**32//12 + 1
        try:
            s = "ü" * size
        except MemoryError:
            self.skipTest('no enough memory (%.0f MiB required)' % (size / 2**20))
        try:
            self.assertRaises(OverflowError, s.upper)
        finally:
            del s

    def test_contains(self):
        # Testing Unicode contains method
        self.assertIn(yp_str('a'), yp_str('abdb'))
        self.assertIn(yp_str('a'), yp_str('bdab'))
        self.assertIn(yp_str('a'), yp_str('bdaba'))
        self.assertIn(yp_str('a'), yp_str('bdba'))
        self.assertNotIn(yp_str('a'), yp_str('bdb'))
        self.assertIn(yp_str('a'), yp_str('bdba'))
        self.assertIn(yp_str('a'), yp_tuple(('a',1,None)))
        self.assertIn(yp_str('a'), yp_tuple((1,None,'a')))
        self.assertIn(yp_str('a'), yp_tuple(('a',1,None)))
        self.assertIn(yp_str('a'), yp_tuple((1,None,'a')))
        self.assertNotIn(yp_str('a'), yp_tuple(('x',1,'y')))
        self.assertNotIn(yp_str('a'), yp_tuple(('x',1,None)))
        self.assertNotIn(yp_str('abcd'), yp_str('abcxxxx'))
        self.assertIn(yp_str('ab'), yp_str('abcd'))
        self.assertIn(yp_str('ab'), yp_str('abc'))
        self.assertIn(yp_str('ab'), yp_tuple((1,None,'ab')))
        self.assertIn(yp_str(''), yp_str('abc'))
        self.assertIn(yp_str(''), yp_str(''))
        self.assertIn(yp_str(''), yp_str('abc'))
        self.assertNotIn(yp_str('\0'), yp_str('abc'))
        self.assertIn(yp_str('\0'), yp_str('\0abc'))
        self.assertIn(yp_str('\0'), yp_str('abc\0'))
        self.assertIn(yp_str('a'), yp_str('\0abc'))
        self.assertIn(yp_str('asdf'), yp_str('asdf'))
        self.assertNotIn(yp_str('asdf'), yp_str('asd'))
        self.assertNotIn(yp_str('asdf'), yp_str(''))

        self.assertRaises(TypeError, yp_str("abc").__contains__)
        # test mixed kinds
        for fill in (yp_str('a'), yp_str('\u0100'), yp_str('\U00010300')):
            fill *= 9
            for delim in ('c', '\u0102', '\U00010302'):
                self.assertNotIn(delim, fill)
                self.assertIn(delim, fill + delim)
                self.assertNotIn(delim * 2, fill)
                self.assertIn(delim * 2, fill + delim * 2)

    def test_issue18183(self):
        '\U00010000\U00100000'.lower()
        '\U00010000\U00100000'.casefold()
        '\U00010000\U00100000'.upper()
        '\U00010000\U00100000'.capitalize()
        '\U00010000\U00100000'.title()
        '\U00010000\U00100000'.swapcase()
        '\U00100000'.center(3, '\U00010000')
        '\U00100000'.ljust(3, '\U00010000')
        '\U00100000'.rjust(3, '\U00010000')

    @yp_unittest.skip_str_format
    def test_format(self):
        self.assertEqual(''.format(), '')
        self.assertEqual('a'.format(), 'a')
        self.assertEqual('ab'.format(), 'ab')
        self.assertEqual('a{{'.format(), 'a{')
        self.assertEqual('a}}'.format(), 'a}')
        self.assertEqual('{{b'.format(), '{b')
        self.assertEqual('}}b'.format(), '}b')
        self.assertEqual('a{{b'.format(), 'a{b')

        # examples from the PEP:
        import datetime
        self.assertEqual("My name is {0}".format('Fred'), "My name is Fred")
        self.assertEqual("My name is {0[name]}".format(dict(name='Fred')),
                         "My name is Fred")
        self.assertEqual("My name is {0} :-{{}}".format('Fred'),
                         "My name is Fred :-{}")

        d = datetime.date(2007, 8, 18)
        self.assertEqual("The year is {0.year}".format(d),
                         "The year is 2007")

        # classes we'll use for testing
        class C:
            def __init__(self, x=100):
                self._x = x
            def __format__(self, spec):
                return spec

        class D:
            def __init__(self, x):
                self.x = x
            def __format__(self, spec):
                return yp_str(self.x)

        # class with __str__, but no __format__
        class E:
            def __init__(self, x):
                self.x = x
            def __str__(self):
                return 'E(' + self.x + ')'

        # class with __repr__, but no __format__ or __str__
        class F:
            def __init__(self, x):
                self.x = x
            def __repr__(self):
                return 'F(' + self.x + ')'

        # class with __format__ that forwards to string, for some format_spec's
        class G:
            def __init__(self, x):
                self.x = x
            def __str__(self):
                return "string is " + self.x
            def __format__(self, format_spec):
                if format_spec == 'd':
                    return 'G(' + self.x + ')'
                return object.__format__(self, format_spec)

        class I(datetime.date):
            def __format__(self, format_spec):
                return self.strftime(format_spec)

        class J(int):
            def __format__(self, format_spec):
                return int.__format__(self * 2, format_spec)

        class M:
            def __init__(self, x):
                self.x = x
            def __repr__(self):
                return 'M(' + self.x + ')'
            __str__ = None

        class N:
            def __init__(self, x):
                self.x = x
            def __repr__(self):
                return 'N(' + self.x + ')'
            __format__ = None

        self.assertEqual(''.format(), '')
        self.assertEqual('abc'.format(), 'abc')
        self.assertEqual('{0}'.format('abc'), 'abc')
        self.assertEqual('{0:}'.format('abc'), 'abc')
#        self.assertEqual('{ 0 }'.format('abc'), 'abc')
        self.assertEqual('X{0}'.format('abc'), 'Xabc')
        self.assertEqual('{0}X'.format('abc'), 'abcX')
        self.assertEqual('X{0}Y'.format('abc'), 'XabcY')
        self.assertEqual('{1}'.format(1, 'abc'), 'abc')
        self.assertEqual('X{1}'.format(1, 'abc'), 'Xabc')
        self.assertEqual('{1}X'.format(1, 'abc'), 'abcX')
        self.assertEqual('X{1}Y'.format(1, 'abc'), 'XabcY')
        self.assertEqual('{0}'.format(-15), '-15')
        self.assertEqual('{0}{1}'.format(-15, 'abc'), '-15abc')
        self.assertEqual('{0}X{1}'.format(-15, 'abc'), '-15Xabc')
        self.assertEqual('{{'.format(), '{')
        self.assertEqual('}}'.format(), '}')
        self.assertEqual('{{}}'.format(), '{}')
        self.assertEqual('{{x}}'.format(), '{x}')
        self.assertEqual('{{{0}}}'.format(123), '{123}')
        self.assertEqual('{{{{0}}}}'.format(), '{{0}}')
        self.assertEqual('}}{{'.format(), '}{')
        self.assertEqual('}}x{{'.format(), '}x{')

        # weird field names
        self.assertEqual("{0[foo-bar]}".format({'foo-bar':'baz'}), 'baz')
        self.assertEqual("{0[foo bar]}".format({'foo bar':'baz'}), 'baz')
        self.assertEqual("{0[ ]}".format({' ':3}), '3')

        self.assertEqual('{foo._x}'.format(foo=C(20)), '20')
        self.assertEqual('{1}{0}'.format(D(10), D(20)), '2010')
        self.assertEqual('{0._x.x}'.format(C(D('abc'))), 'abc')
        self.assertEqual('{0[0]}'.format(['abc', 'def']), 'abc')
        self.assertEqual('{0[1]}'.format(['abc', 'def']), 'def')
        self.assertEqual('{0[1][0]}'.format(['abc', ['def']]), 'def')
        self.assertEqual('{0[1][0].x}'.format(['abc', [D('def')]]), 'def')

        # strings
        self.assertEqual('{0:.3s}'.format('abc'), 'abc')
        self.assertEqual('{0:.3s}'.format('ab'), 'ab')
        self.assertEqual('{0:.3s}'.format('abcdef'), 'abc')
        self.assertEqual('{0:.0s}'.format('abcdef'), '')
        self.assertEqual('{0:3.3s}'.format('abc'), 'abc')
        self.assertEqual('{0:2.3s}'.format('abc'), 'abc')
        self.assertEqual('{0:2.2s}'.format('abc'), 'ab')
        self.assertEqual('{0:3.2s}'.format('abc'), 'ab ')
        self.assertEqual('{0:x<0s}'.format('result'), 'result')
        self.assertEqual('{0:x<5s}'.format('result'), 'result')
        self.assertEqual('{0:x<6s}'.format('result'), 'result')
        self.assertEqual('{0:x<7s}'.format('result'), 'resultx')
        self.assertEqual('{0:x<8s}'.format('result'), 'resultxx')
        self.assertEqual('{0: <7s}'.format('result'), 'result ')
        self.assertEqual('{0:<7s}'.format('result'), 'result ')
        self.assertEqual('{0:>7s}'.format('result'), ' result')
        self.assertEqual('{0:>8s}'.format('result'), '  result')
        self.assertEqual('{0:^8s}'.format('result'), ' result ')
        self.assertEqual('{0:^9s}'.format('result'), ' result  ')
        self.assertEqual('{0:^10s}'.format('result'), '  result  ')
        self.assertEqual('{0:8s}'.format('result'), 'result  ')
        self.assertEqual('{0:0s}'.format('result'), 'result')
        self.assertEqual('{0:08s}'.format('result'), 'result00')
        self.assertEqual('{0:<08s}'.format('result'), 'result00')
        self.assertEqual('{0:>08s}'.format('result'), '00result')
        self.assertEqual('{0:^08s}'.format('result'), '0result0')
        self.assertEqual('{0:10000}'.format('a'), 'a' + ' ' * 9999)
        self.assertEqual('{0:10000}'.format(''), ' ' * 10000)
        self.assertEqual('{0:10000000}'.format(''), ' ' * 10000000)

        # issue 12546: use \x00 as a fill character
        self.assertEqual('{0:\x00<6s}'.format('foo'), 'foo\x00\x00\x00')
        self.assertEqual('{0:\x01<6s}'.format('foo'), 'foo\x01\x01\x01')
        self.assertEqual('{0:\x00^6s}'.format('foo'), '\x00foo\x00\x00')
        self.assertEqual('{0:^6s}'.format('foo'), ' foo  ')

        self.assertEqual('{0:\x00<6}'.format(3), '3\x00\x00\x00\x00\x00')
        self.assertEqual('{0:\x01<6}'.format(3), '3\x01\x01\x01\x01\x01')
        self.assertEqual('{0:\x00^6}'.format(3), '\x00\x003\x00\x00\x00')
        self.assertEqual('{0:<6}'.format(3), '3     ')

        self.assertEqual('{0:\x00<6}'.format(3.14), '3.14\x00\x00')
        self.assertEqual('{0:\x01<6}'.format(3.14), '3.14\x01\x01')
        self.assertEqual('{0:\x00^6}'.format(3.14), '\x003.14\x00')
        self.assertEqual('{0:^6}'.format(3.14), ' 3.14 ')

        self.assertEqual('{0:\x00<12}'.format(3+2.0j), '(3+2j)\x00\x00\x00\x00\x00\x00')
        self.assertEqual('{0:\x01<12}'.format(3+2.0j), '(3+2j)\x01\x01\x01\x01\x01\x01')
        self.assertEqual('{0:\x00^12}'.format(3+2.0j), '\x00\x00\x00(3+2j)\x00\x00\x00')
        self.assertEqual('{0:^12}'.format(3+2.0j), '   (3+2j)   ')

        # format specifiers for user defined type
        self.assertEqual('{0:abc}'.format(C()), 'abc')

        # !r, !s and !a coercions
        self.assertEqual('{0!s}'.format('Hello'), 'Hello')
        self.assertEqual('{0!s:}'.format('Hello'), 'Hello')
        self.assertEqual('{0!s:15}'.format('Hello'), 'Hello          ')
        self.assertEqual('{0!s:15s}'.format('Hello'), 'Hello          ')
        self.assertEqual('{0!r}'.format('Hello'), "'Hello'")
        self.assertEqual('{0!r:}'.format('Hello'), "'Hello'")
        self.assertEqual('{0!r}'.format(F('Hello')), 'F(Hello)')
        self.assertEqual('{0!r}'.format('\u0378'), "'\\u0378'") # nonprintable
        self.assertEqual('{0!r}'.format('\u0374'), "'\u0374'")  # printable
        self.assertEqual('{0!r}'.format(F('\u0374')), 'F(\u0374)')
        self.assertEqual('{0!a}'.format('Hello'), "'Hello'")
        self.assertEqual('{0!a}'.format('\u0378'), "'\\u0378'") # nonprintable
        self.assertEqual('{0!a}'.format('\u0374'), "'\\u0374'") # printable
        self.assertEqual('{0!a:}'.format('Hello'), "'Hello'")
        self.assertEqual('{0!a}'.format(F('Hello')), 'F(Hello)')
        self.assertEqual('{0!a}'.format(F('\u0374')), 'F(\\u0374)')

        # test fallback to object.__format__
        self.assertEqual('{0}'.format({}), '{}')
        self.assertEqual('{0}'.format([]), '[]')
        self.assertEqual('{0}'.format([1]), '[1]')

        self.assertEqual('{0:d}'.format(G('data')), 'G(data)')
        self.assertEqual('{0!s}'.format(G('data')), 'string is data')

        self.assertRaises(TypeError, '{0:^10}'.format, E('data'))
        self.assertRaises(TypeError, '{0:^10s}'.format, E('data'))
        self.assertRaises(TypeError, '{0:>15s}'.format, G('data'))

        self.assertEqual("{0:date: %Y-%m-%d}".format(I(year=2007,
                                                       month=8,
                                                       day=27)),
                         "date: 2007-08-27")

        # test deriving from a builtin type and overriding __format__
        self.assertEqual("{0}".format(J(10)), "20")


        # string format specifiers
        self.assertEqual('{0:}'.format('a'), 'a')

        # computed format specifiers
        self.assertEqual("{0:.{1}}".format('hello world', 5), 'hello')
        self.assertEqual("{0:.{1}s}".format('hello world', 5), 'hello')
        self.assertEqual("{0:.{precision}s}".format('hello world', precision=5), 'hello')
        self.assertEqual("{0:{width}.{precision}s}".format('hello world', width=10, precision=5), 'hello     ')
        self.assertEqual("{0:{width}.{precision}s}".format('hello world', width='10', precision='5'), 'hello     ')

        # test various errors
        self.assertRaises(ValueError, '{'.format)
        self.assertRaises(ValueError, '}'.format)
        self.assertRaises(ValueError, 'a{'.format)
        self.assertRaises(ValueError, 'a}'.format)
        self.assertRaises(ValueError, '{a'.format)
        self.assertRaises(ValueError, '}a'.format)
        self.assertRaises(IndexError, '{0}'.format)
        self.assertRaises(IndexError, '{1}'.format, 'abc')
        self.assertRaises(KeyError,   '{x}'.format)
        self.assertRaises(ValueError, "}{".format)
        self.assertRaises(ValueError, "abc{0:{}".format)
        self.assertRaises(ValueError, "{0".format)
        self.assertRaises(IndexError, "{0.}".format)
        self.assertRaises(ValueError, "{0.}".format, 0)
        self.assertRaises(ValueError, "{0[}".format)
        self.assertRaises(ValueError, "{0[}".format, [])
        self.assertRaises(KeyError,   "{0]}".format)
        self.assertRaises(ValueError, "{0.[]}".format, 0)
        self.assertRaises(ValueError, "{0..foo}".format, 0)
        self.assertRaises(ValueError, "{0[0}".format, 0)
        self.assertRaises(ValueError, "{0[0:foo}".format, 0)
        self.assertRaises(KeyError,   "{c]}".format)
        self.assertRaises(ValueError, "{{ {{{0}}".format, 0)
        self.assertRaises(ValueError, "{0}}".format, 0)
        self.assertRaises(KeyError,   "{foo}".format, bar=3)
        self.assertRaises(ValueError, "{0!x}".format, 3)
        self.assertRaises(ValueError, "{0!}".format, 0)
        self.assertRaises(ValueError, "{0!rs}".format, 0)
        self.assertRaises(ValueError, "{!}".format)
        self.assertRaises(IndexError, "{:}".format)
        self.assertRaises(IndexError, "{:s}".format)
        self.assertRaises(IndexError, "{}".format)
        big = "23098475029384702983476098230754973209482573"
        self.assertRaises(ValueError, ("{" + big + "}").format)
        self.assertRaises(ValueError, ("{[" + big + "]}").format, [0])

        # issue 6089
        self.assertRaises(ValueError, "{0[0]x}".format, [None])
        self.assertRaises(ValueError, "{0[0](10)}".format, [None])

        # can't have a replacement on the field name portion
        self.assertRaises(TypeError, '{0[{1}]}'.format, 'abcdefg', 4)

        # exceed maximum recursion depth
        self.assertRaises(ValueError, "{0:{1:{2}}}".format, 'abc', 's', '')
        self.assertRaises(ValueError, "{0:{1:{2:{3:{4:{5:{6}}}}}}}".format,
                          0, 1, 2, 3, 4, 5, 6, 7)

        # string format spec errors
        sign_msg = "Sign not allowed in string format specifier"
        self.assertRaisesRegex(ValueError, sign_msg, "{0:-s}".format, '')
        self.assertRaisesRegex(ValueError, sign_msg, format, "", "-")
        space_msg = "Space not allowed in string format specifier"
        self.assertRaisesRegex(ValueError, space_msg, "{: }".format, '')
        self.assertRaises(ValueError, "{0:=s}".format, '')

        # Alternate formatting is not supported
        self.assertRaises(ValueError, format, '', '#')
        self.assertRaises(ValueError, format, '', '#20')

        # Non-ASCII
        self.assertEqual("{0:s}{1:s}".format("ABC", "\u0410\u0411\u0412"),
                         'ABC\u0410\u0411\u0412')
        self.assertEqual("{0:.3s}".format("ABC\u0410\u0411\u0412"),
                         'ABC')
        self.assertEqual("{0:.0s}".format("ABC\u0410\u0411\u0412"),
                         '')

        self.assertEqual("{[{}]}".format({"{}": 5}), "5")
        self.assertEqual("{[{}]}".format({"{}" : "a"}), "a")
        self.assertEqual("{[{]}".format({"{" : "a"}), "a")
        self.assertEqual("{[}]}".format({"}" : "a"}), "a")
        self.assertEqual("{[[]}".format({"[" : "a"}), "a")
        self.assertEqual("{[!]}".format({"!" : "a"}), "a")
        self.assertRaises(ValueError, "{a{}b}".format, 42)
        self.assertRaises(ValueError, "{a{b}".format, 42)
        self.assertRaises(ValueError, "{[}".format, 42)

        self.assertEqual("0x{:0{:d}X}".format(0x0,16), "0x0000000000000000")

        # Blocking fallback
        m = M('data')
        self.assertEqual("{!r}".format(m), 'M(data)')
        self.assertRaises(TypeError, "{!s}".format, m)
        self.assertRaises(TypeError, "{}".format, m)
        n = N('data')
        self.assertEqual("{!r}".format(n), 'N(data)')
        self.assertEqual("{!s}".format(n), 'N(data)')
        self.assertRaises(TypeError, "{}".format, n)

    @yp_unittest.skip_str_format
    def test_format_map(self):
        self.assertEqual(''.format_map({}), '')
        self.assertEqual('a'.format_map({}), 'a')
        self.assertEqual('ab'.format_map({}), 'ab')
        self.assertEqual('a{{'.format_map({}), 'a{')
        self.assertEqual('a}}'.format_map({}), 'a}')
        self.assertEqual('{{b'.format_map({}), '{b')
        self.assertEqual('}}b'.format_map({}), '}b')
        self.assertEqual('a{{b'.format_map({}), 'a{b')

        # using mappings
        class Mapping(dict):
            def __missing__(self, key):
                return key
        self.assertEqual('{hello}'.format_map(Mapping()), 'hello')
        self.assertEqual('{a} {world}'.format_map(Mapping(a='hello')), 'hello world')

        class InternalMapping:
            def __init__(self):
                self.mapping = {'a': 'hello'}
            def __getitem__(self, key):
                return self.mapping[key]
        self.assertEqual('{a}'.format_map(InternalMapping()), 'hello')


        class C:
            def __init__(self, x=100):
                self._x = x
            def __format__(self, spec):
                return spec
        self.assertEqual('{foo._x}'.format_map({'foo': C(20)}), '20')

        # test various errors
        self.assertRaises(TypeError, ''.format_map)
        self.assertRaises(TypeError, 'a'.format_map)

        self.assertRaises(ValueError, '{'.format_map, {})
        self.assertRaises(ValueError, '}'.format_map, {})
        self.assertRaises(ValueError, 'a{'.format_map, {})
        self.assertRaises(ValueError, 'a}'.format_map, {})
        self.assertRaises(ValueError, '{a'.format_map, {})
        self.assertRaises(ValueError, '}a'.format_map, {})

        # issue #12579: can't supply positional params to format_map
        self.assertRaises(ValueError, '{}'.format_map, {'a' : 2})
        self.assertRaises(ValueError, '{}'.format_map, 'a')
        self.assertRaises(ValueError, '{a} {}'.format_map, {"a" : 2, "b" : 1})

        class BadMapping:
            def __getitem__(self, key):
                return 1/0
        self.assertRaises(KeyError, '{a}'.format_map, {})
        self.assertRaises(TypeError, '{a}'.format_map, [])
        self.assertRaises(ZeroDivisionError, '{a}'.format_map, BadMapping())

    @yp_unittest.skip_str_format
    def test_format_huge_precision(self):
        format_string = ".{}f".format(sys.maxsize + 1)
        with self.assertRaises(ValueError):
            result = format(2.34, format_string)

    @yp_unittest.skip_str_format
    def test_format_huge_width(self):
        format_string = "{}f".format(sys.maxsize + 1)
        with self.assertRaises(ValueError):
            result = format(2.34, format_string)

    @yp_unittest.skip_str_format
    def test_format_huge_item_number(self):
        format_string = "{{{}:.6f}}".format(sys.maxsize + 1)
        with self.assertRaises(ValueError):
            result = format_string.format(2.34)

    @yp_unittest.skip_str_format
    def test_format_auto_numbering(self):
        class C:
            def __init__(self, x=100):
                self._x = x
            def __format__(self, spec):
                return spec

        self.assertEqual('{}'.format(10), '10')
        self.assertEqual('{:5}'.format('s'), 's    ')
        self.assertEqual('{!r}'.format('s'), "'s'")
        self.assertEqual('{._x}'.format(C(10)), '10')
        self.assertEqual('{[1]}'.format([1, 2]), '2')
        self.assertEqual('{[a]}'.format({'a':4, 'b':2}), '4')
        self.assertEqual('a{}b{}c'.format(0, 1), 'a0b1c')

        self.assertEqual('a{:{}}b'.format('x', '^10'), 'a    x     b')
        self.assertEqual('a{:{}x}b'.format(20, '#'), 'a0x14b')

        # can't mix and match numbering and auto-numbering
        self.assertRaises(ValueError, '{}{1}'.format, 1, 2)
        self.assertRaises(ValueError, '{1}{}'.format, 1, 2)
        self.assertRaises(ValueError, '{:{1}}'.format, 1, 2)
        self.assertRaises(ValueError, '{0:{}}'.format, 1, 2)

        # can mix and match auto-numbering and named
        self.assertEqual('{f}{}'.format(4, f='test'), 'test4')
        self.assertEqual('{}{f}'.format(4, f='test'), '4test')
        self.assertEqual('{:{f}}{g}{}'.format(1, 3, g='g', f=2), ' 1g3')
        self.assertEqual('{f:{}}{}{g}'.format(2, 4, f=1, g='g'), ' 14g')

    @yp_unittest.skip_str_printf
    def test_formatting(self):
        string_tests.MixinStrUnicodeUserStringTest.test_formatting(self)
        # Testing Unicode formatting strings...
        self.assertEqual("%s, %s" % ("abc", "abc"), 'abc, abc')
        self.assertEqual("%s, %s, %i, %f, %5.2f" % ("abc", "abc", 1, 2, 3), 'abc, abc, 1, 2.000000,  3.00')
        self.assertEqual("%s, %s, %i, %f, %5.2f" % ("abc", "abc", 1, -2, 3), 'abc, abc, 1, -2.000000,  3.00')
        self.assertEqual("%s, %s, %i, %f, %5.2f" % ("abc", "abc", -1, -2, 3.5), 'abc, abc, -1, -2.000000,  3.50')
        self.assertEqual("%s, %s, %i, %f, %5.2f" % ("abc", "abc", -1, -2, 3.57), 'abc, abc, -1, -2.000000,  3.57')
        self.assertEqual("%s, %s, %i, %f, %5.2f" % ("abc", "abc", -1, -2, 1003.57), 'abc, abc, -1, -2.000000, 1003.57')
        if not sys.platform.startswith('java'):
            self.assertEqual("%r, %r" % (b"abc", "abc"), "b'abc', 'abc'")
            self.assertEqual("%r" % ("\u1234",), "'\u1234'")
            self.assertEqual("%a" % ("\u1234",), "'\\u1234'")
        self.assertEqual("%(x)s, %(y)s" % {'x':"abc", 'y':"def"}, 'abc, def')
        self.assertEqual("%(x)s, %(\xfc)s" % {'x':"abc", '\xfc':"def"}, 'abc, def')

        self.assertEqual('%c' % 0x1234, '\u1234')
        self.assertEqual('%c' % 0x21483, '\U00021483')
        self.assertRaises(OverflowError, "%c".__mod__, (0x110000,))
        self.assertEqual('%c' % '\U00021483', '\U00021483')
        self.assertRaises(TypeError, "%c".__mod__, "aa")
        self.assertRaises(ValueError, "%.1\u1032f".__mod__, (1.0/3))
        self.assertRaises(TypeError, "%i".__mod__, "aa")

        # formatting jobs delegated from the string implementation:
        self.assertEqual('...%(foo)s...' % {'foo':"abc"}, '...abc...')
        self.assertEqual('...%(foo)s...' % {'foo':"abc"}, '...abc...')
        self.assertEqual('...%(foo)s...' % {'foo':"abc"}, '...abc...')
        self.assertEqual('...%(foo)s...' % {'foo':"abc"}, '...abc...')
        self.assertEqual('...%(foo)s...' % {'foo':"abc",'def':123},  '...abc...')
        self.assertEqual('...%(foo)s...' % {'foo':"abc",'def':123}, '...abc...')
        self.assertEqual('...%s...%s...%s...%s...' % (1,2,3,"abc"), '...1...2...3...abc...')
        self.assertEqual('...%%...%%s...%s...%s...%s...%s...' % (1,2,3,"abc"), '...%...%s...1...2...3...abc...')
        self.assertEqual('...%s...' % "abc", '...abc...')
        self.assertEqual('%*s' % (5,'abc',), '  abc')
        self.assertEqual('%*s' % (-5,'abc',), 'abc  ')
        self.assertEqual('%*.*s' % (5,2,'abc',), '   ab')
        self.assertEqual('%*.*s' % (5,3,'abc',), '  abc')
        self.assertEqual('%i %*.*s' % (10, 5,3,'abc',), '10   abc')
        self.assertEqual('%i%s %*.*s' % (10, 3, 5, 3, 'abc',), '103   abc')
        self.assertEqual('%c' % 'a', 'a')
        class Wrapper:
            def __str__(self):
                return '\u1234'
        self.assertEqual('%s' % Wrapper(), '\u1234')

        # issue 3382
        NAN = float('nan')
        INF = float('inf')
        self.assertEqual('%f' % NAN, 'nan')
        self.assertEqual('%F' % NAN, 'NAN')
        self.assertEqual('%f' % INF, 'inf')
        self.assertEqual('%F' % INF, 'INF')

        # PEP 393
        self.assertEqual('%.1s' % "a\xe9\u20ac", 'a')
        self.assertEqual('%.2s' % "a\xe9\u20ac", 'a\xe9')

        #issue 19995
        class PseudoInt:
            def __init__(self, value):
                self.value = int(value)
            def __int__(self):
                return self.value
            def __index__(self):
                return self.value
        class PseudoFloat:
            def __init__(self, value):
                self.value = float(value)
            def __int__(self):
                return int(self.value)
        pi = PseudoFloat(3.1415)
        letter_m = PseudoInt(109)
        self.assertEqual('%x' % 42, '2a')
        self.assertEqual('%X' % 15, 'F')
        self.assertEqual('%o' % 9, '11')
        self.assertEqual('%c' % 109, 'm')
        self.assertEqual('%x' % letter_m, '6d')
        self.assertEqual('%X' % letter_m, '6D')
        self.assertEqual('%o' % letter_m, '155')
        self.assertEqual('%c' % letter_m, 'm')
        self.assertRaisesRegex(TypeError, '%x format: an integer is required, not float', operator.mod, '%x', 3.14),
        self.assertRaisesRegex(TypeError, '%X format: an integer is required, not float', operator.mod, '%X', 2.11),
        self.assertRaisesRegex(TypeError, '%o format: an integer is required, not float', operator.mod, '%o', 1.79),
        self.assertRaisesRegex(TypeError, '%x format: an integer is required, not PseudoFloat', operator.mod, '%x', pi),
        self.assertRaises(TypeError, operator.mod, '%c', pi),

    @yp_unittest.skip_str_printf
    def test_formatting_with_enum(self):
        # issue18780
        import enum
        class Float(float, enum.Enum):
            PI = 3.1415926
        class Int(enum.IntEnum):
            IDES = 15
        class Str(yp_str, enum.Enum):
            ABC = 'abc'
        # Testing Unicode formatting strings...
        self.assertEqual("%s, %s" % (Str.ABC, Str.ABC),
                         'Str.ABC, Str.ABC')
        self.assertEqual("%s, %s, %d, %i, %u, %f, %5.2f" %
                        (Str.ABC, Str.ABC,
                         Int.IDES, Int.IDES, Int.IDES,
                         Float.PI, Float.PI),
                         'Str.ABC, Str.ABC, 15, 15, 15, 3.141593,  3.14')

        # formatting jobs delegated from the string implementation:
        self.assertEqual('...%(foo)s...' % {'foo':Str.ABC},
                         '...Str.ABC...')
        self.assertEqual('...%(foo)s...' % {'foo':Int.IDES},
                         '...Int.IDES...')
        self.assertEqual('...%(foo)i...' % {'foo':Int.IDES},
                         '...15...')
        self.assertEqual('...%(foo)d...' % {'foo':Int.IDES},
                         '...15...')
        self.assertEqual('...%(foo)u...' % {'foo':Int.IDES, 'def':Float.PI},
                         '...15...')
        self.assertEqual('...%(foo)f...' % {'foo':Float.PI,'def':123},
                         '...3.141593...')

    @yp_unittest.skip_str_format
    def test_formatting_huge_precision(self):
        format_string = "%.{}f".format(sys.maxsize + 1)
        with self.assertRaises(ValueError):
            result = format_string % 2.34

    @yp_unittest.skip_user_defined_types
    def test_issue28598_strsubclass_rhs(self):
        # A subclass of str with an __rmod__ method should be able to hook
        # into the % operator
        class SubclassedStr(yp_str):
            def __rmod__(self, other):
                return 'Success, self.__rmod__({!r}) was called'.format(other)
        self.assertEqual('lhs %% %r' % SubclassedStr('rhs'),
                         "Success, self.__rmod__('lhs %% %r') was called")

    @support.cpython_only
    @yp_unittest.skip_str_format
    def test_formatting_huge_precision_c_limits(self):
        from _testcapi import INT_MAX
        format_string = "%.{}f".format(INT_MAX + 1)
        with self.assertRaises(ValueError):
            result = format_string % 2.34

    @yp_unittest.skip_str_format
    def test_formatting_huge_width(self):
        format_string = "%{}f".format(sys.maxsize + 1)
        with self.assertRaises(ValueError):
            result = format_string % 2.34

    def test_startswith_endswith_errors(self):
        for meth in ('foo'.startswith, 'foo'.endswith):
            with self.assertRaises(TypeError) as cm:
                meth(['f'])

            # TODO(skip_exception_messages) Not applicable to nohtyP
            # exc = yp_str(cm.exception)
            # self.assertIn('str', exc)
            # self.assertIn('tuple', exc)

    @support.run_with_locale('LC_ALL', 'de_DE', 'fr_FR')
    @yp_unittest.skip_str_printf
    def test_format_float(self):
        # should not format with a comma, but always with C locale
        self.assertEqual('1.0', '%.1f' % 1.0)

    @yp_unittest.skip_user_defined_types
    def test_constructor(self):
        # unicode(obj) tests (this maps to PyObject_Unicode() at C level)

        self.assertEqual(
            yp_str('unicode remains unicode'),
            'unicode remains unicode'
        )

        for text in ('ascii', '\xe9', '\u20ac', '\U0010FFFF'):
            subclass = StrSubclass(text)
            self.assertEqual(yp_str(subclass), text)
            self.assertEqual(yp_len(subclass), yp_len(text))
            if text == 'ascii':
                self.assertEqual(subclass.encode('ascii'), b'ascii')
                self.assertEqual(subclass.encode('utf-8'), b'ascii')

        self.assertEqual(
            yp_str('strings are converted to unicode'),
            'strings are converted to unicode'
        )

        class StringCompat:
            def __init__(self, x):
                self.x = x
            def __str__(self):
                return self.x

        self.assertEqual(
            yp_str(StringCompat('__str__ compatible objects are recognized')),
            '__str__ compatible objects are recognized'
        )

        # unicode(obj) is compatible to yp_str():

        o = StringCompat('unicode(obj) is compatible to yp_str()')
        self.assertEqual(yp_str(o), 'unicode(obj) is compatible to yp_str()')
        self.assertEqual(yp_str(o), 'unicode(obj) is compatible to yp_str()')

        for obj in (123, 123.45, 123):
            self.assertEqual(yp_str(obj), yp_str(yp_str(obj)))

        # unicode(obj, encoding, error) tests (this maps to
        # PyUnicode_FromEncodedObject() at C level)

        if not sys.platform.startswith('java'):
            self.assertRaises(
                TypeError,
                yp_str,
                'decoding unicode is not supported',
                'utf-8',
                'strict'
            )

        self.assertEqual(
            yp_str(b'strings are decoded to unicode', 'utf-8', 'strict'),
            'strings are decoded to unicode'
        )

        if not sys.platform.startswith('java'):
            self.assertEqual(
                yp_str(
                    memoryview(b'character buffers are decoded to unicode'),
                    'utf-8',
                    'strict'
                ),
                'character buffers are decoded to unicode'
            )

        self.assertRaises(TypeError, yp_str, 42, 42, 42)

    def test_constructor_keyword_args(self):
        """Pass various keyword argument combinations to the constructor."""
        # The object argument can be passed as a keyword.
        self.assertEqual(yp_str(object='foo'), 'foo')
        self.assertEqual(yp_str(object=b'foo', encoding='utf-8'), 'foo')
        # The errors argument without encoding triggers "decode" mode.
        self.assertEqual(yp_str(b'foo', errors='strict'), 'foo')  # not "b'foo'"
        self.assertEqual(yp_str(object=b'foo', errors='strict'), 'foo')

    def test_constructor_defaults(self):
        """Check the constructor argument defaults."""
        # Unlike Python, the object argument always defaults to '', never b''.
        self.assertEqual(yp_str(), '')
        self.assertRaises(TypeError, yp_str, errors='strict')
        utf8_cent = '¢'.encode('utf-8')
        # The encoding argument defaults to utf-8.
        self.assertEqual(yp_str(utf8_cent, errors='strict'), '¢')

    @yp_unittest.skip_str_codecs
    def test_constructor_defaults_to_strict(self):
        # The errors argument defaults to strict.
        utf8_cent = '¢'.encode('utf-8')
        self.assertRaises(UnicodeDecodeError, yp_str, utf8_cent, encoding='ascii')

    @yp_unittest.skip_str_codecs
    def test_codecs_utf7(self):
        utfTests = ((yp_str(x), yp_bytes(y)) for (x, y) in [
            ('A\u2262\u0391.', b'A+ImIDkQ.'),             # RFC2152 example
            ('Hi Mom -\u263a-!', b'Hi Mom -+Jjo--!'),     # RFC2152 example
            ('\u65E5\u672C\u8A9E', b'+ZeVnLIqe-'),        # RFC2152 example
            ('Item 3 is \u00a31.', b'Item 3 is +AKM-1.'), # RFC2152 example
            ('+', b'+-'),
            ('+-', b'+--'),
            ('+?', b'+-?'),
            (r'\?', b'+AFw?'),
            ('+?', b'+-?'),
            (r'\\?', b'+AFwAXA?'),
            (r'\\\?', b'+AFwAXABc?'),
            (r'++--', b'+-+---'),
            ('\U000abcde', b'+2m/c3g-'),                  # surrogate pairs
            ('/', b'/'),
        ])

        for (x, y) in utfTests:
            self.assertEqual(x.encode('utf-7'), y)

        # Unpaired surrogates are passed through
        self.assertEqual('\uD801'.encode('utf-7'), b'+2AE-')
        self.assertEqual('\uD801x'.encode('utf-7'), b'+2AE-x')
        self.assertEqual('\uDC01'.encode('utf-7'), b'+3AE-')
        self.assertEqual('\uDC01x'.encode('utf-7'), b'+3AE-x')
        self.assertEqual(b'+2AE-'.decode('utf-7'), '\uD801')
        self.assertEqual(b'+2AE-x'.decode('utf-7'), '\uD801x')
        self.assertEqual(b'+3AE-'.decode('utf-7'), '\uDC01')
        self.assertEqual(b'+3AE-x'.decode('utf-7'), '\uDC01x')

        self.assertEqual('\uD801\U000abcde'.encode('utf-7'), b'+2AHab9ze-')
        self.assertEqual(b'+2AHab9ze-'.decode('utf-7'), '\uD801\U000abcde')

        # Issue #2242: crash on some Windows/MSVC versions
        self.assertEqual(b'+\xc1'.decode('utf-7', 'ignore'), '')

        # Direct encoded characters
        set_d = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789'(),-./:?"
        # Optional direct characters
        set_o = '!"#$%&*;<=>@[]^_`{|}'
        for c in set_d:
            self.assertEqual(c.encode('utf7'), c.encode('ascii'))
            self.assertEqual(c.encode('ascii').decode('utf7'), c)
        for c in set_o:
            self.assertEqual(c.encode('ascii').decode('utf7'), c)

        with self.assertRaisesRegex(UnicodeDecodeError,
                                    'ill-formed sequence'):
            b'+@'.decode('utf-7')

    def test_codecs_utf8(self):
        self.assertEqual(yp_str('').encode('utf-8'), b'')
        self.assertEqual(yp_str('\u20ac').encode('utf-8'), b'\xe2\x82\xac')
        self.assertEqual(yp_str('\U00010002').encode('utf-8'), b'\xf0\x90\x80\x82')
        self.assertEqual(yp_str('\U00023456').encode('utf-8'), b'\xf0\xa3\x91\x96')
        self.assertEqual(yp_str('\ud800').encode('utf-8', 'surrogatepass'), b'\xed\xa0\x80')
        self.assertEqual(yp_str('\udc00').encode('utf-8', 'surrogatepass'), b'\xed\xb0\x80')
        self.assertEqual(yp_str('\U00010002'*10).encode('utf-8'),
                         b'\xf0\x90\x80\x82'*10)
        self.assertEqual(yp_str(
            '\u6b63\u78ba\u306b\u8a00\u3046\u3068\u7ffb\u8a33\u306f'
            '\u3055\u308c\u3066\u3044\u307e\u305b\u3093\u3002\u4e00'
            '\u90e8\u306f\u30c9\u30a4\u30c4\u8a9e\u3067\u3059\u304c'
            '\u3001\u3042\u3068\u306f\u3067\u305f\u3089\u3081\u3067'
            '\u3059\u3002\u5b9f\u969b\u306b\u306f\u300cWenn ist das'
            ' Nunstuck git und').encode('utf-8'),
            b'\xe6\xad\xa3\xe7\xa2\xba\xe3\x81\xab\xe8\xa8\x80\xe3\x81'
            b'\x86\xe3\x81\xa8\xe7\xbf\xbb\xe8\xa8\xb3\xe3\x81\xaf\xe3'
            b'\x81\x95\xe3\x82\x8c\xe3\x81\xa6\xe3\x81\x84\xe3\x81\xbe'
            b'\xe3\x81\x9b\xe3\x82\x93\xe3\x80\x82\xe4\xb8\x80\xe9\x83'
            b'\xa8\xe3\x81\xaf\xe3\x83\x89\xe3\x82\xa4\xe3\x83\x84\xe8'
            b'\xaa\x9e\xe3\x81\xa7\xe3\x81\x99\xe3\x81\x8c\xe3\x80\x81'
            b'\xe3\x81\x82\xe3\x81\xa8\xe3\x81\xaf\xe3\x81\xa7\xe3\x81'
            b'\x9f\xe3\x82\x89\xe3\x82\x81\xe3\x81\xa7\xe3\x81\x99\xe3'
            b'\x80\x82\xe5\xae\x9f\xe9\x9a\x9b\xe3\x81\xab\xe3\x81\xaf'
            b'\xe3\x80\x8cWenn ist das Nunstuck git und'
        )

        # UTF-8 specific decoding tests
        self.assertEqual(yp_str(b'\xf0\xa3\x91\x96', 'utf-8'), '\U00023456' )
        self.assertEqual(yp_str(b'\xf0\x90\x80\x82', 'utf-8'), '\U00010002' )
        self.assertEqual(yp_str(b'\xe2\x82\xac', 'utf-8'), '\u20ac' )

        # Other possible utf-8 test cases:
        # * strict decoding testing for all of the
        #   UTF8_ERROR cases in PyUnicode_DecodeUTF8

    def test_utf8_decode_valid_sequences(self):
        sequences = [
            # single byte
            (b'\x00', '\x00'), (b'a', 'a'), (b'\x7f', '\x7f'),
            # 2 bytes
            (b'\xc2\x80', '\x80'), (b'\xdf\xbf', '\u07ff'),
            # 3 bytes
            (b'\xe0\xa0\x80', '\u0800'), (b'\xed\x9f\xbf', '\ud7ff'),
            (b'\xee\x80\x80', '\uE000'), (b'\xef\xbf\xbf', '\uffff'),
            # 4 bytes
            (b'\xF0\x90\x80\x80', '\U00010000'),
            (b'\xf4\x8f\xbf\xbf', '\U0010FFFF')
        ]
        for seq, res in sequences:
            self.assertEqual(yp_bytes(seq).decode('utf-8'), res)


    def test_utf8_decode_invalid_sequences(self):
        # continuation bytes in a sequence of 2, 3, or 4 bytes
        continuation_bytes = [yp_bytes([x]) for x in range(0x80, 0xC0)]
        # start bytes of a 2-byte sequence equivalent to code points < 0x7F
        invalid_2B_seq_start_bytes = [yp_bytes([x]) for x in range(0xC0, 0xC2)]
        # start bytes of a 4-byte sequence equivalent to code points > 0x10FFFF
        invalid_4B_seq_start_bytes = [yp_bytes([x]) for x in range(0xF5, 0xF8)]
        invalid_start_bytes = (
            continuation_bytes + invalid_2B_seq_start_bytes +
            invalid_4B_seq_start_bytes + [yp_bytes([x]) for x in range(0xF7, 0x100)]
        )

        for byte in invalid_start_bytes:
            self.assertRaises(UnicodeDecodeError, byte.decode, 'utf-8')

        for sb in invalid_2B_seq_start_bytes:
            for cb in continuation_bytes:
                self.assertRaises(UnicodeDecodeError, (sb+cb).decode, 'utf-8')

        for sb in invalid_4B_seq_start_bytes:
            for cb1 in continuation_bytes[:3]:
                for cb3 in continuation_bytes[:3]:
                    self.assertRaises(UnicodeDecodeError,
                                      (sb+cb1+b'\x80'+cb3).decode, 'utf-8')

        for cb in [yp_bytes([x]) for x in range(0x80, 0xA0)]:
            self.assertRaises(UnicodeDecodeError,
                              (b'\xE0'+cb+b'\x80').decode, 'utf-8')
            self.assertRaises(UnicodeDecodeError,
                              (b'\xE0'+cb+b'\xBF').decode, 'utf-8')
        # surrogates
        for cb in [yp_bytes([x]) for x in range(0xA0, 0xC0)]:
            self.assertRaises(UnicodeDecodeError,
                              (b'\xED'+cb+b'\x80').decode, 'utf-8')
            self.assertRaises(UnicodeDecodeError,
                              (b'\xED'+cb+b'\xBF').decode, 'utf-8')
        for cb in [yp_bytes([x]) for x in range(0x80, 0x90)]:
            self.assertRaises(UnicodeDecodeError,
                              (b'\xF0'+cb+b'\x80\x80').decode, 'utf-8')
            self.assertRaises(UnicodeDecodeError,
                              (b'\xF0'+cb+b'\xBF\xBF').decode, 'utf-8')
        for cb in [yp_bytes([x]) for x in range(0x90, 0xC0)]:
            self.assertRaises(UnicodeDecodeError,
                              (b'\xF4'+cb+b'\x80\x80').decode, 'utf-8')
            self.assertRaises(UnicodeDecodeError,
                              (b'\xF4'+cb+b'\xBF\xBF').decode, 'utf-8')

    def test_issue8271(self):
        # Issue #8271: during the decoding of an invalid UTF-8 byte sequence,
        # only the start byte and the continuation byte(s) are now considered
        # invalid, instead of the number of bytes specified by the start byte.
        # See https://www.unicode.org/versions/Unicode5.2.0/ch03.pdf (page 95,
        # table 3-8, Row 2) for more information about the algorithm used.
        FFFD = '\ufffd'
        sequences = [
            # invalid start bytes
            (b'\x80', FFFD), # continuation byte
            (b'\x80\x80', FFFD*2), # 2 continuation bytes
            (b'\xc0', FFFD),
            (b'\xc0\xc0', FFFD*2),
            (b'\xc1', FFFD),
            (b'\xc1\xc0', FFFD*2),
            (b'\xc0\xc1', FFFD*2),
            # with start byte of a 2-byte sequence
            (b'\xc2', FFFD), # only the start byte
            (b'\xc2\xc2', FFFD*2), # 2 start bytes
            (b'\xc2\xc2\xc2', FFFD*3), # 3 start bytes
            (b'\xc2\x41', FFFD+'A'), # invalid continuation byte
            # with start byte of a 3-byte sequence
            (b'\xe1', FFFD), # only the start byte
            (b'\xe1\xe1', FFFD*2), # 2 start bytes
            (b'\xe1\xe1\xe1', FFFD*3), # 3 start bytes
            (b'\xe1\xe1\xe1\xe1', FFFD*4), # 4 start bytes
            (b'\xe1\x80', FFFD), # only 1 continuation byte
            (b'\xe1\x41', FFFD+'A'), # invalid continuation byte
            (b'\xe1\x41\x80', FFFD+'A'+FFFD), # invalid cb followed by valid cb
            (b'\xe1\x41\x41', FFFD+'AA'), # 2 invalid continuation bytes
            (b'\xe1\x80\x41', FFFD+'A'), # only 1 valid continuation byte
            (b'\xe1\x80\xe1\x41', FFFD*2+'A'), # 1 valid and the other invalid
            (b'\xe1\x41\xe1\x80', FFFD+'A'+FFFD), # 1 invalid and the other valid
            # with start byte of a 4-byte sequence
            (b'\xf1', FFFD), # only the start byte
            (b'\xf1\xf1', FFFD*2), # 2 start bytes
            (b'\xf1\xf1\xf1', FFFD*3), # 3 start bytes
            (b'\xf1\xf1\xf1\xf1', FFFD*4), # 4 start bytes
            (b'\xf1\xf1\xf1\xf1\xf1', FFFD*5), # 5 start bytes
            (b'\xf1\x80', FFFD), # only 1 continuation bytes
            (b'\xf1\x80\x80', FFFD), # only 2 continuation bytes
            (b'\xf1\x80\x41', FFFD+'A'), # 1 valid cb and 1 invalid
            (b'\xf1\x80\x41\x41', FFFD+'AA'), # 1 valid cb and 1 invalid
            (b'\xf1\x80\x80\x41', FFFD+'A'), # 2 valid cb and 1 invalid
            (b'\xf1\x41\x80', FFFD+'A'+FFFD), # 1 invalid cv and 1 valid
            (b'\xf1\x41\x80\x80', FFFD+'A'+FFFD*2), # 1 invalid cb and 2 invalid
            (b'\xf1\x41\x80\x41', FFFD+'A'+FFFD+'A'), # 2 invalid cb and 1 invalid
            (b'\xf1\x41\x41\x80', FFFD+'AA'+FFFD), # 1 valid cb and 1 invalid
            (b'\xf1\x41\xf1\x80', FFFD+'A'+FFFD),
            (b'\xf1\x41\x80\xf1', FFFD+'A'+FFFD*2),
            (b'\xf1\xf1\x80\x41', FFFD*2+'A'),
            (b'\xf1\x41\xf1\xf1', FFFD+'A'+FFFD*2),
            # with invalid start byte of a 4-byte sequence (rfc2279)
            (b'\xf5', FFFD), # only the start byte
            (b'\xf5\xf5', FFFD*2), # 2 start bytes
            (b'\xf5\x80', FFFD*2), # only 1 continuation byte
            (b'\xf5\x80\x80', FFFD*3), # only 2 continuation byte
            (b'\xf5\x80\x80\x80', FFFD*4), # 3 continuation bytes
            (b'\xf5\x80\x41', FFFD*2+'A'), #  1 valid cb and 1 invalid
            (b'\xf5\x80\x41\xf5', FFFD*2+'A'+FFFD),
            (b'\xf5\x41\x80\x80\x41', FFFD+'A'+FFFD*2+'A'),
            # with invalid start byte of a 5-byte sequence (rfc2279)
            (b'\xf8', FFFD), # only the start byte
            (b'\xf8\xf8', FFFD*2), # 2 start bytes
            (b'\xf8\x80', FFFD*2), # only one continuation byte
            (b'\xf8\x80\x41', FFFD*2 + 'A'), # 1 valid cb and 1 invalid
            (b'\xf8\x80\x80\x80\x80', FFFD*5), # invalid 5 bytes seq with 5 bytes
            # with invalid start byte of a 6-byte sequence (rfc2279)
            (b'\xfc', FFFD), # only the start byte
            (b'\xfc\xfc', FFFD*2), # 2 start bytes
            (b'\xfc\x80\x80', FFFD*3), # only 2 continuation bytes
            (b'\xfc\x80\x80\x80\x80\x80', FFFD*6), # 6 continuation bytes
            # invalid start byte
            (b'\xfe', FFFD),
            (b'\xfe\x80\x80', FFFD*3),
            # other sequences
            (b'\xf1\x80\x41\x42\x43', '\ufffd\x41\x42\x43'),
            (b'\xf1\x80\xff\x42\x43', '\ufffd\ufffd\x42\x43'),
            (b'\xf1\x80\xc2\x81\x43', '\ufffd\x81\x43'),
            (b'\x61\xF1\x80\x80\xE1\x80\xC2\x62\x80\x63\x80\xBF\x64',
             '\x61\uFFFD\uFFFD\uFFFD\x62\uFFFD\x63\uFFFD\uFFFD\x64'),
        ]
        for n, (seq, res) in enumerate(sequences):
            seq = yp_bytes(seq)
            res = yp_str(res)
            self.assertRaises(UnicodeDecodeError, seq.decode, 'utf-8', 'strict')
            self.assertEqual(seq.decode('utf-8', 'replace'), res)
            self.assertEqual((seq+yp_bytes(b'b')).decode('utf-8', 'replace'), res+yp_str('b'))
            self.assertEqual(seq.decode('utf-8', 'ignore'),
                             res.replace('\uFFFD', ''))

    def test_utf8_decode_nohtyp_precheck(self):
        """nohtyP uses the str's inline buffer to decode "the first few" bytes to determine what
        encoding should be used for the string (i.e. latin-1, ucs-2, or ucs-4).  The intention is
        to save having to immediately reallocate the buffer in order to upconvert to the correct
        encoding.  However, this is not performed if:
            - the bytes start with ascii (ascii-as-utf-8 is heavily optimized)
            - (similar if the bytes are empty or null)
            - if the decoded string is expected to fit in the inline buffer anyway
        """
        empty  = (yp_bytes(b""), yp_str(""))
        latin1 = (yp_bytes(b"\xc2\x80"), yp_str("\x80"))
        ucs2_2 = (yp_bytes(b"\xc4\x80"), yp_str("\u0100"))
        ucs2_3 = (yp_bytes(b'\xe0\xa0\x80'), yp_str("\u0800"))
        ucs4   = (yp_bytes(b"\xf0\x90\x80\x80"), yp_str("\U00010000" ))
        error  = (yp_bytes(b"\xff"), yp_str("\ufffd"))
        sequences = (
            (empty[0].join( (a[0], b[0], c[0], d[0]) ), empty[1].join( (a[1], b[1], c[1], d[1]) ))
            for a in (empty, latin1)
                for b in (empty, ucs2_2, ucs2_3)
                    for c in (empty, ucs4)
                        for d in (empty, error)
            # The above should generate the following test cases:
            # ""
            # error
            # ucs-4
            # ucs-4, then error
            # ucs-2
            # ucs-2, then error
            # ucs-2, then ucs-4
            # ucs-2, then ucs-4, then error
            # latin-1
            # latin-1, then error
            # latin-1, then ucs-4
            # latin-1, then ucs-4, then error
            # latin-1, then ucs-2
            # latin-1, then ucs-2, then error
            # latin-1, then ucs-2, then ucs-4
            # latin-1, then ucs-2, then ucs-4, then error
        )

        # The maximum (latin-1) length of test strings that we will generate to test the inlinelen
        # (i.e. fake_end) boundary.  Since we can't be sure exactly where this boundary is, we
        # test a range of values in order to slowly push our test data towards and over.
        inlinelen_test_end = 1024+1

        # These must be ascii byte/chars so that 1 byte is 1 character and is represented in the
        # smallest internal string encoding
        padBytes_1 = yp_bytes( b"a" )
        padStr_1 = yp_str( "a" )
        padBytes = tuple( padBytes_1 * i for i in range( inlinelen_test_end ) )
        padStr   = tuple( padStr_1   * i for i in range( inlinelen_test_end ) )

        # Verifies the precheck works in the usual cases.  We have to pad the test data so we
        # don't take the len(seq)<inlinelen shortcut, but otherwise we're ensuring the decoding
        # is working just as if the precheck wasn't there.
        for seq, result in sequences:
            seq    += padBytes[-1]
            result += padStr[-1]
            self.assertEqual( seq.decode( errors="replace" ), result )

        # Check with a valid surr pair that gets truncated by fake_end.  We don't care if the
        # shortcut is taken in early iterations.
        for surrBytes, surrChar in (latin1, ucs2_2, ucs2_3, ucs4):
            for pad_len in range( inlinelen_test_end ):
                # We must start with a valid non-ascii character.  Then add a byte each time to push
                # the surrogate pair towards and over the inlinelen boundary.
                seq    = padBytes[0].join( (latin1[0], padBytes[pad_len], surrBytes) )
                result = padStr[0].join(   (latin1[1], padStr[pad_len],   surrChar)  )
                self.assertEqual( seq.decode(), result )

        # Debug builds check for unnecessary string resizes.  There was a bug in precheck that
        # resized the string when there was plenty of room left (i.e. the first estimate assumed
        # each byte was a char, but by the time the precheck was finished it determined most
        # characters were two or three bytes, and it had room inline even if rest were one byte).
        # Again, we don't care if the shortcut is taken.
        for surrBytes, surrChar in (latin1, ucs2_2, ucs2_3, ucs4):
            for pad_len in range( inlinelen_test_end ):
                seq    = surrBytes * pad_len
                result = surrChar  * pad_len
                self.assertEqual( seq.decode(), result )

        # If a ucs-2 character is detected before a ucs-4, the precheck attempts to up-convert the
        # decoded data in-place (i.e. in the inline buffer).  However, it first needs to check
        # that there is room to perform this upconversion; this is tested here.  A bug in this
        # code would manifest as an out-of-bounds error (i.e. memory corruption).
        for surrBytes, surrChar in (ucs2_2, ucs2_3):
            for pad_len in range( inlinelen_test_end ):
                # We must start with a valid non-ascii character.  Then add a byte each time to push
                # the ucs-2 surrogate pair towards and over the midway point, such that upconverting
                # the inline buffer in-place is not possible or not beneficial.  We also need to pad
                # the end to ensure the len(seq)<inlinelen shortcut is not taken.
                seq    = padBytes[0].join( (latin1[0], padBytes[pad_len], surrBytes, padBytes[-1]) )
                result = padStr[0].join(   (latin1[1], padStr[pad_len],   surrChar,  padStr[-1])   )
                self.assertEqual( seq.decode(), result )

    def assertCorrectUTF8Decoding(self, seq, res, err):
        """
        Check that an invalid UTF-8 sequence raises a UnicodeDecodeError when
        'strict' is used, returns res when 'replace' is used, and that doesn't
        return anything when 'ignore' is used.
        """
        with self.assertRaises(UnicodeDecodeError) as cm:
            seq.decode('utf-8')
        exc = cm.exception

        # TODO nohtyP exceptions don't currently have messages
        #self.assertIn(err, yp_str(exc))
        self.assertEqual(seq.decode('utf-8', 'replace'), res)
        self.assertEqual((yp_bytes(b'aaaa') + seq + yp_bytes(b'bbbb')).decode('utf-8', 'replace'),
                         yp_str('aaaa') + res + yp_str('bbbb'))
        res = res.replace('\ufffd', '')
        self.assertEqual(seq.decode('utf-8', 'ignore'), res)
        self.assertEqual((yp_bytes(b'aaaa') + seq + yp_bytes(b'bbbb')).decode('utf-8', 'ignore'),
                          yp_str('aaaa') + res + yp_str('bbbb'))

    def test_invalid_start_byte(self):
        """
        Test that an 'invalid start byte' error is raised when the first byte
        is not in the ASCII range or is not a valid start byte of a 2-, 3-, or
        4-bytes sequence. The invalid start byte is replaced with a single
        U+FFFD when errors='replace'.
        E.g. <80> is a continuation byte and can appear only after a start byte.
        """
        FFFD = '\ufffd'
        for byte in b'\x80\xA0\x9F\xBF\xC0\xC1\xF5\xFF':
            self.assertCorrectUTF8Decoding(yp_bytes([byte]), '\ufffd',
                                           'invalid start byte')

    def test_unexpected_end_of_data(self):
        """
        Test that an 'unexpected end of data' error is raised when the string
        ends after a start byte of a 2-, 3-, or 4-bytes sequence without having
        enough continuation bytes.  The incomplete sequence is replaced with a
        single U+FFFD when errors='replace'.
        E.g. in the sequence <F3 80 80>, F3 is the start byte of a 4-bytes
        sequence, but it's followed by only 2 valid continuation bytes and the
        last continuation bytes is missing.
        Note: the continuation bytes must be all valid, if one of them is
        invalid another error will be raised.
        """
        sequences = [
            'C2', 'DF',
            'E0 A0', 'E0 BF', 'E1 80', 'E1 BF', 'EC 80', 'EC BF',
            'ED 80', 'ED 9F', 'EE 80', 'EE BF', 'EF 80', 'EF BF',
            'F0 90', 'F0 BF', 'F0 90 80', 'F0 90 BF', 'F0 BF 80', 'F0 BF BF',
            'F1 80', 'F1 BF', 'F1 80 80', 'F1 80 BF', 'F1 BF 80', 'F1 BF BF',
            'F3 80', 'F3 BF', 'F3 80 80', 'F3 80 BF', 'F3 BF 80', 'F3 BF BF',
            'F4 80', 'F4 8F', 'F4 80 80', 'F4 80 BF', 'F4 8F 80', 'F4 8F BF'
        ]
        FFFD = '\ufffd'
        for seq in sequences:
            self.assertCorrectUTF8Decoding(yp_bytes.fromhex(seq), '\ufffd',
                                           'unexpected end of data')

    def test_invalid_cb_for_2bytes_seq(self):
        """
        Test that an 'invalid continuation byte' error is raised when the
        continuation byte of a 2-bytes sequence is invalid.  The start byte
        is replaced by a single U+FFFD and the second byte is handled
        separately when errors='replace'.
        E.g. in the sequence <C2 41>, C2 is the start byte of a 2-bytes
        sequence, but 41 is not a valid continuation byte because it's the
        ASCII letter 'A'.
        """
        FFFD = yp_str('\ufffd')
        FFFDx2 = FFFD * 2
        sequences = [
            (yp_str('C2 00'), FFFD+yp_str('\x00')), (yp_str('C2 7F'), FFFD+yp_str('\x7f')),
            (yp_str('C2 C0'), FFFDx2),              (yp_str('C2 FF'), FFFDx2),
            (yp_str('DF 00'), FFFD+yp_str('\x00')), (yp_str('DF 7F'), FFFD+yp_str('\x7f')),
            (yp_str('DF C0'), FFFDx2),              (yp_str('DF FF'), FFFDx2),
        ]
        for seq, res in sequences:
            self.assertCorrectUTF8Decoding(yp_bytes.fromhex(seq), res,
                                           'invalid continuation byte')

    def test_invalid_cb_for_3bytes_seq(self):
        """
        Test that an 'invalid continuation byte' error is raised when the
        continuation byte(s) of a 3-bytes sequence are invalid.  When
        errors='replace', if the first continuation byte is valid, the first
        two bytes (start byte + 1st cb) are replaced by a single U+FFFD and the
        third byte is handled separately, otherwise only the start byte is
        replaced with a U+FFFD and the other continuation bytes are handled
        separately.
        E.g. in the sequence <E1 80 41>, E1 is the start byte of a 3-bytes
        sequence, 80 is a valid continuation byte, but 41 is not a valid cb
        because it's the ASCII letter 'A'.
        Note: when the start byte is E0 or ED, the valid ranges for the first
        continuation byte are limited to A0..BF and 80..9F respectively.
        Python 2 used to consider all the bytes in range 80..BF valid when the
        start byte was ED.  This is fixed in Python 3.
        """
        FFFD = '\ufffd'
        FFFDx2 = FFFD * 2
        sequences = [
            ('E0 00', FFFD+'\x00'), ('E0 7F', FFFD+'\x7f'), ('E0 80', FFFDx2),
            ('E0 9F', FFFDx2), ('E0 C0', FFFDx2), ('E0 FF', FFFDx2),
            ('E0 A0 00', FFFD+'\x00'), ('E0 A0 7F', FFFD+'\x7f'),
            ('E0 A0 C0', FFFDx2), ('E0 A0 FF', FFFDx2),
            ('E0 BF 00', FFFD+'\x00'), ('E0 BF 7F', FFFD+'\x7f'),
            ('E0 BF C0', FFFDx2), ('E0 BF FF', FFFDx2), ('E1 00', FFFD+'\x00'),
            ('E1 7F', FFFD+'\x7f'), ('E1 C0', FFFDx2), ('E1 FF', FFFDx2),
            ('E1 80 00', FFFD+'\x00'), ('E1 80 7F', FFFD+'\x7f'),
            ('E1 80 C0', FFFDx2), ('E1 80 FF', FFFDx2),
            ('E1 BF 00', FFFD+'\x00'), ('E1 BF 7F', FFFD+'\x7f'),
            ('E1 BF C0', FFFDx2), ('E1 BF FF', FFFDx2), ('EC 00', FFFD+'\x00'),
            ('EC 7F', FFFD+'\x7f'), ('EC C0', FFFDx2), ('EC FF', FFFDx2),
            ('EC 80 00', FFFD+'\x00'), ('EC 80 7F', FFFD+'\x7f'),
            ('EC 80 C0', FFFDx2), ('EC 80 FF', FFFDx2),
            ('EC BF 00', FFFD+'\x00'), ('EC BF 7F', FFFD+'\x7f'),
            ('EC BF C0', FFFDx2), ('EC BF FF', FFFDx2), ('ED 00', FFFD+'\x00'),
            ('ED 7F', FFFD+'\x7f'),
            ('ED A0', FFFDx2), ('ED BF', FFFDx2), # see note ^
            ('ED C0', FFFDx2), ('ED FF', FFFDx2), ('ED 80 00', FFFD+'\x00'),
            ('ED 80 7F', FFFD+'\x7f'), ('ED 80 C0', FFFDx2),
            ('ED 80 FF', FFFDx2), ('ED 9F 00', FFFD+'\x00'),
            ('ED 9F 7F', FFFD+'\x7f'), ('ED 9F C0', FFFDx2),
            ('ED 9F FF', FFFDx2), ('EE 00', FFFD+'\x00'),
            ('EE 7F', FFFD+'\x7f'), ('EE C0', FFFDx2), ('EE FF', FFFDx2),
            ('EE 80 00', FFFD+'\x00'), ('EE 80 7F', FFFD+'\x7f'),
            ('EE 80 C0', FFFDx2), ('EE 80 FF', FFFDx2),
            ('EE BF 00', FFFD+'\x00'), ('EE BF 7F', FFFD+'\x7f'),
            ('EE BF C0', FFFDx2), ('EE BF FF', FFFDx2), ('EF 00', FFFD+'\x00'),
            ('EF 7F', FFFD+'\x7f'), ('EF C0', FFFDx2), ('EF FF', FFFDx2),
            ('EF 80 00', FFFD+'\x00'), ('EF 80 7F', FFFD+'\x7f'),
            ('EF 80 C0', FFFDx2), ('EF 80 FF', FFFDx2),
            ('EF BF 00', FFFD+'\x00'), ('EF BF 7F', FFFD+'\x7f'),
            ('EF BF C0', FFFDx2), ('EF BF FF', FFFDx2),
        ]
        for seq, res in sequences:
            self.assertCorrectUTF8Decoding(yp_bytes.fromhex(seq), res,
                                           'invalid continuation byte')

    def test_invalid_cb_for_4bytes_seq(self):
        """
        Test that an 'invalid continuation byte' error is raised when the
        continuation byte(s) of a 4-bytes sequence are invalid.  When
        errors='replace',the start byte and all the following valid
        continuation bytes are replaced with a single U+FFFD, and all the bytes
        starting from the first invalid continuation bytes (included) are
        handled separately.
        E.g. in the sequence <E1 80 41>, E1 is the start byte of a 3-bytes
        sequence, 80 is a valid continuation byte, but 41 is not a valid cb
        because it's the ASCII letter 'A'.
        Note: when the start byte is E0 or ED, the valid ranges for the first
        continuation byte are limited to A0..BF and 80..9F respectively.
        However, when the start byte is ED, Python 2 considers all the bytes
        in range 80..BF valid.  This is fixed in Python 3.
        """
        FFFD = '\ufffd'
        FFFDx2 = FFFD * 2
        sequences = [
            ('F0 00', FFFD+'\x00'), ('F0 7F', FFFD+'\x7f'), ('F0 80', FFFDx2),
            ('F0 8F', FFFDx2), ('F0 C0', FFFDx2), ('F0 FF', FFFDx2),
            ('F0 90 00', FFFD+'\x00'), ('F0 90 7F', FFFD+'\x7f'),
            ('F0 90 C0', FFFDx2), ('F0 90 FF', FFFDx2),
            ('F0 BF 00', FFFD+'\x00'), ('F0 BF 7F', FFFD+'\x7f'),
            ('F0 BF C0', FFFDx2), ('F0 BF FF', FFFDx2),
            ('F0 90 80 00', FFFD+'\x00'), ('F0 90 80 7F', FFFD+'\x7f'),
            ('F0 90 80 C0', FFFDx2), ('F0 90 80 FF', FFFDx2),
            ('F0 90 BF 00', FFFD+'\x00'), ('F0 90 BF 7F', FFFD+'\x7f'),
            ('F0 90 BF C0', FFFDx2), ('F0 90 BF FF', FFFDx2),
            ('F0 BF 80 00', FFFD+'\x00'), ('F0 BF 80 7F', FFFD+'\x7f'),
            ('F0 BF 80 C0', FFFDx2), ('F0 BF 80 FF', FFFDx2),
            ('F0 BF BF 00', FFFD+'\x00'), ('F0 BF BF 7F', FFFD+'\x7f'),
            ('F0 BF BF C0', FFFDx2), ('F0 BF BF FF', FFFDx2),
            ('F1 00', FFFD+'\x00'), ('F1 7F', FFFD+'\x7f'), ('F1 C0', FFFDx2),
            ('F1 FF', FFFDx2), ('F1 80 00', FFFD+'\x00'),
            ('F1 80 7F', FFFD+'\x7f'), ('F1 80 C0', FFFDx2),
            ('F1 80 FF', FFFDx2), ('F1 BF 00', FFFD+'\x00'),
            ('F1 BF 7F', FFFD+'\x7f'), ('F1 BF C0', FFFDx2),
            ('F1 BF FF', FFFDx2), ('F1 80 80 00', FFFD+'\x00'),
            ('F1 80 80 7F', FFFD+'\x7f'), ('F1 80 80 C0', FFFDx2),
            ('F1 80 80 FF', FFFDx2), ('F1 80 BF 00', FFFD+'\x00'),
            ('F1 80 BF 7F', FFFD+'\x7f'), ('F1 80 BF C0', FFFDx2),
            ('F1 80 BF FF', FFFDx2), ('F1 BF 80 00', FFFD+'\x00'),
            ('F1 BF 80 7F', FFFD+'\x7f'), ('F1 BF 80 C0', FFFDx2),
            ('F1 BF 80 FF', FFFDx2), ('F1 BF BF 00', FFFD+'\x00'),
            ('F1 BF BF 7F', FFFD+'\x7f'), ('F1 BF BF C0', FFFDx2),
            ('F1 BF BF FF', FFFDx2), ('F3 00', FFFD+'\x00'),
            ('F3 7F', FFFD+'\x7f'), ('F3 C0', FFFDx2), ('F3 FF', FFFDx2),
            ('F3 80 00', FFFD+'\x00'), ('F3 80 7F', FFFD+'\x7f'),
            ('F3 80 C0', FFFDx2), ('F3 80 FF', FFFDx2),
            ('F3 BF 00', FFFD+'\x00'), ('F3 BF 7F', FFFD+'\x7f'),
            ('F3 BF C0', FFFDx2), ('F3 BF FF', FFFDx2),
            ('F3 80 80 00', FFFD+'\x00'), ('F3 80 80 7F', FFFD+'\x7f'),
            ('F3 80 80 C0', FFFDx2), ('F3 80 80 FF', FFFDx2),
            ('F3 80 BF 00', FFFD+'\x00'), ('F3 80 BF 7F', FFFD+'\x7f'),
            ('F3 80 BF C0', FFFDx2), ('F3 80 BF FF', FFFDx2),
            ('F3 BF 80 00', FFFD+'\x00'), ('F3 BF 80 7F', FFFD+'\x7f'),
            ('F3 BF 80 C0', FFFDx2), ('F3 BF 80 FF', FFFDx2),
            ('F3 BF BF 00', FFFD+'\x00'), ('F3 BF BF 7F', FFFD+'\x7f'),
            ('F3 BF BF C0', FFFDx2), ('F3 BF BF FF', FFFDx2),
            ('F4 00', FFFD+'\x00'), ('F4 7F', FFFD+'\x7f'), ('F4 90', FFFDx2),
            ('F4 BF', FFFDx2), ('F4 C0', FFFDx2), ('F4 FF', FFFDx2),
            ('F4 80 00', FFFD+'\x00'), ('F4 80 7F', FFFD+'\x7f'),
            ('F4 80 C0', FFFDx2), ('F4 80 FF', FFFDx2),
            ('F4 8F 00', FFFD+'\x00'), ('F4 8F 7F', FFFD+'\x7f'),
            ('F4 8F C0', FFFDx2), ('F4 8F FF', FFFDx2),
            ('F4 80 80 00', FFFD+'\x00'), ('F4 80 80 7F', FFFD+'\x7f'),
            ('F4 80 80 C0', FFFDx2), ('F4 80 80 FF', FFFDx2),
            ('F4 80 BF 00', FFFD+'\x00'), ('F4 80 BF 7F', FFFD+'\x7f'),
            ('F4 80 BF C0', FFFDx2), ('F4 80 BF FF', FFFDx2),
            ('F4 8F 80 00', FFFD+'\x00'), ('F4 8F 80 7F', FFFD+'\x7f'),
            ('F4 8F 80 C0', FFFDx2), ('F4 8F 80 FF', FFFDx2),
            ('F4 8F BF 00', FFFD+'\x00'), ('F4 8F BF 7F', FFFD+'\x7f'),
            ('F4 8F BF C0', FFFDx2), ('F4 8F BF FF', FFFDx2)
        ]
        for seq, res in sequences:
            self.assertCorrectUTF8Decoding(yp_bytes.fromhex(seq), res,
                                           'invalid continuation byte')

    @yp_unittest.skip_str_codecs
    def test_codecs_idna(self):
        # Test whether trailing dot is preserved
        self.assertEqual("www.python.org.".encode("idna"), b"www.python.org.")

    @yp_unittest.skip_str_codecs
    def test_codecs_errors(self):
        # Error handling (encoding)
        self.assertRaises(UnicodeError, 'Andr\202 x'.encode, 'ascii')
        self.assertRaises(UnicodeError, 'Andr\202 x'.encode, 'ascii','strict')
        self.assertEqual('Andr\202 x'.encode('ascii','ignore'), b"Andr x")
        self.assertEqual('Andr\202 x'.encode('ascii','replace'), b"Andr? x")
        self.assertEqual('Andr\202 x'.encode('ascii', 'replace'),
                         'Andr\202 x'.encode('ascii', errors='replace'))
        self.assertEqual('Andr\202 x'.encode('ascii', 'ignore'),
                         'Andr\202 x'.encode(encoding='ascii', errors='ignore'))

        # Error handling (decoding)
        self.assertRaises(UnicodeError, yp_str, b'Andr\202 x', 'ascii')
        self.assertRaises(UnicodeError, yp_str, b'Andr\202 x', 'ascii', 'strict')
        self.assertEqual(yp_str(b'Andr\202 x', 'ascii', 'ignore'), "Andr x")
        self.assertEqual(yp_str(b'Andr\202 x', 'ascii', 'replace'), 'Andr\uFFFD x')
        self.assertEqual(yp_str(b'\202 x', 'ascii', 'replace'), '\uFFFD x')

        # Error handling (unknown character names)
        self.assertEqual(b"\\N{foo}xx".decode("unicode-escape", "ignore"), "xx")

        # Error handling (truncated escape sequence)
        self.assertRaises(UnicodeError, b"\\".decode, "unicode-escape")

        self.assertRaises(TypeError, b"hello".decode, "test.unicode1")
        self.assertRaises(TypeError, yp_str, b"hello", "test.unicode2")
        self.assertRaises(TypeError, "hello".encode, "test.unicode1")
        self.assertRaises(TypeError, "hello".encode, "test.unicode2")

        # Error handling (wrong arguments)
        self.assertRaises(TypeError, "hello".encode, 42, 42, 42)

        # Error handling (lone surrogate in
        # _PyUnicode_TransformDecimalAndSpaceToASCII())
        self.assertRaises(ValueError, int, "\ud800")
        self.assertRaises(ValueError, int, "\udf00")
        self.assertRaises(ValueError, float, "\ud800")
        self.assertRaises(ValueError, float, "\udf00")
        self.assertRaises(ValueError, complex, "\ud800")
        self.assertRaises(ValueError, complex, "\udf00")

    @yp_unittest.skip_str_codecs
    def test_codecs(self):
        # Encoding
        self.assertEqual(yp_str('hello').encode('ascii'), b'hello')
        self.assertEqual(yp_str('hello').encode('utf-7'), b'hello')
        self.assertEqual(yp_str('hello').encode('utf-8'), b'hello')
        self.assertEqual(yp_str('hello').encode('utf-8'), b'hello')
        self.assertEqual(yp_str('hello').encode('utf-16-le'), b'h\000e\000l\000l\000o\000')
        self.assertEqual(yp_str('hello').encode('utf-16-be'), b'\000h\000e\000l\000l\000o')
        self.assertEqual(yp_str('hello').encode('latin-1'), b'hello')

        # Default encoding is utf-8
        self.assertEqual(yp_str('\u2603').encode(), b'\xe2\x98\x83')

        # Roundtrip safety for BMP (just the first 1024 chars)
        for c in range(1024):
            u = chr(c)
            for encoding in ('utf-7', 'utf-8', 'utf-16', 'utf-16-le',
                             'utf-16-be', 'raw_unicode_escape',
                             'unicode_escape'):
                self.assertEqual(yp_str(u.encode(encoding),encoding), u)

        # Roundtrip safety for BMP (just the first 256 chars)
        for c in range(256):
            u = chr(c)
            for encoding in ('latin-1',):
                self.assertEqual(yp_str(u.encode(encoding),encoding), u)

        # Roundtrip safety for BMP (just the first 128 chars)
        for c in range(128):
            u = chr(c)
            for encoding in ('ascii',):
                self.assertEqual(yp_str(u.encode(encoding),encoding), u)

        # Roundtrip safety for non-BMP (just a few chars)
        with warnings.catch_warnings():
            u = yp_str('\U00010001\U00020002\U00030003\U00040004\U00050005')
            for encoding in ('utf-8', 'utf-16', 'utf-16-le', 'utf-16-be',
                             'raw_unicode_escape', 'unicode_escape'):
                self.assertEqual(yp_str(u.encode(encoding),encoding), u)

    @support.requires_resource('cpu')
    def test_codecs_all_code_points(self):
        # UTF-8 must be roundtrip safe for all code points
        # (except surrogates, which are forbidden).
        # TODO This optimization can be contributed back to Python
        u  = yp_str('').join(yp_chr(x) for x in range(0, 0xd800))
        u += yp_str('').join(yp_chr(x) for x in range(0xe000, 0x110000))
        for encoding in ('utf-8',):
            self.assertEqual(yp_str(u.encode(encoding),encoding), u)

    # TODO remove once nohtyP supports utf-16, etc
    def test_codecs_only_utf8(self):
        # Encoding
        self.assertEqual(yp_str('hello').encode('utf-8'), b'hello')
        self.assertEqual(yp_str('hello').encode('utf-8'), b'hello')

        # Default encoding is utf-8
        self.assertEqual(yp_str('\u2603').encode(), b'\xe2\x98\x83')

        # Roundtrip safety for BMP (just the first 1024 chars)
        for c in range(1024):
            u = yp_chr(c)
            for encoding in ('utf-8', ):
                with warnings.catch_warnings():
                    # unicode-internal has been deprecated
                    warnings.simplefilter("ignore", DeprecationWarning)

                    self.assertEqual(yp_str(u.encode(encoding),encoding), u)

        # Roundtrip safety for non-BMP (just a few chars)
        with warnings.catch_warnings():
            # unicode-internal has been deprecated
            warnings.simplefilter("ignore", DeprecationWarning)

            u = yp_str('\U00010001\U00020002\U00030003\U00040004\U00050005')
            for encoding in ('utf-8', ):
                self.assertEqual(yp_str(u.encode(encoding),encoding), u)

    @yp_unittest.skip_str_codecs
    def test_codecs_charmap(self):
        # 0-127
        s = yp_bytes(range(128))
        for encoding in (
            'cp037', 'cp1026', 'cp273',
            'cp437', 'cp500', 'cp720', 'cp737', 'cp775', 'cp850',
            'cp852', 'cp855', 'cp858', 'cp860', 'cp861', 'cp862',
            'cp863', 'cp865', 'cp866', 'cp1125',
            'iso8859_10', 'iso8859_13', 'iso8859_14', 'iso8859_15',
            'iso8859_2', 'iso8859_3', 'iso8859_4', 'iso8859_5', 'iso8859_6',
            'iso8859_7', 'iso8859_9',
            'koi8_r', 'koi8_t', 'koi8_u', 'kz1048', 'latin_1',
            'mac_cyrillic', 'mac_latin2',

            'cp1250', 'cp1251', 'cp1252', 'cp1253', 'cp1254', 'cp1255',
            'cp1256', 'cp1257', 'cp1258',
            'cp856', 'cp857', 'cp864', 'cp869', 'cp874',

            'mac_greek', 'mac_iceland','mac_roman', 'mac_turkish',
            'cp1006', 'iso8859_8',

            ### These have undefined mappings:
            #'cp424',

            ### These fail the round-trip:
            #'cp875'

            ):
            self.assertEqual(yp_str(s, encoding).encode(encoding), s)

        # 128-255
        s = yp_bytes(range(128, 256))
        for encoding in (
            'cp037', 'cp1026', 'cp273',
            'cp437', 'cp500', 'cp720', 'cp737', 'cp775', 'cp850',
            'cp852', 'cp855', 'cp858', 'cp860', 'cp861', 'cp862',
            'cp863', 'cp865', 'cp866', 'cp1125',
            'iso8859_10', 'iso8859_13', 'iso8859_14', 'iso8859_15',
            'iso8859_2', 'iso8859_4', 'iso8859_5',
            'iso8859_9', 'koi8_r', 'koi8_u', 'latin_1',
            'mac_cyrillic', 'mac_latin2',

            ### These have undefined mappings:
            #'cp1250', 'cp1251', 'cp1252', 'cp1253', 'cp1254', 'cp1255',
            #'cp1256', 'cp1257', 'cp1258',
            #'cp424', 'cp856', 'cp857', 'cp864', 'cp869', 'cp874',
            #'iso8859_3', 'iso8859_6', 'iso8859_7', 'koi8_t', 'kz1048',
            #'mac_greek', 'mac_iceland','mac_roman', 'mac_turkish',

            ### These fail the round-trip:
            #'cp1006', 'cp875', 'iso8859_8',

            ):
            self.assertEqual(yp_str(s, encoding).encode(encoding), s)

    @yp_unittest.skip_not_applicable
    def test_concatenation(self):
        self.assertEqual(("abc" "def"), "abcdef")
        self.assertEqual(("abc" "def"), "abcdef")
        self.assertEqual(("abc" "def"), "abcdef")
        self.assertEqual(("abc" "def" "ghi"), "abcdefghi")
        self.assertEqual(("abc" "def" "ghi"), "abcdefghi")

    @yp_unittest.skip_str_codecs
    def test_ucs4(self):
        x = '\U00100000'
        y = x.encode("raw-unicode-escape").decode("raw-unicode-escape")
        self.assertEqual(x, y)

        y = br'\U00100000'
        x = y.decode("raw-unicode-escape").encode("raw-unicode-escape")
        self.assertEqual(x, y)
        y = br'\U00010000'
        x = y.decode("raw-unicode-escape").encode("raw-unicode-escape")
        self.assertEqual(x, y)

        try:
            br'\U11111111'.decode("raw-unicode-escape")
        except UnicodeDecodeError as e:
            self.assertEqual(e.start, 0)
            self.assertEqual(e.end, 10)
        else:
            self.fail("Should have raised UnicodeDecodeError")

    @yp_unittest.skip_user_defined_types
    def test_conversion(self):
        # Make sure __str__() works properly
        class ObjectToStr:
            def __str__(self):
                return "foo"

        class StrSubclassToStr(yp_str):
            def __str__(self):
                return "foo"

        class StrSubclassToStrSubclass(yp_str):
            def __new__(cls, content=""):
                return yp_str.__new__(cls, 2*content)
            def __str__(self):
                return self

        self.assertEqual(yp_str(ObjectToStr()), "foo")
        self.assertEqual(yp_str(StrSubclassToStr("bar")), "foo")
        s = yp_str(StrSubclassToStrSubclass("foo"))
        self.assertEqual(s, "foofoo")
        self.assertIs(type(s), StrSubclassToStrSubclass)
        s = StrSubclass(StrSubclassToStrSubclass("foo"))
        self.assertEqual(s, "foofoo")
        self.assertIs(type(s), StrSubclass)

    @yp_unittest.skip_not_applicable
    def test_unicode_repr(self):
        class s1:
            def __repr__(self):
                return '\\n'

        class s2:
            def __repr__(self):
                return '\\n'

        self.assertEqual(yp_repr(s1()), '\\n')
        self.assertEqual(yp_repr(s2()), '\\n')

    def test_printable_repr(self):
        self.assertEqual(yp_repr('\U00010000'), "'%c'" % (0x10000,)) # printable
        self.assertEqual(yp_repr('\U00014000'), "'\\U00014000'")     # nonprintable

    # This test only affects 32-bit platforms because expandtabs can only take
    # an int as the max value, not a 64-bit C long.  If expandtabs is changed
    # to take a 64-bit long, this test should apply to all platforms.
    @yp_unittest.skipIf(sys.maxsize > (1 << 32) or struct.calcsize('P') != 4,
                     'only applies to 32-bit platforms')
    @yp_unittest.skip_str_space
    def test_expandtabs_overflows_gracefully(self):
        self.assertRaises(OverflowError, 't\tt\t'.expandtabs, sys.maxsize)

    @support.cpython_only
    @yp_unittest.skip_str_space
    def test_expandtabs_optimization(self):
        s = 'abc'
        self.assertIs(s.expandtabs(), s)

    def test_raiseMemError(self):
        if struct.calcsize('P') == 8:
            # 64 bits pointers
            ascii_struct_size = 48
            compact_struct_size = 72
        else:
            # 32 bits pointers
            ascii_struct_size = 24
            compact_struct_size = 36

        for char in ('a', '\xe9', '\u20ac', '\U0010ffff'):
            code = ord(char)
            if code < 0x100:
                char_size = 1  # sizeof(Py_UCS1)
                struct_size = ascii_struct_size
            elif code < 0x10000:
                char_size = 2  # sizeof(Py_UCS2)
                struct_size = compact_struct_size
            else:
                char_size = 4  # sizeof(Py_UCS4)
                struct_size = compact_struct_size
            # Note: sys.maxsize is half of the actual max allocation because of
            # the signedness of Py_ssize_t. Strings of maxlen-1 should in principle
            # be allocatable, given enough memory.
            maxlen = ((sys.maxsize - struct_size) // char_size)
            alloc = lambda: char * maxlen
            self.assertRaises(MemoryError, alloc)
            self.assertRaises(MemoryError, alloc)

    @yp_unittest.skip_str_format
    @yp_unittest.skip_user_defined_types
    def test_format_subclass(self):
        class S(yp_str):
            def __str__(self):
                return '__str__ overridden'
        s = S('xxx')
        self.assertEqual("%s" % s, '__str__ overridden')
        self.assertEqual("{}".format(s), '__str__ overridden')

    @yp_unittest.skip_user_defined_types
    def test_subclass_add(self):
        class S(yp_str):
            def __add__(self, o):
                return "3"
        self.assertEqual(S("4") + S("5"), "3")
        class S(yp_str):
            def __iadd__(self, o):
                return "3"
        s = S("1")
        s += "4"
        self.assertEqual(s, "3")

    @yp_unittest.skip_pickling
    def test_getnewargs(self):
        text = yp_str('abc')
        args = text.__getnewargs__()
        self.assertIsNot(args[0], text)
        self.assertEqual(args[0], text)
        self.assertEqual(len(args), 1)

    @support.cpython_only
    @support.requires_legacy_unicode_capi
    @yp_unittest.skip_not_applicable
    def test_resize(self):
        from _testcapi import getargs_u
        for length in range(1, 100, 7):
            # generate a fresh string (refcount=1)
            text = 'a' * length + 'b'

            # fill wstr internal field
            with self.assertWarns(DeprecationWarning):
                abc = getargs_u(text)
            self.assertEqual(abc, text)

            # resize text: wstr field must be cleared and then recomputed
            text += 'c'
            with self.assertWarns(DeprecationWarning):
                abcdef = getargs_u(text)
            self.assertNotEqual(abc, abcdef)
            self.assertEqual(abcdef, text)

    def test_compare(self):
        # Issue #17615
        N = 10
        ascii = yp_str('a') * N
        ascii2 = yp_str('z') * N
        latin = yp_str('\x80') * N
        latin2 = yp_str('\xff') * N
        bmp = yp_str('\u0100') * N
        bmp2 = yp_str('\uffff') * N
        astral = yp_str('\U00100000') * N
        astral2 = yp_str('\U0010ffff') * N
        strings = (
            ascii, ascii2,
            latin, latin2,
            bmp, bmp2,
            astral, astral2)

        with self.nohtyPCheck(enabled=False):
            for text1, text2 in itertools.combinations(strings, 2):
                equal = text1 is text2
                self.assertEqual(text1 == text2, equal)
                self.assertEqual(text1 != text2, not equal)

                if equal:
                    self.assertTrue(text1 <= text2)
                    self.assertTrue(text1 >= text2)

                    # text1 is text2: duplicate strings to skip the "str1 == str2"
                    # optimization in unicode_compare_eq() and really compare
                    # character per character
                    copy1 = duplicate_string(text1)
                    copy2 = duplicate_string(text2)
                    self.assertIsNot(copy1, copy2)

                    self.assertTrue(copy1 == copy2)
                    self.assertFalse(copy1 != copy2)

                    self.assertTrue(copy1 <= copy2)
                    self.assertTrue(copy2 >= copy2)

            # TODO(skip_str_big_chars) Some comparisons are not yet implemented

            self.assertTrue(ascii < ascii2)
            self.assertTrue(ascii < latin)
            # self.assertTrue(ascii < bmp)
            # self.assertTrue(ascii < astral)
            self.assertFalse(ascii >= ascii2)
            self.assertFalse(ascii >= latin)
            # self.assertFalse(ascii >= bmp)
            # self.assertFalse(ascii >= astral)

            self.assertFalse(latin < ascii)
            self.assertTrue(latin < latin2)
            # self.assertTrue(latin < bmp)
            # self.assertTrue(latin < astral)
            self.assertTrue(latin >= ascii)
            self.assertFalse(latin >= latin2)
            # self.assertFalse(latin >= bmp)
            # self.assertFalse(latin >= astral)

            # self.assertFalse(bmp < ascii)
            # self.assertFalse(bmp < latin)
            # self.assertTrue(bmp < bmp2)
            # self.assertTrue(bmp < astral)
            # self.assertTrue(bmp >= ascii)
            # self.assertTrue(bmp >= latin)
            # self.assertFalse(bmp >= bmp2)
            # self.assertFalse(bmp >= astral)

            # self.assertFalse(astral < ascii)
            # self.assertFalse(astral < latin)
            # self.assertFalse(astral < bmp2)
            # self.assertTrue(astral < astral2)
            # self.assertTrue(astral >= ascii)
            # self.assertTrue(astral >= latin)
            # self.assertTrue(astral >= bmp2)
            # self.assertFalse(astral >= astral2)

    @yp_unittest.skip_user_defined_types
    def test_free_after_iterating(self):
        support.check_free_after_iterating(self, iter, yp_str)
        support.check_free_after_iterating(self, reversed, yp_str)

    @yp_unittest.skip_not_applicable
    def test_check_encoding_errors(self):
        # bpo-37388: str(bytes) and str.decode() must check encoding and errors
        # arguments in dev mode
        encodings = ('ascii', 'utf8', 'latin1')
        invalid = 'Boom, Shaka Laka, Boom!'
        code = textwrap.dedent(f'''
            import sys
            encodings = {encodings!r}

            for data in (b'', b'short string'):
                try:
                    yp_str(data, encoding={invalid!r})
                except LookupError:
                    pass
                else:
                    sys.exit(21)

                try:
                    yp_str(data, errors={invalid!r})
                except LookupError:
                    pass
                else:
                    sys.exit(22)

                for encoding in encodings:
                    try:
                        yp_str(data, encoding, errors={invalid!r})
                    except LookupError:
                        pass
                    else:
                        sys.exit(22)

            for data in ('', 'short string'):
                try:
                    data.encode(encoding={invalid!r})
                except LookupError:
                    pass
                else:
                    sys.exit(23)

                try:
                    data.encode(errors={invalid!r})
                except LookupError:
                    pass
                else:
                    sys.exit(24)

                for encoding in encodings:
                    try:
                        data.encode(encoding, errors={invalid!r})
                    except LookupError:
                        pass
                    else:
                        sys.exit(24)

            sys.exit(10)
        ''')
        proc = assert_python_failure('-X', 'dev', '-c', code)
        self.assertEqual(proc.rc, 10, proc)


class UnicodeChrarrayTest(string_tests.CommonTest,
        string_tests.MixinStrUnicodeUserStringTest,
        string_tests.MixinStrUnicodeTest,
        yp_unittest.TestCase):

    type2test = yp_chrarray

    @yp_unittest.skip("chrarray doesn't support yp_hash")
    def test_hash(self):
        pass


class CAPITest(yp_unittest.TestCase):

    # Test PyUnicode_FromFormat()
    @yp_unittest.skip_not_applicable
    def test_from_format(self):
        import_helper.import_module('ctypes')
        from ctypes import (
            c_char_p,
            pythonapi, py_object, sizeof,
            c_int, c_long, c_longlong, c_ssize_t,
            c_uint, c_ulong, c_ulonglong, c_size_t, c_void_p)
        name = "PyUnicode_FromFormat"
        _PyUnicode_FromFormat = getattr(pythonapi, name)
        _PyUnicode_FromFormat.argtypes = (c_char_p,)
        _PyUnicode_FromFormat.restype = py_object

        def PyUnicode_FromFormat(format, *args):
            cargs = tuple(
                py_object(arg) if isinstance(arg, yp_str) else arg
                for arg in args)
            return _PyUnicode_FromFormat(format, *cargs)

        def check_format(expected, format, *args):
            text = PyUnicode_FromFormat(format, *args)
            self.assertEqual(expected, text)

        # ascii format, non-ascii argument
        check_format('ascii\x7f=unicode\xe9',
                     b'ascii\x7f=%U', 'unicode\xe9')

        # non-ascii format, ascii argument: ensure that PyUnicode_FromFormatV()
        # raises an error
        self.assertRaisesRegex(ValueError,
            r'^PyUnicode_FromFormatV\(\) expects an ASCII-encoded format '
            'string, got a non-ASCII byte: 0xe9$',
            PyUnicode_FromFormat, b'unicode\xe9=%s', 'ascii')

        # test "%c"
        check_format('\uabcd',
                     b'%c', c_int(0xabcd))
        check_format('\U0010ffff',
                     b'%c', c_int(0x10ffff))
        with self.assertRaises(OverflowError):
            PyUnicode_FromFormat(b'%c', c_int(0x110000))
        # Issue #18183
        check_format('\U00010000\U00100000',
                     b'%c%c', c_int(0x10000), c_int(0x100000))

        # test "%"
        check_format('%',
                     b'%')
        check_format('%',
                     b'%%')
        check_format('%s',
                     b'%%s')
        check_format('[%]',
                     b'[%%]')
        check_format('%abc',
                     b'%%%s', b'abc')

        # truncated string
        check_format('abc',
                     b'%.3s', b'abcdef')
        check_format('abc[\ufffd',
                     b'%.5s', 'abc[\u20ac]'.encode('utf8'))
        check_format("'\\u20acABC'",
                     b'%A', '\u20acABC')
        check_format("'\\u20",
                     b'%.5A', '\u20acABCDEF')
        check_format("'\u20acABC'",
                     b'%R', '\u20acABC')
        check_format("'\u20acA",
                     b'%.3R', '\u20acABCDEF')
        check_format('\u20acAB',
                     b'%.3S', '\u20acABCDEF')
        check_format('\u20acAB',
                     b'%.3U', '\u20acABCDEF')
        check_format('\u20acAB',
                     b'%.3V', '\u20acABCDEF', None)
        check_format('abc[\ufffd',
                     b'%.5V', None, 'abc[\u20ac]'.encode('utf8'))

        # following tests comes from #7330
        # test width modifier and precision modifier with %S
        check_format("repr=  abc",
                     b'repr=%5S', 'abc')
        check_format("repr=ab",
                     b'repr=%.2S', 'abc')
        check_format("repr=   ab",
                     b'repr=%5.2S', 'abc')

        # test width modifier and precision modifier with %R
        check_format("repr=   'abc'",
                     b'repr=%8R', 'abc')
        check_format("repr='ab",
                     b'repr=%.3R', 'abc')
        check_format("repr=  'ab",
                     b'repr=%5.3R', 'abc')

        # test width modifier and precision modifier with %A
        check_format("repr=   'abc'",
                     b'repr=%8A', 'abc')
        check_format("repr='ab",
                     b'repr=%.3A', 'abc')
        check_format("repr=  'ab",
                     b'repr=%5.3A', 'abc')

        # test width modifier and precision modifier with %s
        check_format("repr=  abc",
                     b'repr=%5s', b'abc')
        check_format("repr=ab",
                     b'repr=%.2s', b'abc')
        check_format("repr=   ab",
                     b'repr=%5.2s', b'abc')

        # test width modifier and precision modifier with %U
        check_format("repr=  abc",
                     b'repr=%5U', 'abc')
        check_format("repr=ab",
                     b'repr=%.2U', 'abc')
        check_format("repr=   ab",
                     b'repr=%5.2U', 'abc')

        # test width modifier and precision modifier with %V
        check_format("repr=  abc",
                     b'repr=%5V', 'abc', b'123')
        check_format("repr=ab",
                     b'repr=%.2V', 'abc', b'123')
        check_format("repr=   ab",
                     b'repr=%5.2V', 'abc', b'123')
        check_format("repr=  123",
                     b'repr=%5V', None, b'123')
        check_format("repr=12",
                     b'repr=%.2V', None, b'123')
        check_format("repr=   12",
                     b'repr=%5.2V', None, b'123')

        # test integer formats (%i, %d, %u)
        check_format('010',
                     b'%03i', c_int(10))
        check_format('0010',
                     b'%0.4i', c_int(10))
        check_format('-123',
                     b'%i', c_int(-123))
        check_format('-123',
                     b'%li', c_long(-123))
        check_format('-123',
                     b'%lli', c_longlong(-123))
        check_format('-123',
                     b'%zi', c_ssize_t(-123))

        check_format('-123',
                     b'%d', c_int(-123))
        check_format('-123',
                     b'%ld', c_long(-123))
        check_format('-123',
                     b'%lld', c_longlong(-123))
        check_format('-123',
                     b'%zd', c_ssize_t(-123))

        check_format('123',
                     b'%u', c_uint(123))
        check_format('123',
                     b'%lu', c_ulong(123))
        check_format('123',
                     b'%llu', c_ulonglong(123))
        check_format('123',
                     b'%zu', c_size_t(123))

        # test long output
        min_longlong = -(2 ** (8 * sizeof(c_longlong) - 1))
        max_longlong = -min_longlong - 1
        check_format(yp_str(min_longlong),
                     b'%lld', c_longlong(min_longlong))
        check_format(yp_str(max_longlong),
                     b'%lld', c_longlong(max_longlong))
        max_ulonglong = 2 ** (8 * sizeof(c_ulonglong)) - 1
        check_format(yp_str(max_ulonglong),
                     b'%llu', c_ulonglong(max_ulonglong))
        PyUnicode_FromFormat(b'%p', c_void_p(-1))

        # test padding (width and/or precision)
        check_format('123'.rjust(10, '0'),
                     b'%010i', c_int(123))
        check_format('123'.rjust(100),
                     b'%100i', c_int(123))
        check_format('123'.rjust(100, '0'),
                     b'%.100i', c_int(123))
        check_format('123'.rjust(80, '0').rjust(100),
                     b'%100.80i', c_int(123))

        check_format('123'.rjust(10, '0'),
                     b'%010u', c_uint(123))
        check_format('123'.rjust(100),
                     b'%100u', c_uint(123))
        check_format('123'.rjust(100, '0'),
                     b'%.100u', c_uint(123))
        check_format('123'.rjust(80, '0').rjust(100),
                     b'%100.80u', c_uint(123))

        check_format('123'.rjust(10, '0'),
                     b'%010x', c_int(0x123))
        check_format('123'.rjust(100),
                     b'%100x', c_int(0x123))
        check_format('123'.rjust(100, '0'),
                     b'%.100x', c_int(0x123))
        check_format('123'.rjust(80, '0').rjust(100),
                     b'%100.80x', c_int(0x123))

        # test %A
        check_format(r"%A:'abc\xe9\uabcd\U0010ffff'",
                     b'%%A:%A', 'abc\xe9\uabcd\U0010ffff')

        # test %V
        check_format('repr=abc',
                     b'repr=%V', 'abc', b'xyz')

        # Test string decode from parameter of %s using utf-8.
        # b'\xe4\xba\xba\xe6\xb0\x91' is utf-8 encoded byte sequence of
        # '\u4eba\u6c11'
        check_format('repr=\u4eba\u6c11',
                     b'repr=%V', None, b'\xe4\xba\xba\xe6\xb0\x91')

        #Test replace error handler.
        check_format('repr=abc\ufffd',
                     b'repr=%V', None, b'abc\xff')

        # not supported: copy the raw format string. these tests are just here
        # to check for crashes and should not be considered as specifications
        check_format('%s',
                     b'%1%s', b'abc')
        check_format('%1abc',
                     b'%1abc')
        check_format('%+i',
                     b'%+i', c_int(10))
        check_format('%.%s',
                     b'%.%s', b'abc')

        # Issue #33817: empty strings
        check_format('',
                     b'')
        check_format('',
                     b'%s', b'')

    # Test PyUnicode_AsWideChar()
    @support.cpython_only
    @yp_unittest.skip_not_applicable
    def test_aswidechar(self):
        from _testcapi import unicode_aswidechar
        import_helper.import_module('ctypes')
        from ctypes import c_wchar, sizeof

        wchar, size = unicode_aswidechar('abcdef', 2)
        self.assertEqual(size, 2)
        self.assertEqual(wchar, 'ab')

        wchar, size = unicode_aswidechar('abc', 3)
        self.assertEqual(size, 3)
        self.assertEqual(wchar, 'abc')

        wchar, size = unicode_aswidechar('abc', 4)
        self.assertEqual(size, 3)
        self.assertEqual(wchar, 'abc\0')

        wchar, size = unicode_aswidechar('abc', 10)
        self.assertEqual(size, 3)
        self.assertEqual(wchar, 'abc\0')

        wchar, size = unicode_aswidechar('abc\0def', 20)
        self.assertEqual(size, 7)
        self.assertEqual(wchar, 'abc\0def\0')

        nonbmp = chr(0x10ffff)
        if sizeof(c_wchar) == 2:
            buflen = 3
            nchar = 2
        else: # sizeof(c_wchar) == 4
            buflen = 2
            nchar = 1
        wchar, size = unicode_aswidechar(nonbmp, buflen)
        self.assertEqual(size, nchar)
        self.assertEqual(wchar, nonbmp + '\0')

    # Test PyUnicode_AsWideCharString()
    @support.cpython_only
    @yp_unittest.skip_not_applicable
    def test_aswidecharstring(self):
        from _testcapi import unicode_aswidecharstring
        import_helper.import_module('ctypes')
        from ctypes import c_wchar, sizeof

        wchar, size = unicode_aswidecharstring('abc')
        self.assertEqual(size, 3)
        self.assertEqual(wchar, 'abc\0')

        wchar, size = unicode_aswidecharstring('abc\0def')
        self.assertEqual(size, 7)
        self.assertEqual(wchar, 'abc\0def\0')

        nonbmp = chr(0x10ffff)
        if sizeof(c_wchar) == 2:
            nchar = 2
        else: # sizeof(c_wchar) == 4
            nchar = 1
        wchar, size = unicode_aswidecharstring(nonbmp)
        self.assertEqual(size, nchar)
        self.assertEqual(wchar, nonbmp + '\0')

    # Test PyUnicode_AsUCS4()
    @support.cpython_only
    @yp_unittest.skip_not_applicable
    def test_asucs4(self):
        from _testcapi import unicode_asucs4
        for s in ['abc', '\xa1\xa2', '\u4f60\u597d', 'a\U0001f600',
                  'a\ud800b\udfffc', '\ud834\udd1e']:
            l = len(s)
            self.assertEqual(unicode_asucs4(s, l, yp_True), s+'\0')
            self.assertEqual(unicode_asucs4(s, l, yp_False), s+'\uffff')
            self.assertEqual(unicode_asucs4(s, l+1, yp_True), s+'\0\uffff')
            self.assertEqual(unicode_asucs4(s, l+1, yp_False), s+'\0\uffff')
            self.assertRaises(SystemError, unicode_asucs4, s, l-1, yp_True)
            self.assertRaises(SystemError, unicode_asucs4, s, l-2, yp_False)
            s = '\0'.join([s, s])
            self.assertEqual(unicode_asucs4(s, len(s), yp_True), s+'\0')
            self.assertEqual(unicode_asucs4(s, len(s), yp_False), s+'\uffff')

    # Test PyUnicode_AsUTF8()
    @support.cpython_only
    @yp_unittest.skip_not_applicable
    def test_asutf8(self):
        from _testcapi import unicode_asutf8

        bmp = '\u0100'
        bmp2 = '\uffff'
        nonbmp = chr(0x10ffff)

        self.assertEqual(unicode_asutf8(bmp), b'\xc4\x80')
        self.assertEqual(unicode_asutf8(bmp2), b'\xef\xbf\xbf')
        self.assertEqual(unicode_asutf8(nonbmp), b'\xf4\x8f\xbf\xbf')
        self.assertRaises(UnicodeEncodeError, unicode_asutf8, 'a\ud800b\udfffc')

    # Test PyUnicode_AsUTF8AndSize()
    @support.cpython_only
    @yp_unittest.skip_not_applicable
    def test_asutf8andsize(self):
        from _testcapi import unicode_asutf8andsize

        bmp = '\u0100'
        bmp2 = '\uffff'
        nonbmp = chr(0x10ffff)

        self.assertEqual(unicode_asutf8andsize(bmp), (b'\xc4\x80', 2))
        self.assertEqual(unicode_asutf8andsize(bmp2), (b'\xef\xbf\xbf', 3))
        self.assertEqual(unicode_asutf8andsize(nonbmp), (b'\xf4\x8f\xbf\xbf', 4))
        self.assertRaises(UnicodeEncodeError, unicode_asutf8andsize, 'a\ud800b\udfffc')

    # Test PyUnicode_FindChar()
    @support.cpython_only
    @yp_unittest.skip_not_applicable
    def test_findchar(self):
        from _testcapi import unicode_findchar

        for str in "\xa1", "\u8000\u8080", "\ud800\udc02", "\U0001f100\U0001f1f1":
            for i, ch in enumerate(str):
                self.assertEqual(unicode_findchar(str, ord(ch), 0, len(str), 1), i)
                self.assertEqual(unicode_findchar(str, ord(ch), 0, len(str), -1), i)

        str = "!>_<!"
        self.assertEqual(unicode_findchar(str, 0x110000, 0, len(str), 1), -1)
        self.assertEqual(unicode_findchar(str, 0x110000, 0, len(str), -1), -1)
        # start < end
        self.assertEqual(unicode_findchar(str, ord('!'), 1, len(str)+1, 1), 4)
        self.assertEqual(unicode_findchar(str, ord('!'), 1, len(str)+1, -1), 4)
        # start >= end
        self.assertEqual(unicode_findchar(str, ord('!'), 0, 0, 1), -1)
        self.assertEqual(unicode_findchar(str, ord('!'), len(str), 0, 1), -1)
        # negative
        self.assertEqual(unicode_findchar(str, ord('!'), -len(str), -1, 1), 0)
        self.assertEqual(unicode_findchar(str, ord('!'), -len(str), -1, -1), 0)

    # Test PyUnicode_CopyCharacters()
    @support.cpython_only
    @yp_unittest.skip_not_applicable
    def test_copycharacters(self):
        from _testcapi import unicode_copycharacters

        strings = [
            'abcde', '\xa1\xa2\xa3\xa4\xa5',
            '\u4f60\u597d\u4e16\u754c\uff01',
            '\U0001f600\U0001f601\U0001f602\U0001f603\U0001f604'
        ]

        for idx, from_ in enumerate(strings):
            # wide -> narrow: exceed maxchar limitation
            for to in strings[:idx]:
                self.assertRaises(
                    SystemError,
                    unicode_copycharacters, to, 0, from_, 0, 5
                )
            # same kind
            for from_start in range(5):
                self.assertEqual(
                    unicode_copycharacters(from_, 0, from_, from_start, 5),
                    (from_[from_start:from_start+5].ljust(5, '\0'),
                     5-from_start)
                )
            for to_start in range(5):
                self.assertEqual(
                    unicode_copycharacters(from_, to_start, from_, to_start, 5),
                    (from_[to_start:to_start+5].rjust(5, '\0'),
                     5-to_start)
                )
            # narrow -> wide
            # Tests omitted since this creates invalid strings.

        s = strings[0]
        self.assertRaises(IndexError, unicode_copycharacters, s, 6, s, 0, 5)
        self.assertRaises(IndexError, unicode_copycharacters, s, -1, s, 0, 5)
        self.assertRaises(IndexError, unicode_copycharacters, s, 0, s, 6, 5)
        self.assertRaises(IndexError, unicode_copycharacters, s, 0, s, -1, 5)
        self.assertRaises(SystemError, unicode_copycharacters, s, 1, s, 0, 5)
        self.assertRaises(SystemError, unicode_copycharacters, s, 0, s, 0, -1)
        self.assertRaises(SystemError, unicode_copycharacters, s, 0, b'', 0, 0)

    @support.cpython_only
    @support.requires_legacy_unicode_capi
    @yp_unittest.skip_not_applicable
    def test_encode_decimal(self):
        from _testcapi import unicode_encodedecimal
        with warnings_helper.check_warnings():
            warnings.simplefilter('ignore', DeprecationWarning)
            self.assertEqual(unicode_encodedecimal('123'),
                             b'123')
            self.assertEqual(unicode_encodedecimal('\u0663.\u0661\u0664'),
                             b'3.14')
            self.assertEqual(unicode_encodedecimal(
                             "\N{EM SPACE}3.14\N{EN SPACE}"), b' 3.14 ')
            self.assertRaises(UnicodeEncodeError,
                              unicode_encodedecimal, "123\u20ac", "strict")
            self.assertRaisesRegex(
                ValueError,
                "^'decimal' codec can't encode character",
                unicode_encodedecimal, "123\u20ac", "replace")

    @support.cpython_only
    @support.requires_legacy_unicode_capi
    @yp_unittest.skip_not_applicable
    def test_transform_decimal(self):
        from _testcapi import unicode_transformdecimaltoascii as transform_decimal
        with warnings_helper.check_warnings():
            warnings.simplefilter('ignore', DeprecationWarning)
            self.assertEqual(transform_decimal('123'),
                             '123')
            self.assertEqual(transform_decimal('\u0663.\u0661\u0664'),
                             '3.14')
            self.assertEqual(transform_decimal("\N{EM SPACE}3.14\N{EN SPACE}"),
                             "\N{EM SPACE}3.14\N{EN SPACE}")
            self.assertEqual(transform_decimal('123\u20ac'),
                             '123\u20ac')

    @support.cpython_only
    @yp_unittest.skip_not_applicable
    def test_pep393_utf8_caching_bug(self):
        # Issue #25709: Problem with string concatenation and utf-8 cache
        from _testcapi import getargs_s_hash
        for k in 0x24, 0xa4, 0x20ac, 0x1f40d:
            s = ''
            for i in range(5):
                # Due to CPython specific optimization the 's' string can be
                # resized in-place.
                s += chr(k)
                # Parsing with the "s#" format code calls indirectly
                # PyUnicode_AsUTF8AndSize() which creates the UTF-8
                # encoded string cached in the Unicode object.
                self.assertEqual(getargs_s_hash(s), chr(k).encode() * (i + 1))
                # Check that the second call returns the same result
                self.assertEqual(getargs_s_hash(s), chr(k).encode() * (i + 1))

@yp_unittest.skip_str_format
class StringModuleTest(yp_unittest.TestCase):
    def test_formatter_parser(self):
        def parse(format):
            return list(_string.formatter_parser(format))

        formatter = parse("prefix {2!s}xxx{0:^+10.3f}{obj.attr!s} {z[0]!s:10}")
        self.assertEqual(formatter, [
            ('prefix ', '2', '', 's'),
            ('xxx', '0', '^+10.3f', None),
            ('', 'obj.attr', '', 's'),
            (' ', 'z[0]', '10', 's'),
        ])

        formatter = parse("prefix {} suffix")
        self.assertEqual(formatter, [
            ('prefix ', '', '', None),
            (' suffix', None, None, None),
        ])

        formatter = parse("str")
        self.assertEqual(formatter, [
            ('str', None, None, None),
        ])

        formatter = parse("")
        self.assertEqual(formatter, [])

        formatter = parse("{0}")
        self.assertEqual(formatter, [
            ('', '0', '', None),
        ])

        self.assertRaises(TypeError, _string.formatter_parser, 1)

    def test_formatter_field_name_split(self):
        def split(name):
            items = list(_string.formatter_field_name_split(name))
            items[1] = list(items[1])
            return items
        self.assertEqual(split("obj"), ["obj", []])
        self.assertEqual(split("obj.arg"), ["obj", [(yp_True, 'arg')]])
        self.assertEqual(split("obj[key]"), ["obj", [(yp_False, 'key')]])
        self.assertEqual(split("obj.arg[key1][key2]"), [
            "obj",
            [(yp_True, 'arg'),
             (yp_False, 'key1'),
             (yp_False, 'key2'),
            ]])
        self.assertRaises(TypeError, _string.formatter_field_name_split, 1)


if __name__ == "__main__":
    yp_unittest.main()