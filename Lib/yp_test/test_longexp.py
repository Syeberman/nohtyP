from yp import *
import unittest
from yp_test import support

@unittest.skip( "TODO: convert to yp.py" )
class LongExpText(unittest.TestCase):
    def test_longexp(self):
        REPS = 65580
        l = eval("[" + "2," * REPS + "]")
        self.assertEqual(len(l), REPS)

def test_main():
    support.run_unittest(LongExpText)

if __name__=="__main__":
    test_main()
