"""yp_unittest\case.py - Wraps Python's unittest to ensure we're actually testing nohtyP

Sye van der Veen
November 5, 2013
"""

from unittest.case import *
import unittest as _unittest
import contextlib as _contextlib
import yp as _yp

# The danger of using Python's test suite to test nohtyP is that we might forget to convert an
# object to nohtyP; this is designed 
def _checkFornohtyP(*objs):
    for obj in objs:
        if isinstance(obj, _yp.ypObject): return
        if isinstance(obj, type) and issubclass(obj, _yp.ypObject): return
    raise TypeError("expected at least one ypObject in assertion")

@_contextlib.contextmanager
def _nohtyPCheckChange(case, enabled):
    old = case._nohtyPCheckEnabled
    case._nohtyPCheckEnabled = enabled
    try: yield
    finally: case._nohtyPCheckEnabled = old

class TestCase(_unittest.TestCase):

    def __init__(self, *args, **kwargs):
        _unittest.TestCase.__init__(self, *args, **kwargs)
        self._nohtyPCheckEnabled = True

    def nohtyPCheck(self, enabled):
        """Use as a context manager to enable/disable checking for nohtyP types.

            with self.nohtyPCheck(enabled=False):
                do_something()
        """
        return _nohtyPCheckChange(self, enabled)

    def assertFalse(self, expr, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(expr)
        _unittest.TestCase.assertFalse(self, expr, msg)

    def assertTrue(self, expr, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(expr)
        _unittest.TestCase.assertTrue(self, expr, msg)

    # TODO Implement for nohtyP
    #def assertRaises(self, excClass, callableObj=None, *args, **kwargs):

    def assertEqual(self, first, second, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(first, second)
        _unittest.TestCase.assertEqual(self, first, second, msg)

    def assertNotEqual(self, first, second, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(first, second)
        _unittest.TestCase.assertNotEqual(self, first, second, msg)

    def assertAlmostEqual(self, first, second, places=None, msg=None,
                          delta=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(first, second)
        _unittest.TestCase.assertAlmostEqual(self, first, second, places, msg,
                          delta)

    def assertNotAlmostEqual(self, first, second, places=None, msg=None,
                             delta=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(first, second)
        _unittest.TestCase.assertNotAlmostEqual(self, first, second, places, msg,
                             delta)

    def assertSequenceEqual(self, seq1, seq2, msg=None, seq_type=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(seq1, seq2)
        _unittest.TestCase.assertSequenceEqual(self, seq1, seq2, msg, seq_type)

    def assertListEqual(self, list1, list2, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(list1, list2)
        _unittest.TestCase.assertListEqual(self, list1, list2, msg)
    
    def assertTupleEqual(self, tuple1, tuple2, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(tuple1, tuple2)
        _unittest.TestCase.assertTupleEqual(self, tuple1, tuple2, msg)

    def assertSetEqual(self, set1, set2, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(set1, set2)
        _unittest.TestCase.assertSetEqual(self, set1, set2, msg)

    def assertIn(self, member, container, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(member, container)
        _unittest.TestCase.assertIn(self, member, container, msg)

    def assertNotIn(self, member, container, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(member, container)
        _unittest.TestCase.assertNotIn(self, member, container, msg)

    def assertIs(self, expr1, expr2, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(expr1, expr2)
        _unittest.TestCase.assertIs(self, expr1, expr2, msg)

    def assertIsNot(self, expr1, expr2, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(expr1, expr2)
        _unittest.TestCase.assertIsNot(self, expr1, expr2, msg)

    def assertDictEqual(self, d1, d2, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(d1, d2)
        _unittest.TestCase.assertDictEqual(self, d1, d2, msg)

    def assertDictContainsSubset(self, subset, dictionary, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(subset, dictionary)
        _unittest.TestCase.assertDictContainsSubset(self, subset, dictionary, msg)

    def assertCountEqual(self, first, second, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(first, second)
        _unittest.TestCase.assertCountEqual(self, first, second, msg)

    def assertMultiLineEqual(self, first, second, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(first, second)
        _unittest.TestCase.assertMultiLineEqual(self, first, second, msg)

    def assertLess(self, a, b, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(a, b)
        _unittest.TestCase.assertLess(self, a, b, msg)

    def assertLessEqual(self, a, b, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(a, b)
        _unittest.TestCase.assertLessEqual(self, a, b, msg)

    def assertGreater(self, a, b, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(a, b)
        _unittest.TestCase.assertGreater(self, a, b, msg)

    def assertGreaterEqual(self, a, b, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(a, b)
        _unittest.TestCase.assertGreaterEqual(self, a, b, msg)

    def assertIsNone(self, obj, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(obj)
        _unittest.TestCase.assertIsNone(self, obj, msg)

    def assertIsNotNone(self, obj, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(obj)
        _unittest.TestCase.assertIsNotNone(self, obj, msg)

    def assertIsInstance(self, obj, cls, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(obj)
        _unittest.TestCase.assertIsInstance(self, obj, cls, msg)

    def assertNotIsInstance(self, obj, cls, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(obj)
        _unittest.TestCase.assertNotIsInstance(self, obj, cls, msg)

    def assertRaisesRegex(self, expected_exception, expected_regex,
                          callable_obj=None, *args, **kwargs):
        raise NotImplementedError("assertRaisesRegex not applicable to nohtyP")

    def assertRegex(self, text, expected_regex, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(text)
        _unittest.TestCase.assertRegex(self, text, expected_regex, msg)

    def assertNotRegex(self, text, unexpected_regex, msg=None):
        if self._nohtyPCheckEnabled: _checkFornohtyP(text)
        _unittest.TestCase.assertNotRegex(self, text, unexpected_regex, msg)



