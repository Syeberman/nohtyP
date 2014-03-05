# XXX NOT a SCons tool module; instead, a library for the python_* tool modules

import os, os.path, sys, functools, subprocess, re, glob, tempfile
import SCons.Errors
import SCons.Platform
import SCons.Tool
import SCons.Warnings

_platform_name = SCons.Platform.Platform( ).name
if _platform_name in ("win32", "cygwin"):
    # Try some known, and unknown-but-logical, default locations for Python
    _python_path_globs = (
            "C:\\Python*\\",
            "C:\\Program Files\\Python*",
            "C:\\Program Files (x86)\\Python*",
            )
    _python_paths_found = [
            pyDir for pattern in _python_path_globs for pyDir in glob.iglob( pattern ) ]
else:
    _python_paths_found = []   # rely on the environment's path for now


def _arch2maxsize( targ_arch ):
    arch2opts = {
            "x86": 0x7FFFFFFF,
            "amd64": 0x7FFFFFFFFFFFFFFF,
            }
    try: opts = arch2opts[targ_arch]
    except KeyError: SCons.Errors.StopError( "not yet supporting %r with python" % targ_arch )
    return opts

_test_python_cache = {}
def _test_python( python, targ_hexversion, targ_arch ):
    """Tests if the given python matches the version, and supports the target arch.  Caches
    results for speed."""
    # Get this first so we can fail quickly on new architectures
    targ_maxsize = _arch2maxsize( targ_arch )

    # See if we've tested this executable before
    try: output = _test_python_cache[python]
    except KeyError:
        try: output = subprocess.check_output( [python, 
            "-c", "import sys; print( (sys.hexversion, sys.maxsize) )"], stderr=subprocess.PIPE )
        except: output = ""
        output = output.strip( )
        _test_python_cache[python] = output
    
    # Parse the tuple that was output, failing on error, then see if it meets our requirements
    try: hexversion, maxsize = eval( output, {"__builtins__": None} )
    except: return False
    if hexversion < targ_hexversion: return False
    if maxsize != targ_maxsize: return False
    return True

# TODO This, or something like it, should be in SCons
def _find( env, targ_hexversion ):
    """Find a python executable that can run our target's arch, returning the path or None."""
    if env["TARGET_OS"] != env["HOST_OS"]: raise SCons.Errors.StopError( "can only run Python on the native OS" )
    pyDirs = list( os.environ.get( "PATH", "" ).split( os.pathsep ) )
    pyDirs.extend( _python_paths_found )
    for pyDir in pyDirs:
        python = os.path.join( pyDir, "python" )
        supported = _test_python( python, targ_hexversion, env["TARGET_ARCH"] )
        if supported: return python
    return None


def DefinePythonToolFunctions( hexversion, strversion ):
    """Returns (generate, exists), suitable for use as the SCons tool module functions."""

    def generate( env ):
        # TODO Read info from site-tools.py to make this compiler easier to find
        # TODO Make hexversion a range, then pick the Python version that's the latest that fits

        # Find a Python that supports our target, and prepend it to the path
        python_path = _find( env, hexversion )
        if not python_path:
            raise SCons.Errors.StopError( "python %s (%r) detection failed" % (strversion, env["TARGET_ARCH"]) )
        path, python = os.path.split( python_path )
        env.PrependENVPath( "PATH", path )
        env["PYTHON"] = python

        # TODO Add an entry for this compiler in site-tools.py if it doesn't already exist?

    def exists( env ):
        return True # FIXME? _find( env, hexversion )

    return generate, exists

