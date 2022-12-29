"""Configures the general environment used for this SCons build: separate environments are then
cloned off this root.
"""

import os
import platform

from SCons.Defaults import DefaultEnvironment
from SCons.Subst import SetAllowableExceptions
from SCons.Variables import Variables
from tools_config import ToolsConfig


# Logs for SCons- and script-related warnings that can usually be ignored.
SconscriptLog = open("Build/.sconscript.log", "w")
_mscommonLog_path = "Build/.mscommon.log"
with open(_mscommonLog_path, "w"):
    pass  # clears the file
os.environ["SCONS_MSCOMMON_DEBUG"] = os.path.abspath(_mscommonLog_path)


def MakeRootEnv():
    # Command-line build variables.
    vars = Variables(None)

    # Generic build environment applicable to all compilers/targets and used internally by SCons.
    RootEnv = DefaultEnvironment(tools=[], variables=vars)

    # Require that only known command-line variables are used.
    vars_unknown = vars.UnknownVariables()
    if vars_unknown:
        RootEnv.Exit("Unknown variables: %r" % vars_unknown.keys())

    # Require that construction variable names exist at expansion, then add those allowed to be
    # empty.
    SetAllowableExceptions()
    RootEnv.Replace(
        __RPATH=[],
        _FRAMEWORKPATH="",
        _RPATH=[],
        CCCOMSTR="",
        COVCOMSTR="",
        COVTESTACTIONSTR="",
        CPPFLAGS=[],
        LIBNOVERSIONSYMLINKS="",
        LIBPATH=[],
        LIBVERSION="",
        LINKCOMSTR="",
        PCH="",
        PPCCCOMSTR="",
        RPATH=[],
        SHCCCOMSTR="",
        SHLIBNOVERSIONSYMLINKS="",
        SHLIBVERSION="",
        SHLINKCOMSTR="",
        WINDOWS_INSERT_DEF=0,
        WINDOWSDEFPREFIX="",
        WINDOWSDEFSUFFIX="",
    )

    # Put .sconsign.dblite (et al) in Build rather than the top nohtyP directory.
    RootEnv.SConsignFile(os.path.abspath("Build/.sconsign"))

    # For dependencies, first consider timestamps, then MD5 checksums.
    RootEnv.Decider("MD5-timestamp")

    # Always use cmd.exe on Windows, regardless of user's shell; ignored on other platforms.
    RootEnv["ENV"]["COMSPEC"] = "cmd.exe"

    # TODO These Scons environment variables should be cross-platform, not Windows-only.
    if not RootEnv["HOST_OS"]:
        RootEnv["HOST_OS"] = RootEnv["PLATFORM"]
    if not RootEnv["HOST_ARCH"]:
        RootEnv["HOST_ARCH"] = platform.machine()

    # Store compiler autodetection results in a file that the developer can modify.
    # TODO OS-specific tools config, so same repo can build from Windows and Linux?
    RootEnv["TOOLS_CONFIG"] = ToolsConfig("Build/site_toolsconfig.py")

    return RootEnv
