# XXX NOT a SCons tool module; instead, a library for the gcc_* tool modules

import os, os.path, sys, functools, subprocess, re, glob, tempfile
import SCons.Errors
import SCons.Platform
import SCons.Tool
import SCons.Warnings

# TODO Add at least 3.4.6 (2006), 4.2.4 (2007+), 4.4.7 (2009+), 4.6.4 (2011+)

_devnul = open( os.devnull, "w" )

# Without a toolpath argument these will find SCons' tool modules
_platform_name = SCons.Platform.Platform( ).name
if _platform_name in ("win32", "cygwin"):
    _tools = [SCons.Tool.Tool( "mingw" ), ]

    # Try some known, and unknown-but-logical, default locations for mingw
    _mingw_path_globs = (
            # MinGW-builds (http://sourceforge.net/projects/mingwbuilds/)
            "C:\\Program Files\\mingw-builds\\*\*\\bin",
                                        # C:\Program Files\mingw-builds\x64-4.8.1-win32-seh-rev5\mingw64
            "C:\\Program Files (x86)\\mingw-builds\\*\*\\bin",
            # Win-builds (http://win-builds.org/)
            "C:\\win-builds-*\\bin",    # C:\win-builds-32, C:\win-builds-64, C:\win-builds-64-1.3
            "C:\\win-builds-*\\*\\bin", # C:\win-builds-64\1.3
            # Official MinGW (http://www.mingw.org/)
            "C:\\MinGW*\\bin",          # C:\MinGW, C:\MinGW0.5, C:\MinGW_0.5
            "C:\\MinGW\\*\\bin",        # C:\MinGW\0.5
            )
    _gcc_paths_found = [
            binDir for pattern in _mingw_path_globs for binDir in glob.iglob( pattern ) ]
else:
    _tools = [SCons.Tool.Tool( x ) for x in ("gcc", "g++", "gnulink")]
    _gcc_paths_found = []   # rely on the environment's path for now


def _archOptions( targ_os, targ_arch ):
    arch2opts = {
            "x86": ("-m32", ),
            "amd64": ("-m64", ),
            }
    try: opts = arch2opts[targ_arch]
    except KeyError: SCons.Errors.StopError( "not yet supporting %r with gcc" % targ_arch )
    return opts

_test_gcc_cache = {}
_test_gcc_temp_dir = None
def _test_gcc( gcc, re_dumpversion, targ_os, targ_arch ):
    """Tests if the given gcc matches the version, and supports the target OS and arch.  Caches
    results for speed."""
    # Get this first so we can fail quickly on new architectures
    archOpts = _archOptions( targ_os, targ_arch )

    # See if we've tested this executable before
    try: gcc_cache = _test_gcc_cache[gcc]
    except KeyError: gcc_cache = _test_gcc_cache[gcc] = {}

    # Determine if the version's right, returning if it isn't
    try: dumpversion = gcc_cache["dumpversion"]
    except KeyError:
        try: dumpversion = subprocess.check_output( [gcc, "-dumpversion"], stderr=subprocess.PIPE )
        except: dumpversion = ""
        dumpversion = dumpversion.strip( )
        gcc_cache["dumpversion"] = dumpversion
    if re_dumpversion.search( dumpversion ) is None: return False

    # If we've already tested this compiler for this architecture, return the cached value
    # TODO when we support cross-compiling, check the os value too
    try: return gcc_cache[targ_arch]
    except KeyError: pass

    # A small C file we can use to test if command arguments are supported
    global _test_gcc_temp_dir
    if _test_gcc_temp_dir is None:
        _test_gcc_temp_dir = tempfile.mkdtemp( prefix="nohtyP-gcc-" )
        with open( os.path.join( _test_gcc_temp_dir, "test.c" ), "w" ) as outfile:
            outfile.write( "void main(){}\n" )

    # Test to see if gcc supports the target
    env = dict( os.environ )
    env["PATH"] = os.path.dirname( gcc ) + os.pathsep + env.get( "PATH", "" )
    gcc_result = subprocess.call( [gcc, "test.c"]+list( archOpts ),
            cwd=_test_gcc_temp_dir, stdout=_devnul, stderr=_devnul, env=env )
    gcc_cache[targ_arch] = (gcc_result == 0) # gcc returns non-zero on error
    return gcc_cache[targ_arch]

