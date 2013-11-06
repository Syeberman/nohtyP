from yp import *
from yp_test import yp_unittest
from yp_test import support

@yp_unittest.skip( "TODO: convert to yp.py" )
class LongExpText(yp_unittest.TestCase):
    def test_longexp(self):
        REPS = 65580
        l = eval("[" + "2," * REPS + "]")
        self.assertEqual(len(l), REPS)

def test_main():
    support.run_unittest(LongExpText)

if __name__=="__main__":
    test_main()
