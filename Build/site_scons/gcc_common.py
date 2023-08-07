# XXX NOT a SCons tool module; instead, a library for the gcc_* tool modules

import os
import os.path
import functools
import subprocess
import re
import tempfile
import SCons.Errors
import SCons.Platform
import SCons.Tool
import SCons.Warnings
import cc_preprocessed
import gcc_coverage
from root_environment import SconscriptLog
from tool_finder import ToolFinder

_arch2opts = {
    "x86": ("-m32",),
    "amd64": ("-m64",),
}
_test_gcc_temp_dir = None

re_gcc_stem = re.compile(r"gcc(-[0-9.]+)?")


def _version_detector(gcc):
    """Returns (version, archs), where archs is a tuple of supported architectures."""
    SconscriptLog.write(f"Detecting version of {gcc}\n")

    # Our exe_globs picks up related tools
    if not re_gcc_stem.fullmatch(gcc.stem):
        return "", ()

    # Determine if the version's right, returning if it isn't
    try:
        # They changed -dumpverion: https://stackoverflow.com/a/47410010
        version = (
            subprocess.check_output(
                [str(gcc), "-dumpfullversion", "-dumpversion"], stderr=subprocess.PIPE
            )
            .decode()
            .strip()
        )
    except subprocess.CalledProcessError:
        return "", ()

    # A small C file we can use to test if command arguments are supported
    global _test_gcc_temp_dir
    if _test_gcc_temp_dir is None:
        _test_gcc_temp_dir = tempfile.mkdtemp(prefix="nohtyP-gcc-")
        with open(os.path.join(_test_gcc_temp_dir, "test.c"), "w") as outfile:
            outfile.write("int main(){}\n")

    # Test to see if gcc supports the target
    supportedArchs = []
    env = dict(os.environ)
    env["PATH"] = str(gcc.parent) + os.pathsep + env.get("PATH", "")
    for arch, archOpts in _arch2opts.items():
        gcc_args = [str(gcc), "test.c"] + list(archOpts)
        SconscriptLog.write("Testing for architecture support: %r\n" % (gcc_args,))
        SconscriptLog.flush()
        gcc_result = subprocess.call(
            gcc_args, cwd=_test_gcc_temp_dir, stdout=SconscriptLog, stderr=SconscriptLog, env=env
        )
        SconscriptLog.flush()
        SconscriptLog.write("gcc %r returned %r\n" % (version, gcc_result))
        if gcc_result == 0:  # gcc returns zero on success
            supportedArchs.append(arch)

    return version, tuple(supportedArchs)


gcc_finder = ToolFinder(
    # Try some known, and unknown-but-logical, default locations for mingw
    # WinLibs https://winlibs.com/
    #   C:\MinGW\winlibs-x86_64-posix-seh-gcc-12.2.0-llvm-14.0.6-mingw-w64ucrt-10.0.0-r2\mingw64
    # MinGW-w64 (http://sourceforge.net/projects/mingw-w64/)
    #   C:\Program Files\mingw-w64\x86_64-4.8.4-posix-seh-rt_v3-rev0\mingw64
    # Official MinGW (http://www.mingw.org/)
    #   C:\MinGW, C:\MinGW0.5, C:\MinGW_0.5, C:\MinGW\0.5
    # MinGW-builds (http://sourceforge.net/projects/mingwbuilds/)
    #   C:\Program Files\mingw-builds\x32-4.8.1-win32-seh-rev5\mingw32
    #   C:\Program Files\mingw-builds\x64-4.8.1-win32-seh-rev5\mingw64
    # Win-builds (http://win-builds.org/)
    #   C:\win-builds-32, C:\win-builds-64, C:\win-builds-64-1.3, C:\win-builds-64\1.3
    win_dirs=(
        "mingw*\\bin",
        "mingw*\\*\\bin",
        "mingw*\\*\\*\\bin",
        "win-builds*\\bin",
        "win-builds*\\*\\bin",
    ),
    posix_dirs=(),  # rely on the environment's path for now
    darwin_dirs=(),  # rely on the environment's path for now
    exe_globs=("gcc-*.*", "gcc-*", "gcc"),  # prefer specificity
    version_detector=_version_detector,
)

