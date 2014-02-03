# XXX NOT a SCons tool module; instead, a library for the gcc_* tool modules

import os, os.path, sys, functools, subprocess, re
import SCons.Errors
import SCons.Platform
import SCons.Tool
import SCons.Warnings

# Without a toolpath argument these will find SCons' tool modules
_platform_name = SCons.Platform.Platform( ).name
if _platform_name in ("win32", "cygwin"):
    _tools = [SCons.Tool.Tool( "mingw" ), ]
    _supportedArchs = ("x86", ) # TODO enable 64-bit on Windows
else:
    _tools = [SCons.Tool.Tool( x ) for x in ("gcc", "g++", "gnulink")]
    _supportedArchs = ("x86", "amd64")


# It's a lot of work to add target files to a compilation!
# TODO Just add native .asm/.s, .map, etc support to SCons
def _ccEmitter( target, source, env, parent_emitter ):
    # Emitters appear to be inconsistent in whether they modify target/source, or return new objs
    target, source = parent_emitter( target, source, env )
    for s in source:
        s_base = os.path.splitext( s.path )[0]
        # FIXME When we add these to target, they become inputs into the linker phase, and gcc then
        # chokes and dies.  So...how are we supposed to get these files cleaned?
        for ext in (".i", ".s"): target.append( s_base + ext )
    return target, source
def _updateCcEmitters( env ):
    builders = (env['BUILDERS']['StaticObject'], env['BUILDERS']['SharedObject'])
    for builder in builders:
        for source_suffix, parent_emitter in builder.emitter.items( ):
            builder.emitter[source_suffix] = functools.partial( 
                    _ccEmitter, parent_emitter=parent_emitter ) 

def _linkEmitter( target, source, env ):
    t = str( target[0] )
    assert t.endswith( ".dll" ) or t.endswith( ".exe" )
    #target.append( os.path.splitext( t )[0] + ".map" )
    return target, source
def _updateLinkEmitters( env, version ):
    env.Append( PROGEMITTER=[_linkEmitter, ], SHLIBEMITTER=[_linkEmitter, ], 
            LDMODULEEMITTER=[_linkEmitter, ] )


def _archOptions( env ):
    arch2opts = {
            "x86": ("-m32", ),
            "amd64": ("-m64", ),
            }
    opts = arch2opts.get( env["TARGET_ARCH"], None )
    if opts is None: raise SCons.Errors.StopError( "not yet supporting %r with gcc" % env["TARGET_ARCH"] )
    return opts

def ApplyGCCOptions( env, version ):
    """Updates env with GCC-specific compiler options for nohtyP.  version is numeric (ie 4.8).
    """
    archOpts = _archOptions( env )

    def addCcFlags( *args ): env.AppendUnique( CCFLAGS=list( args ) )
    # TODO analyze? (enable -Wextra, disable -Werror, supress individual warnings)
    addCcFlags( *archOpts )
    addCcFlags(
            # Warnings-as-errors, all (avoidable) warnings, 
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
                "-Og",
                # Runtime checks: int overflow, stack overflow, 
                "-ftrapv", "-fstack-check", 
                # Runtime check: buffer overflow (needs -fmudflap to linker)
                # TODO Not supported on MinGW/Windows, apparently
                #"-fmudflapth",
                )
    else:
        addCcFlags( 
                # Optimize: for speed, whole program
                "-O3", "-flto",
                )
    # Disable frame-pointer omission (ie frame pointers will be available on all builds)
    # XXX Must come after any other optimization compiler options
    addCcFlags( "-fno-omit-frame-pointer" )
    # Ensure SCons knows to clean .s, etc
    # FIXME _updateCcEmitters( env )

    def addCppDefines( *args ): env.AppendUnique( CPPDEFINES=list( args ) )
    if env["CONFIGURATION"] == "debug":
        addCppDefines( "_DEBUG" )
    else:
        addCppDefines( "NDEBUG" )

    return # FIXME

    def addLinkFlags( *args ): env.AppendUnique( LINKFLAGS=list( args ) )
    addLinkFlags( *archOpts )
    addLinkFlags(
            # Warnings-as-errors
            "/WX",
            # Large address aware (>2GB)
            "/LARGEADDRESSAWARE",
            # Disable incremental linking
            "/INCREMENTAL:NO",
            # Create a mapfile (.map), include exported functions
            "/MAP", "/MAPINFO:EXPORTS",
            # Version stamp
            "/VERSION:${NOHTYP_MAJOR}.${NOHTYP_MINOR}"
            )
    if env["CONFIGURATION"] == "debug":
        addLinkFlags(
                # Required by -fmudflap above
                #"-fmudflap",
                # Disable optimizations
                "/OPT:NOREF", "/OPT:NOICF",
                # Add DebuggableAttribute (because I don't know?)
                "/ASSEMBLYDEBUG",
                )
    else:
        addLinkFlags(
                # Required by -flto above
                "-flto",
                # Eliminate unreferenced funcs/data, fold identical COMDATs
                "/OPT:REF", "/OPT:ICF",
                # Link-time code generation; required by /GL cc flag
                "/LTCG", 
                )
    # If the CRT for this MSVC version needs a manifest, make sure it's embedded
    if _crtRequiresManifest( version ): env["WINDOWS_EMBED_MANIFEST"] = True
    # Ensure SCons knows to clean .map, etc
    _updateLinkEmitters( env, version )

def DefineGCCToolFunctions( numericVersion, major, minor ):
    """Returns (generate, exists), suitable for use as the SCons tool module functions."""

    re_dumpversion = re.compile( r"^%s\.%s(\.\d+)?$" % (major, minor) )

    def generate( env ):
        if env["TARGET_OS"] != env["HOST_OS"]: raise SCons.Errors.StopError( "not yet supporting cross-OS compile in GCC" )
        if env["TARGET_ARCH"] not in _supportedArchs: raise SCons.Errors.StopError( "not yet supporting %r in GCC" % env["TARGET_ARCH"] )
        if env["CONFIGURATION"] not in ("release", "debug"): raise SCons.Errors.StopError( "GCC doesn't support the %r configuration (yet)" % env["CONFIGURATION"] )

        # TODO Read info from site-tools.py to make this compiler easier to find?

        for tool in _tools: tool.generate( env )
        cc_path = env.WhereIs( "$CC" )
        if not env.WhereIs( "$CC" ):
            raise SCons.Errors.StopError( "gcc %s.%s (%r) configuration failed" % (major, minor, env["TARGET_ARCH"]) )
        def check_version( env, output ):
            output = output.strip( )
            if re_dumpversion.search( output ) is None: raise SCons.Errors.StopError( "tried finding gcc %s.%s, found %r instead" % (major, minor, output) )
        env.ParseConfig( "$CC -dumpversion", check_version )

        # TODO Add an entry for this compiler in site-tools.py if it doesn't already exist?

        ApplyGCCOptions( env, numericVersion )

    def exists( env ):
        return all( x.exists( env ) for x in _tools )

    return generate, exists