# TODO This, or something like it, should be in SCons
def _find( env, re_dumpversion ):
    """Find a gcc executable that can build our target OS and arch, returning the path or None."""
    if env["TARGET_OS"] != env["HOST_OS"]: raise SCons.Errors.StopError( "not yet supporting cross-OS compile in GCC" )
    binDirs = list( os.environ.get( "PATH", "" ).split( os.pathsep ) )
    binDirs.extend( _gcc_paths_found )
    for binDir in binDirs:
        gcc = os.path.join( binDir, "gcc" )
        supported = _test_gcc( gcc, re_dumpversion, env["TARGET_OS"], env["TARGET_ARCH"] )
        if supported: return gcc
    return None


# It's a lot of work to add target files to a compilation!
# TODO Just add native .asm/.s, .map, etc support to SCons
def _ccEmitter( target, source, env, parent_emitter ):
    # Emitters appear to be inconsistent in whether they modify target/source, or return new objs
    target, source = parent_emitter( target, source, env )
    # TODO Remove these asserts once we've gotten this right
    assert len( source ) == 1
    assert os.path.splitext( target[0].path )[1] == ".o"
    s_base = os.path.splitext( source[0].path )[0]
    for ext in (".i", ".s"): env.Clean( target[0], s_base+ext )
    return target, source
def _updateCcEmitters( env ):
    builders = (env['BUILDERS']['StaticObject'], env['BUILDERS']['SharedObject'])
    # TODO Instead, translate the emitter into a ListEmitter
    for builder in builders:
        for source_suffix, parent_emitter in builder.emitter.items( ):
            builder.emitter[source_suffix] = functools.partial(
                    _ccEmitter, parent_emitter=parent_emitter )

def _linkEmitter( target, source, env ):
    t_base, t_ext = os.path.splitext( target[0].path )
    # TODO Remove these asserts once we've gotten this right
    assert t_ext in (".dll", ".exe")
    for ext in (".map", ): env.Clean( target[0], t_base+ext )
    return target, source
def _updateLinkEmitters( env, version ):
    env.Append( PROGEMITTER=[_linkEmitter, ], SHLIBEMITTER=[_linkEmitter, ],
            LDMODULEEMITTER=[_linkEmitter, ] )


def ApplyGCCOptions( env, version ):
    """Updates env with GCC-specific compiler options for nohtyP.  version is numeric (ie 4.8).
    """
    archOpts = _archOptions( env["TARGET_OS"], env["TARGET_ARCH"] )

    def addCcFlags( *args ): env.AppendUnique( CCFLAGS=list( args ) )
    # TODO analyze? (enable -Wextra, disable -Werror, supress individual warnings)
    addCcFlags( *archOpts )
    addCcFlags(
            # Warnings-as-errors, all (avoidable) warnings
            "-Werror", "-Wall", "-Wsign-compare", "-Wundef", "-Wstrict-prototypes",
            "-Wmissing-prototypes", "-Wmissing-declarations", "-Wold-style-declaration",
            "-Wold-style-definition", "-Wmissing-parameter-type",
            "-Wshadow",
            # Disable some warnings
            # TODO maybe-uninitialized would be good during analyze
            "-Wno-unused", "-Wno-pointer-sign", "-Wno-maybe-uninitialized",
            # Debugging information
            "-g3",
            # TODO Is there an /sdl or /GS equivalent for gcc?
            # Save intermediate files (.i and .s)
            "-save-temps=obj", "-fverbose-asm",
            # Source/assembly listing (.s) TODO preprocessed?
            #"-Wa,-alns=${TARGET.base}.s",
            )
    if env["CONFIGURATION"] == "debug":
        addCcFlags(
                # Disable (non-debuggable) optimizations
                "-Og" if version > 4.8 else "-O0",
                # Runtime checks: int overflow, stack overflow,
                "-ftrapv", "-fstack-check",
                # Runtime check: buffer overflow (needs -fmudflap to linker)
                # TODO Not supported on MinGW/Windows, apparently
                #"-fmudflapth",
                )
    else:
        addCcFlags(
                # Optimize: for speed, whole program (needs -flto to linker)
                "-O3", "-flto",
                )
    # Disable frame-pointer omission (ie frame pointers will be available on all builds)
    # XXX Must come after any other optimization compiler options
    addCcFlags( "-fno-omit-frame-pointer" )
    # Ensure SCons knows to clean .s, etc
    _updateCcEmitters( env )

    def addCppDefines( *args ): env.AppendUnique( CPPDEFINES=list( args ) )
    if env["CONFIGURATION"] == "debug":
        addCppDefines( "_DEBUG" )
    else:
        addCppDefines( "NDEBUG" )

    def addLinkFlags( *args ): env.AppendUnique( LINKFLAGS=list( args ) )
    addLinkFlags( *archOpts )
    addLinkFlags(
            # Warnings-as-errors, all (avoidable) warnings
            "-Werror", "-Wall",
            # Static libgcc (arithmetic, mostly)
            "-static-libgcc",
            # Create a mapfile (.map)
            "-Wl,-Map,${TARGET.base}.map",
            # TODO Version stamp?
            )
    if env["TARGET_ARCH"] == "x86":
        # Large address aware (>2GB)
        addLinkFlags( "-Wl,--large-address-aware" )
    if env["CONFIGURATION"] == "debug":
        addLinkFlags(
                # Disable (non-debuggable) optimizations
                "-Og" if version > 4.8 else "-O0",
                # Runtime check: buffer overflow (needs -fmudflap* to compiler)
                # TODO Not supported on MinGW/Windows, apparently
                #"-fmudflap",
                )
    else:
        addLinkFlags(
                # Optimize: for speed, whole program (needs -flto to compiler)
                "-O3", "-flto",
                )
    # Ensure SCons knows to clean .map, etc
    _updateLinkEmitters( env, version )