# Without a toolpath argument these will find SCons' tool modules
_platform_name = SCons.Platform.Platform().name
if _platform_name in ("win32", "cygwin"):
    _tools = [
        SCons.Tool.Tool("mingw"),
    ]
else:
    _tools = [SCons.Tool.Tool(x) for x in ("gcc", "g++", "gnulink")]


# TODO This, or something like it, should be in SCons
def _find(env, re_version):
    """Find a gcc executable that can build our target OS and arch, returning the path or None."""
    if env["TARGET_OS"] != env["HOST_OS"]:
        raise SCons.Errors.StopError("not yet supporting cross-OS compile in GCC")

    for gcc, (version, archs) in gcc_finder:
        if re_version.fullmatch(version) and env["TARGET_ARCH"] in archs:
            return str(gcc)  # TODO would be nice to use Path everywhere
    return None


# It's a lot of work to add target files to a compilation!
# TODO Just add native .asm/.s, .map, etc support to SCons
def _ccEmitter(target, source, env, parent_emitter):
    # Emitters appear to be inconsistent in whether they modify target/source, or return new objs
    target, source = parent_emitter(target, source, env)
    s_base = os.path.splitext(source[0].path)[0]
    for ext in (".i", ".s", ".gcno", ".gcda", ".gcov"):
        env.Clean(target[0], s_base + ext)
    return target, source


def _updateCcEmitters(env):
    builders = (env["BUILDERS"]["StaticObject"], env["BUILDERS"]["SharedObject"])
    # TODO Instead, translate the emitter into a ListEmitter
    for builder in builders:
        for source_suffix, parent_emitter in builder.emitter.items():
            builder.emitter[source_suffix] = functools.partial(
                _ccEmitter, parent_emitter=parent_emitter
            )


def _linkEmitter(target, source, env):
    t_base, t_ext = os.path.splitext(target[0].path)
    assert t_ext in (".dll", ".dylib", ".exe", ".so", ".", ""), target[0].path
    for ext in (".map",):
        env.Clean(target[0], t_base + ext)
    return target, source


def _updateLinkEmitters(env, version):
    env.Append(
        PROGEMITTER=[
            _linkEmitter,
        ],
        SHLIBEMITTER=[
            _linkEmitter,
        ],
        LDMODULEEMITTER=[
            _linkEmitter,
        ],
    )


