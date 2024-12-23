This is nohtyP version 0.5.0
============================

Copyright (c) 2001 Python Software Foundation. All rights reserved.

nohtyP is a pure-C implementation of the Python 3.x built-in library. It is implemented in one .c
and one .h file for easy embedding in C projects. The API is designed to compensate for C's
weaknesses in handling "exceptions". nohtyP is tested against Python's test suite to ensure
conformance.

[![][AppVeyor badge]][AppVeyor log]
[![][Codecov badge]][Codecov project]

CI is run against Linux, MacOS, and Windows with the generous support of [AppVeyor] and [Codecov].


Build Instructions
------------------

In the simplest case, add `nohtyP.c` and `nohtyP.h` to your C project, and call `yp_initialize` on
start-up. nohtyP also builds as a shared library.

The Build directory contains an optional SCons makefile that supports various compilers and targets.
To build a shared library for your platform, run the following from the top-level directory:

    ./Build/make

This will build both a debug and a release version for your native OS and architecture, and will
copy the binaries to `Build/native`. For more information, run the above command with the `-h`
option.


Documentation
-------------

`nohtyP.h` contains complete, albeit brief, documentation on all functions; its brevity is
best-suited for those already familiar with Python. More detailed information can be found in
Python's own documentation:

> http://docs.python.org/3/


Testing
-------

To test nohtyP, change to the `Lib` directory, set `NOHTYP_LIBRARY` to the path to the shared
library (i.e. `../Build/native/libnohtyP.so`), then run:

    python -m python_test

The test set produces some output. You can generally ignore the messages about skipped tests due to
optional features which can't be imported. If a message is printed about a failed test or a
traceback or a core dump is produced, something is wrong.

If the tests fail and you decide to file a bug report, please provide the output from the following
command:

    python -m python_test -v test_whatever

This runs the test in verbose mode. You can also obtain this output using the `test` target in
`./Build/make`, which creates a `python_test.log` file in the same directory as the shared library.


Issue Tracker and Mailing List
------------------------------

GitHub is used to track bug reports and feature requests:

> https://github.com/Syeberman/nohtyP/issues

Feedback is extremely welcome, as are pull requests.


Copyright and License Information
---------------------------------

As a derivative work of Python, nohtyP shares the same copyrights and licenses.

Copyright (c) 2001 Python Software Foundation. All rights reserved.

Copyright (c) 2000 BeOpen.com. All rights reserved.

Copyright (c) 1995-2001 Corporation for National Research Initiatives. All rights reserved.

Copyright (c) 1991-1995 Stichting Mathematisch Centrum. All rights reserved.

See the file "LICENSE" for information on the history of this software, terms & conditions for
usage, and a DISCLAIMER OF ALL WARRANTIES.

nohtyP is patterned after, and contains source from, Python 2.7 and above. nohtyP has always been
released under a Python Software Foundation (PSF) license. Source taken from Python comes from
distributions containing *no* GNU General Public License (GPL) code. As such, nohtyP may be used in
proprietary projects. There are interfaces to some GNU code but these are entirely optional.

All trademarks referenced herein are property of their respective holders.


[AppVeyor badge]: https://ci.appveyor.com/api/projects/status/8t43r157h40vmfu6/branch/main?svg=true
[AppVeyor log]: https://ci.appveyor.com/project/Syeberman/nohtyp/branch/main
[AppVeyor]: http://ci.appveyor.com
[Codecov badge]: https://codecov.io/github/Syeberman/nohtyP/branch/main/graph/badge.svg?token=fYVEwGJz7W
[Codecov project]: https://app.codecov.io/github/Syeberman/nohtyP/tree/main
[Codecov]: https://about.codecov.io/
