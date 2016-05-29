# XXX NOT a SCons tool module; instead, a library for the python_* tool modules

import os
import os.path
import subprocess
import glob
import SCons.Errors
import SCons.Platform
import SCons.Tool
import SCons.Warnings

_platform_name = SCons.Platform.Platform().name
if _platform_name in ("win32", "cygwin"):
    # Try some known, and unknown-but-logical, default locations for Python
    _python_dir_globs = (
        "C:\\Python*",
        "C:\\Program Files\\Python*",
        "C:\\Program Files (x86)\\Python*",
        )
    _python_exe_globs = (
        "python.exe",
        )
else:
    _python_dir_globs = ()  # rely on the environment's path for now
    _python_exe_globs = (
        "python[0-9].[0-9]",
        "python",
        )


def _arch2maxsize(targ_arch):
    arch2opts = {
        "x86": 0x7FFFFFFF,
        "amd64": 0x7FFFFFFFFFFFFFFF,
        }
    try:
        opts = arch2opts[targ_arch]
    except KeyError:
        raise SCons.Errors.StopError("not yet supporting %r with python" % targ_arch)
    return opts

_test_python_cache = {}


def _test_python(python, targ_hexversions, targ_arch):
    """Tests if the given python matches the version, and supports the target arch.  Returns
    hexversion if so, None if not.  Caches results for speed."""
    # Get this first so we can fail quickly on new architectures
    targ_maxsize = _arch2maxsize(targ_arch)

    # See if we've tested this executable before
    try:
        output = _test_python_cache[python]
    except KeyError:
        try:
            output = subprocess.check_output([python,
                                              "-c", "import sys; print( (sys.hexversion, sys.maxsize) )"], stderr=subprocess.PIPE)
        except:
            output = ""
        output = output.strip()
        _test_python_cache[python] = output

    # Parse the tuple that was output, failing on error, then see if it meets our requirements
    try:
        hexversion, maxsize = eval(output, {"__builtins__": None})
    except:
        return None
    if hexversion not in targ_hexversions:
        return None
    if maxsize != targ_maxsize:
        return None
    return hexversion

# TODO This, or something like it, should be in SCons


def _find(env, targ_hexversions):
    """Find a python executable that can run our target's arch, returning the path or None.
    Picks the executable with the largest hexversion contained in targ_hexversions."""
    dir_globs = list(os.environ.get("PATH", "").split(os.pathsep))
    dir_globs.extend(_python_dir_globs)

    python_globs = (os.path.join(dir, exe) for exe in _python_exe_globs for dir in dir_globs)
    pythons = (python for pattern in python_globs for python in glob.iglob(pattern))

    versions = []
    for python in pythons:
        python_hexversion = _test_python(python, targ_hexversions, env["TARGET_ARCH"])
        if python_hexversion is None:
            continue
        versions.append((python_hexversion, python))

    if not versions:
        return None
    versions.sort()         # sort by hexversion
    return versions[-1][1]   # return the python path with the largest hexversion


def DefinePythonToolFunctions(hexversions, tool_name):
    """Returns (generate, exists), suitable for use as the SCons tool module functions.
    hexversions is a container (possibly a range) of acceptable values of hexversion."""
    if tool_name == "__main__":
        raise ImportError("this tool module cannot be run as a script")

    def generate(env):
        if env["TARGET_OS"] != env["HOST_OS"]:
            raise SCons.Errors.StopError("can only run Python on the native OS")

        # See if site_toolsconfig.py already knows where to find this Python version
        toolsConfig = env["TOOLS_CONFIG"]
        python_siteName = "%s_%s" % (tool_name.upper(), env["TARGET_ARCH"].upper())
        python_path = toolsConfig.get(python_siteName, "")
        if python_path is None:
            raise SCons.Errors.StopError("%s (%r) disabled in %s" % (
                tool_name, env["TARGET_ARCH"], toolsConfig.basename))

        # If site_toolsconfig.py came up empty, find a Python that supports our target, then update
        if not python_path:
            python_path = _find(env, hexversions)
            if not python_path:
                raise SCons.Errors.StopError("%s (%r) detection failed" %
                                             (tool_name, env["TARGET_ARCH"]))
            toolsConfig.update({python_siteName: python_path})

        # Now, prepend it to the path
        path, python = os.path.split(python_path)
        env.PrependENVPath("PATH", path)
        env["PYTHON"] = python

    def exists(env):
        # We rely on generate to tell us if a tool is available
        return True

    return generate, exists