def ApplyGCCOptions(env, version):
    """Updates env with GCC-specific compiler options for nohtyP. version is numeric (ie 4.8)."""
    try:
        archOpts = _arch2opts[env["TARGET_ARCH"]]
    except KeyError:
        SCons.Errors.StopError("not yet supporting %r with gcc" % env["TARGET_ARCH"])

    def addCcFlags(*args):
        env.AppendUnique(CCFLAGS=list(args))

    # TODO analyze? (enable -Wextra, disable -Werror, supress individual warnings)
    addCcFlags(*archOpts)
    addCcFlags(
        # Warnings-as-errors, all (avoidable) warnings
        "-Werror",
        "-Wall",
        "-Wsign-compare",
        "-Wconversion",
        "-Wundef",
        "-Wstrict-prototypes",
        "-Wmissing-prototypes",
        "-Wmissing-declarations",
        "-Wold-style-declaration",
        "-Wold-style-definition",
        "-Wmissing-parameter-type",
        # Before 4.8, shadow warned if a declaration shadowed a function (index, div)
        "-Wshadow" if version >= 4.8 else "",
        # Disable some warnings
        "-Wno-unused-function",  # TODO Mark MethodError_lenfunc/etc as unused (portably)?
        "-Wno-pointer-sign",
        "-Wno-unknown-pragmas",
        # float-conversion warns about passing a double to finite/isnan, unfortunately
        "-Wno-float-conversion",
        # TODO maybe-uninitialized would be good during analyze
        "-Wno-maybe-uninitialized" if version >= 4.8 else "-Wno-uninitialized",
        # For shared libraries, only expose functions explicitly marked ypAPI
        "-fvisibility=hidden" if version >= 4.0 else "",
        # Debugging information
        "-g3",
        # TODO Is there an /sdl or /GS equivalent for gcc?
        # Save intermediate files (.i and .s)
        "-save-temps=obj",
        "-fverbose-asm",
        # Source/assembly listing (.s) TODO preprocessed?
        # "-Wa,-alns=${TARGET.base}.s",
    )
    if env["CONFIGURATION"] in ("debug", "coverage"):
        addCcFlags(
            # Disable (non-debuggable) optimizations
            # gcc 7.0 (and previous) on Linux, Windows, and Mac all crash with -Og:
            #   test_function.c: In function 'test_t_function_call':
            #   test_function.c:2892:1: internal error: in get_insn_template, at final.c:2086
            #   libbacktrace could not find executable to open
            "-Og" if version >= 8.0 else "-O0",
            # Runtime check: int overflow
            "-ftrapv",
            # Runtime check: stack overflow
            # XXX Not supported on Windows: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90458#c4
            # TODO Stack overflow detection should possibly always be enabled.
            "-fstack-clash-protection"
            if version >= 8 and _platform_name != "win32"
            else "-fstack-check",
            # Runtime check: buffer overflow (needs -fmudflap to linker)
            # TODO Not supported on MinGW/Windows, apparently
            # "-fmudflapth",
        )
    else:
        addCcFlags(
            # Optimize for speed
            # gcc 7.0 (and previous) on Linux, Windows, and Mac all crash with -O3:
            #   test_function.c: In function 'test_t_function_call':
            #   test_function.c:2892:1: internal error: in get_insn_template, at final.c:2086
            #   libbacktrace could not find executable to open
            "-O3" if version >= 8.0 else "-O0",
            # Optimize whole program (needs -flto to linker): conflicts with -g
            # https://gcc.gnu.org/onlinedocs/gcc-4.9.0/gcc/Optimize-Options.html
            # "-flto",
        )
    if env["CONFIGURATION"] == "coverage":
        addCcFlags(
            "--coverage",
            "-fprofile-abs-path",
        )

    # Disable frame-pointer omission (ie frame pointers will be available on all builds)
    # XXX Must come after any other optimization compiler options
    if _platform_name == "win32" and version == 8 and env["TARGET_ARCH"] == "amd64":
        # MinGW-W64 for gcc 8.1 crashes with -fno-omit-frame-pointer:
        #   nohtyP.c: In function 'MethodError_objsliceobjproc':
        #   nohtyP.c:572:1: internal compiler error: in based_loc_descr, at dwarf2out.c:14264
        #    DEFINE_GENERIC_METHODS(MethodError, yp_MethodError);
        pass
    else:
        addCcFlags("-fno-omit-frame-pointer")
    # Ensure SCons knows to clean .s, etc
    _updateCcEmitters(env)

    def addCppDefines(*args):
        env.AppendUnique(CPPDEFINES=list(args))

    if env["CONFIGURATION"] in ("debug", "coverage"):
        addCppDefines("_DEBUG")
    else:
        addCppDefines("NDEBUG")

    # PPCC is the preprocessor-only mode for CC, the C compiler (compare with SHCC et al)
    # TODO -save-temps above also writes the .i file
    # TODO Create PPCC, PPCFLAGS, PPCCFLAGS just like SHCC/etc, and contribute back to SCons?
    # TODO For SCons: be smart and when passed a preprocessed file, compiler skips certain options?
    env["PPCCCOM"] = "$CC -E -o $TARGET -c $CFLAGS $CCFLAGS $_CCCOMCOM $SOURCES"

    def addLinkFlags(*args):
        env.AppendUnique(LINKFLAGS=list(args))

    addLinkFlags(*archOpts)
    addLinkFlags(
        # Warnings-as-errors, all (avoidable) warnings
        "-Werror",
        "-Wall",
        # Building a shared library with GCC on Windows requires the GCC shared libraries, which are
        # not available by default. So link these libraries statically.
        "-static" if env["TARGET_OS"] == "win32" else "",
        # Create a mapfile (.map)
        "-Wl,-map,${TARGET.base}.map"
        if _platform_name == "darwin"
        else "-Wl,-Map,${TARGET.base}.map",
        # TODO Version stamp?
    )
    if env["TARGET_ARCH"] == "x86":
        # Large address aware (>2GB)
        addLinkFlags("-Wl,--large-address-aware")
    if env["CONFIGURATION"] in ("debug", "coverage"):
        addLinkFlags(
            # Disable (non-debuggable) optimizations
            "-Og" if version >= 4.8 else "-O0",
            # Runtime check: buffer overflow (needs -fmudflap* to compiler)
            # TODO Not supported on MinGW/Windows, apparently
            # "-fmudflap",
            "--coverage" if env["CONFIGURATION"] == "coverage" else "",
        )
    else:
        addLinkFlags(
            # Optimize for speed
            "-O3",
            # Optimize whole program (needs -flto to compiler): see above
            # "-flto",
        )
    # Ensure SCons knows to clean .map, etc
    _updateLinkEmitters(env, version)


