# XXX NOT a SCons tool module; instead, a library for the python_* tool modules

import ast
import os
import os.path
import re
import subprocess
import SCons.Errors
import SCons.Platform
import SCons.Tool
import SCons.Warnings
from root_environment import SconscriptLog
from tool_finder import ToolFinder

re_python_stem = re.compile(r"python([0-9.]+)?")


def _version_detector(python):
    """Returns (hexversion, maxsize) for the given Python executable, or (None, None) on error."""
    SconscriptLog.write(f"Detecting version of {python}\n")

    # Our exe_globs picks up related tools
    if not re_python_stem.fullmatch(python.stem):
        return None, None

    try:
        output = subprocess.check_output(
            [
                str(python),
                "-c",
                "import sys; print('(%d, %d)' % (sys.hexversion, sys.maxsize))",
            ],
            stderr=subprocess.PIPE,
        ).decode()
        hexversion, maxsize = ast.literal_eval(output.strip())
        return hexversion, maxsize
    except subprocess.CalledProcessError:
        return None, None


python_finder = ToolFinder(
    win_dirs=("Python*",),
    posix_dirs=(),  # rely on the environment's path for now
    darwin_dirs=(),  # rely on the environment's path for now
    exe_globs=("python*.*", "python?*", "python"),  # prefer specificity
    version_detector=_version_detector,
)


def _arch2maxsize(targ_arch):
    arch2maxsize = {
        "x86": 0x7FFFFFFF,
        "amd64": 0x7FFFFFFFFFFFFFFF,
    }
    try:
        maxsize = arch2maxsize[targ_arch]
    except KeyError:
        raise SCons.Errors.StopError("not yet supporting %r with python" % targ_arch)
    return maxsize


def _find(env, targ_hexversions):
    """Find a python executable that can run our target's arch, returning the path or None.
    Picks the executable with the largest hexversion contained in targ_hexversions."""
    targ_maxsize = _arch2maxsize(env["TARGET_ARCH"])

    versions = []
    for python, (hexversion, maxsize) in python_finder:
        if hexversion in targ_hexversions and maxsize == targ_maxsize:
            versions.append((hexversion, python))

    if not versions:
        return None

    versions.sort()  # sort by hexversion
    # TODO It'd be nice to use Path objects everywhere we can
    return str(versions[-1][1])  # return the python path with the largest hexversion


def DefinePythonToolFunctions(hexversions, tool_name):
    """Returns (generate, exists), suitable for use as the SCons tool module functions.
    hexversions is a container (possibly a range) of acceptable values of hexversion."""
    if tool_name == "__main__":
        raise ImportError("this tool module cannot be run as a script")

    # FIXME If the right Python cannot be found, then each time we clone an environment we take a
    # long time iterating in _find. This takes a long time despite the caching in ToolFinder.
    # Figure out why and optimize.
    def generate(env):
        if env["TARGET_OS"] != env["HOST_OS"]:
            raise SCons.Errors.StopError("can only run Python on the native OS")

        # See if site_toolsconfig.py already knows where to find this Python version
        toolsConfig = env["TOOLS_CONFIG"]
        python_siteName = "%s_%s" % (tool_name.upper(), env["TARGET_ARCH"].upper())
        python_path = toolsConfig.get(python_siteName, "")
        if python_path is None:
            raise SCons.Errors.StopError(
                "%s (%r) disabled in %s"
                % (tool_name, env["TARGET_ARCH"], toolsConfig.basename)
            )

        # If site_toolsconfig.py came up empty, find a Python that supports our target, then update
        if not python_path:
            python_path = _find(env, hexversions)
            toolsConfig.update({python_siteName: python_path})
            if not python_path:
                raise SCons.Errors.StopError(
                    "%s (%r) detection failed" % (tool_name, env["TARGET_ARCH"])
                )

        # Now, prepend it to the path
        path, python = os.path.split(python_path)
        env.PrependENVPath("PATH", path)
        env["PYTHON"] = python

        # FIXME Treat warnings as errors?
        # env["ENV"]["PYTHONWARNINGS"] = "error"

    def exists(env):
        # We rely on generate to tell us if a tool is available
        return True

    return generate, exists
