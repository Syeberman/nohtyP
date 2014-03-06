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
def _test_python( python, targ_hexversions, targ_arch ):
    """Tests if the given python matches the version, and supports the target arch.  Returns
    hexversion if so, None if not.  Caches results for speed."""
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
    except: return None
    if hexversion not in targ_hexversions: return None
    if maxsize != targ_maxsize: return None
    return hexversion

# TODO This, or something like it, should be in SCons
def _find( env, targ_hexversions ):
    """Find a python executable that can run our target's arch, returning the path or None.
    Picks the executable with the largest hexversion contained in targ_hexversions."""
    if env["TARGET_OS"] != env["HOST_OS"]: raise SCons.Errors.StopError( "can only run Python on the native OS" )
    pyDirs = list( os.environ.get( "PATH", "" ).split( os.pathsep ) )
    pyDirs.extend( _python_paths_found )
    pythons = []
    for pyDir in pyDirs:
        python = os.path.join( pyDir, "python" )
        python_hexversion = _test_python( python, targ_hexversions, env["TARGET_ARCH"] )
        if python_hexversion is None: continue
        pythons.append( (python_hexversion, python) )
    if not pythons: return None
    pythons.sort( )         # sort by hexversion
    return pythons[-1][1]   # return the python path with the largest hexversion

def DefinePythonToolFunctions( hexversions, strversion ):
    """Returns (generate, exists), suitable for use as the SCons tool module functions.  
    hexversions is a container (possibly a range) of acceptable values of hexversion."""

    def generate( env ):
        # TODO Read info from site-tools.py to make this compiler easier to find

        # Find a Python that supports our target, and prepend it to the path
        python_path = _find( env, hexversions )
        if not python_path:
            raise SCons.Errors.StopError( "python %s (%r) detection failed" % (strversion, env["TARGET_ARCH"]) )
        path, python = os.path.split( python_path )
        env.PrependENVPath( "PATH", path )
        env["PYTHON"] = python

        # TODO Add an entry for this compiler in site-tools.py if it doesn't already exist?

    def exists( env ):
        return True # FIXME? _find( env, hexversions )

    return generate, exists

