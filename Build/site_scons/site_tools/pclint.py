
# XXX This supports both PC-lint and FlexeLint by Gimpel Software

import os, os.path, sys, functools, subprocess, re, glob, tempfile
import SCons.Errors
import SCons.Platform
import SCons.Tool
import SCons.Warnings

_tool_name = __name__   # ie "pclint"

_platform_name = SCons.Platform.Platform( ).name
if _platform_name in ("win32", "cygwin"):
    # Try some known, and unknown-but-logical, default locations for PC-lint
    _pclint_path_globs = (
            "C:\\lint\\",
            "C:\\Program Files\\lint",
            "C:\\Program Files (x86)\\lint",
            "C:\\PC-Lint\\",
            "C:\\Program Files\\PC-Lint",
            "C:\\Program Files (x86)\\PC-Lint",
            )
    _pclint_paths_found = [
            pclintDir for pattern in _pclint_path_globs for pclintDir in glob.iglob( pattern ) ]
    _pclint_exename = "lint-nt"
else:
    _pclint_paths_found = []   # rely on the environment's path for now


_re_pclint_version = re.compile( r"Vers\. (\d+)\.(\d+)([a-z])\," )
_test_pclint_cache = {}
def _test_pclint( pclint ):
    """Tests if the given pclint exists, and return its version if so, or None if not.  Caches 
    results for speed."""
    # See if we've tested this executable before
    try: output = _test_pclint_cache[pclint]
    except KeyError:
        try: output = subprocess.check_output( [pclint, "+b"] )
        except: output = ""
        output = output.strip( )
        _test_pclint_cache[pclint] = output
    
    # Parse the version:
    #   PC-lint for C/C++ (NT) Vers. 9.00k, Copyright Gimpel Software 1985-2013
    match = _re_pclint_version.search( output )
    if match is None: return None
    return (int( match.group( 1 ) ), int( match.group( 2 ) ), match.group( 3 ) )

# TODO This, or something like it, should be in SCons
def _find( env ):
    """Find a PC-Lint executable returning the path or None.  Picks the executable with the 
    largest version number."""
    pclintDirs = list( os.environ.get( "PATH", "" ).split( os.pathsep ) )
    pclintDirs.extend( _pclint_paths_found )
    pclints = []
    for pclintDir in pclintDirs:
        pclint = os.path.join( pclintDir, _pclint_exename )
        pclint_version = _test_pclint( pclint )
        if pclint_version is None: continue
        pclints.append( (pclint_version, pclint) )
    if not pclints: return None
    pclints.sort( )         # sort by version
    return pclints[-1][1]   # return the path with the largest version

def generate( env ):
    # See if site-tools.py already knows where to find this PC-Lint version
    siteTools_name, siteTools_dict = env["SITE_TOOLS"]( )
    pclint_siteName = _tool_name.upper( )
    pclint_path = siteTools_dict.get( pclint_siteName, None )

    # If site-tools.py came up empty, find a PC-Lint that supports our target, then update 
    # site-tools.py
    if not pclint_path:
        pclint_path = _find( env )
        if not pclint_path:
            raise SCons.Errors.StopError( "%s detection failed" % _tool_name )
        siteTools_dict[pclint_siteName] = pclint_path
        with open( siteTools_name, "a" ) as outfile:
            outfile.write( "%s = %r\n\n" % (pclint_siteName, pclint_path) )

    # Now, prepend it to the path
    path, pclint = os.path.split( pclint_path )
    env.PrependENVPath( "PATH", path )
    env["PCLINT"] = pclint

def exists( env ):
    return True # TODO? _find( env )


"""
lint-nt.exe

// This will depend on if we're running on "debug" or "analyze"
// TODO do we also want to support "release"?  I don't think so, because asserts help lint...
-w2

// Some compilers' headers have long lines
+linebuf

// Don't warn on sign differences in char pointers
-epuc

// The last value of args by va_arg (or va_end?) looks like it's not being used
// TODO more-precise fix needed
-e438

// "For clause irregularity: variable tested in 2nd expression does not match that modified in 3rd"
-e440

// Some if statements always evaluate to true/false on certain targets: compiler can easily remove
-e506 -e685

// Can ignore return value
-ecall(534,yp_incref)

// div is a function defined in compiler headers
-esym(578,div)

// We perform some questionable operations in yp_STATIC_ASSERT in order to test the compiler
-emacro(572,yp_STATIC_ASSERT) -emacro(649,yp_STATIC_ASSERT)

// These macros appear to dereference NULL...but only to get the size/offset of a member
-emacro((413),yp_sizeof_member,yp_offsetof)

// We don't necessarily use all these tp_* stubs, and that's OK (the compiler will remove anyway)
-esym(528,MethodError_*,TypeError_*,InvalidatedError_*,ExceptionMethod_*)

// ypTypeTable is a static array that must be declared at the top and defined at the bottom...but
// the strict C standard doesn't allow for that (although compilers do)
-esym(31,ypTypeTable)

// Some compilers' headers have data following incomplete arrays (?!)
-elib(157)

// Will need to pull this information from SCons
co-msc100.lnt -Dyp_DEBUG_LEVEL=1 nohtyP.c

"""
