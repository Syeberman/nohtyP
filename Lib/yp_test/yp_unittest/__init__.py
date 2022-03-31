# Take everything that Python's unittest module exposes, but override with our custom TestCase
from unittest import *
from .case import TestCase

# Easy-to-search "skip" decorators for various features we have yet to support.
skip_array = skip("TODO: Support yp_array?")
skip_cmp_recursion = skip("TODO: Implement protection against recursion in yp_eq/etc")
skip_complex = skip("TODO: Implement yp_complex?")
skip_deepcopy_memo = skip("TODO: Support memo argument in yp_deepcopy")
skip_dict_deepcopy = skip("TODO: Support yp_dict.__deepcopy__")
skip_dict_mutating_iteration = skip("TODO: Support yp_dict iteration mutation detection")
skip_dict_order = skip("TODO: Support ordered yp_dict")
skip_dict_views = skip("TODO: Support keys, values, and items views on yp_dict")
skip_exception_messages = skip("TODO: Support exception instances (with messages) in nohtyP")
skip_files = skip("TODO: Implement files in nohtyP")
skip_filter = skip("TODO: Implement yp_filter")
skip_floats = skip("TODO: Implement yp_float methods")
skip_func_ascii = skip("TODO: Implement ascii() in nohtyP?")
skip_function_deepcopy = skip("TODO: Support yp_function.__deepcopy__")
skip_hash = skip("TODO: Update to test yp_hash")  # TODO It's the tests that need updating
skip_int_bit_count = skip("TODO: Implement yp_int.bit_count")
skip_int_fromunicode = skip("TODO: Support non-latin-1 characters in yp_int-from-yp_str")
skip_int_to_bytes = skip("TODO: Implement yp_int.to_bytes")
skip_int_underscores = skip("TODO: Support underscores in yp_int")
skip_list_mutating_iteration = skip("TODO: Support yp_list iteration mutation detection")
skip_long_ints = skip("TODO: Support unbounded precision in yp_int")
skip_map = skip("TODO: Implement yp_map")
skip_marshal = skip("TODO: Implement marshal in nohtyP?")
skip_math = skip("The math module is not supported in nohtyP")
skip_memoryview = skip("TODO: Implement memoryview in nohtyP?")
skip_min = skip("TODO: Implement yp_min/yp_max")
skip_not_applicable = skip("Not applicable to nohtyP")  # TODO Verify these are truly non-nohtyP
skip_num_integer_ratio = skip("TODO: Support integer_ratio")
skip_num_methods = skip("TODO: Support overriding the number methods (yp_add/etc) for any object")
skip_ord = skip("TODO: Support ord in nohtyP")  # TODO Really easy...
skip_pickling = skip("TODO: Implement nohtyP pickling")
skip_range_attributes = skip("TODO: Implement yp_range attributes")
skip_regexp = skip("TODO: Support regular expressions in nohtyP")
skip_str_big_chars = skip("TODO: Improve support for chars >latin-1")
skip_str_case = skip("TODO: Implement yp_str.lower/swapcase/islower/etc")
skip_str_codecs = skip("TODO: Support additional str codecs in nohtyP")  # TODO Fake it first.
skip_str_count = skip("TODO: Implement yp_str.count")
skip_str_format = skip("TODO: Implement yp_str.format")
skip_str_printf = skip("printf-style string formatting not supported in nohtyP")  # Unneeded?
skip_str_replace = skip("TODO: Implement yp_str.replace/translate/etc")
skip_str_repr = skip("TODO: Implement yp_str/yp_repr")
skip_str_space = skip("TODO: Implement yp_str.expandtabs/strip/ljust/isspace/etc")
skip_str_split = skip("TODO: Implement yp_str.split/splitlines/partition/etc")
skip_str_unicode_db = skip("TODO: Implement yp_str.isalpha/isdigit/etc")
skip_str_zfill = skip("TODO: Implement yp_str.zfill")
skip_string_module = skip("TODO: Implement string module in nohtyP?")
skip_sys_getsizeof = skip("TODO: Implement sys.getsizeof in nohtyP?")
skip_template = skip("TODO: Implement Template in nohtyP?") # Unlikely...
skip_unpack = skip("TODO: Test yp_unpackN")  # TODO Just need the tests
skip_user_defined_types = skip("TODO: Implement user-defined types in nohtyP")
skip_weakref = skip("TODO: Implement weakref in nohtyP")
skip_zip = skip("TODO: Implement yp_zip")
