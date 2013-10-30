This is nohtyP version 0.5.0
============================

Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011,
2012 Python Software Foundation.  All rights reserved.

nohtyP is a pure-C implementation of the Python 3.x built-in library.  It is
implemented in one .c and one .h file for easy embedding in C projects.  The
API is designed to compensate for C's weaknesses in handling "exceptions".
nohtyP is tested against Python's test suite to ensure conformance.


Build Instructions
------------------

In the simplest case, just add nohtyP.c and nohtyP.h to your C project, and
call yp_initialize on start-up.  nohtyP also builds as a DLL.

The Build directory contains makefiles for various compilers and targets; for
example, the following will build nohtyP.dll under Visual Studio 10:

    Build/VS10.0/ypDLL.vcxproj


Documentation
-------------

nohtyP.h contains complete, albeit brief, documentation on all functions; its
brevity is best-suited for those already familiar with Python.  More detailed
information can be found in Python's own documentation:

    http://docs.python.org/3/


Testing
-------

To test nohtyP, first build a dynamically-linked library, and ensure it is
listed on your PATH.  Then, in the Lib directory, run:

    python -m yp_test

The test set produces some output.  You can generally ignore the messages 
about skipped tests due to optional features which can't be imported.  If a
message is printed about a failed test or a traceback or core dump is 
produced, something is wrong.

IMPORTANT: If the tests fail and you decide to file a bug report, *don't*
include the normal output.  It is useless.  Run the failing test manually, 
as follows:

    python -m yp_test -v test_whatever

This runs the test in verbose mode.


Issue Tracker and Mailing List
------------------------------

Bitbucket is used to track bug reports and feature requests:

    http://bitbucket.org/Syeberman/nohtyp/issues

Feedback is extremely welcome, as are pull requests.


Copyright and License Information
---------------------------------

As a derivative work of Python, nohtyP shares the same copyrights and
licenses.

Copyright (c) 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011
Python Software Foundation.  All rights reserved.

Copyright (c) 2000 BeOpen.com.  All rights reserved.

Copyright (c) 1995-2001 Corporation for National Research Initiatives.  All
rights reserved.

Copyright (c) 1991-1995 Stichting Mathematisch Centrum.  All rights reserved.

See the file "LICENSE" for information on the history of this software, terms &
conditions for usage, and a DISCLAIMER OF ALL WARRANTIES.

nohtyP is patterned after, and contains source from, Python 2.7 and above.
nohtyP has always been released under a Python Software Foundation (PSF) 
license.  Source taken from Python comes from distributions containing *no*
GNU General Public License (GPL) code.  As such, nohtyP may be used in 
proprietary projects.

All trademarks referenced herein are property of their respective holders.

