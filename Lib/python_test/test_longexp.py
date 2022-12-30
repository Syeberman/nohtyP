from python_test import yp_unittest

@yp_unittest.skip_not_applicable
class LongExpText(yp_unittest.TestCase):
    def test_longexp(self):
        REPS = 65580
        l = eval("[" + "2," * REPS + "]")
        self.assertEqual(len(l), REPS)

if __name__ == "__main__":
    yp_unittest.main()