def DefineGCCToolFunctions(numericVersion, major, minor=None):
    """Returns (generate, exists), suitable for use as the SCons tool module functions."""

    # Major numbers started incrementing faster with GCC 5, and patch numbers were dropped. It was
    # 10 years between 4.0.0 and 5.1, and 4.x saw 54 releases in that time. In contrast, only one
    # year separates 5.1 and 6.1, and 5.x saw just 5 releases.
    if major >= 5:
        if minor is not None:
            raise ValueError("minor is invalid for major>=5")
        gcc_name = f"gcc {major}"
        re_version = re.compile(rf"{major}(\.\d+)*")
    else:
        if minor is None:
            raise ValueError("minor is required for major<5")
        gcc_name = f"gcc {major}.{minor}"
        re_version = re.compile(rf"{major}\.{minor}(\.\d+)*")

    def generate(env):
        if env["CONFIGURATION"] not in ("release", "debug", "coverage"):
            raise SCons.Errors.StopError(
                "GCC doesn't support the %r configuration (yet)" % env["CONFIGURATION"]
            )

        gcc_name_arch = f"{gcc_name} ({env['TARGET_ARCH']})"

        # See if site_toolsconfig.py already knows where to find this gcc version
        toolsConfig = env["TOOLS_CONFIG"]
        gcc_siteName = "%s_%s" % (env["COMPILER"].name.upper(), env["TARGET_ARCH"].upper())
        gcc_path = toolsConfig.get(gcc_siteName, "")
        if gcc_path is None:
            raise SCons.Errors.StopError(f"{gcc_name_arch} disabled in {toolsConfig.basename}")

        # If site_toolsconfig.py came up empty, find a gcc that supports our target, then update
        if not gcc_path:
            gcc_path = _find(env, re_version)
            toolsConfig.update({gcc_siteName: gcc_path})
            if not gcc_path:
                raise SCons.Errors.StopError(f"{gcc_name_arch} detection failed")

        # TODO Update SCons to skip autodetection when requested
        for tool in _tools:
            tool.generate(env)
        cc_preprocessed.generate_PreprocessedBuilder(env)
        gcc_coverage.generate_CoverageBuilder(env)

        # The tool (ie mingw) may helpfully prepend to path and other variables...so make sure we
        # prepend *our* path after the tool does its thing
        env.PrependENVPath("PATH", os.path.dirname(gcc_path))
        env["CC"] = os.path.basename(gcc_path)
        del env["CXX"]  # TODO provide proper path when we need it
        if not env.WhereIs("$CC"):
            raise SCons.Errors.StopError(f"{gcc_name_arch} configuration failed")

        def check_version(env, output):
            output = output.strip()
            if re_version.fullmatch(output) is None:
                raise SCons.Errors.StopError(f"tried finding {gcc_name}, found {output} instead")

        env.ParseConfig("$CC -dumpfullversion -dumpversion", check_version)

        ApplyGCCOptions(env, numericVersion)

    def exists(env):
        # We rely on generate to tell us if a tool is available
        return True

    return generate, exists