def DefineGCCToolFunctions( numericVersion, major, minor ):
    """Returns (generate, exists), suitable for use as the SCons tool module functions."""

    re_dumpversion = re.compile( r"^%s\.%s(\.\d+)?$" % (major, minor) )

    def generate( env ):
        if env["CONFIGURATION"] not in ("release", "debug"): raise SCons.Errors.StopError( "GCC doesn't support the %r configuration (yet)" % env["CONFIGURATION"] )

        # See if site-tools.py already knows where to find this gcc version
        siteTools_name, siteTools_dict = env["SITE_TOOLS"]( )
        gcc_siteName = "%s_%s" % (env["COMPILER"].name.upper( ), env["TARGET_ARCH"].upper( ))
        gcc_path = siteTools_dict.get( gcc_siteName, None )

        # If site-tools.py came up empty, find a gcc that supports our target, then update 
        # site-tools.py
        if not gcc_path:
            gcc_path = _find( env, re_dumpversion )
            if not gcc_path:
                raise SCons.Errors.StopError( "gcc %s.%s (%r) detection failed" % (major, minor, env["TARGET_ARCH"]) )
            siteTools_dict[gcc_siteName] = gcc_path
            with open( siteTools_name, "a" ) as outfile:
                outfile.write( "%s = %r\n\n" % (gcc_siteName, gcc_path) )

        # The tool (ie mingw) may helpfully try to autodetect and prepend to path...so make sure we
        # prepend *our* path after the tool does its thing
        # TODO Update SCons to skip autodetection when requested
        for tool in _tools: tool.generate( env )
        env.PrependENVPath( "PATH", os.path.dirname( gcc_path ) )
        if not env.WhereIs( "$CC" ):
            raise SCons.Errors.StopError( "gcc %s.%s (%r) configuration failed" % (major, minor, env["TARGET_ARCH"]) )
        def check_version( env, output ):
            output = output.strip( )
            if re_dumpversion.search( output ) is None: raise SCons.Errors.StopError( "tried finding gcc %s.%s, found %r instead" % (major, minor, output) )
        env.ParseConfig( "$CC -dumpversion", check_version )

        # TODO Add an entry for this compiler in site-tools.py if it doesn't already exist?

        ApplyGCCOptions( env, numericVersion )

    def exists( env ):
        return True # FIXME? _find( env, re_dumpversion )

    return generate, exists

