# XXX NOT a SCons tool module; instead, a library for the msvs_* tool modules

import os, os.path, sys, functools
import SCons.Errors
import SCons.Tool
import SCons.Tool.MSCommon.vs


# Without a toolpath argument this will find SCons' tool modules
_msvsTool = SCons.Tool.Tool( "msvs" )
_msvcTool = SCons.Tool.Tool( "msvc" )
_mslinkTool = SCons.Tool.Tool( "mslink" )

# TODO http://randomascii.wordpress.com/2012/07/22/more-adventures-in-failing-to-crash-properly/

def _crtRequiresManifest( version ):
    """Manifest files were required for the CRT starting with MSVS 2005, and ending with 2010."""
    return 8.0 <= version < 10.0

# It's a lot of work to add target files to a compilation!
# TODO Just add native .asm, .map, etc support to SCons
def _ccEmitter( target, source, env, parent_emitter ):
    # Emitters appear to be inconsistent in whether they modify target/source, or return new objs
    target, source = parent_emitter( target, source, env )
    t = str( target[0] )
    assert t.endswith( ".obj" )
    target.append( t[:-4] + ".asm" ) # TODO do better
    return target, source
def _updateCcEmitters( env ):
    builders = (env['BUILDERS']['StaticObject'], env['BUILDERS']['SharedObject'])
    for builder in builders:
        for source_suffix, parent_emitter in builder.emitter.items( ):
            builder.emitter[source_suffix] = functools.partial( 
                    _ccEmitter, parent_emitter=parent_emitter ) 

def _linkEmitter( target, source, env, msvs_version ):
    t = str( target[0] )
    assert t.endswith( ".dll" ) or t.endswith( ".exe" )
    #if _crtRequiresManifest( msvs_version ): target.append( t + ".manifest" )
    target.append( t[:-4] + ".map" ) # TODO do better
    return target, source
def _updateLinkEmitters( env, version ):
    emitter = functools.partial( _linkEmitter, msvs_version=version )
    env.Append( PROGEMITTER=[emitter, ], SHLIBEMITTER=[emitter, ], 
            LDMODULEEMITTER=[emitter, ] )


def ApplyMSVSOptions( env, version ):
    """Updates env with MSVS-specific compiler options for nohtyP.  version is numeric (ie 12.0).
    """
    def addCcFlags( *args ): env.AppendUnique( CCFLAGS=list( args ) )
    # TODO /analyze? (enable /Wall, disable /WX, supress individual warnings)
    addCcFlags(
            # Warning level 3, warnings-as-errors
            "/W3", "/WX", 
            # Security compile- and runtime-checks (for all builds); /sdl implies /GS
            "/sdl" if version >= 11.0 else "/GS",
            # Function-level linking, disable minimal rebuild, disable runtime type info
            "/Gy", "/Gm-", "/GR-",
            # Source/assembly listing (.asm), exception handling model
            "/FAs", "/Fa${TARGET.dir}"+os.sep, "/EHsc",
            )
    if env["CONFIGURATION"] == "debug":
        addCcFlags( 
                # Runtime checks, disable optimizations
                "/RTCcsu", "/Od",
                # Debug multithread and DLL MSVCRT
                "/MDd",
                )
    else:
        addCcFlags( 
                # Optimize: Whole program, full speed; /GL requires /LTCG link flag
                "/GL", "/Ox",
                # Multithreaded and DLL MSVCRT
                "/MD",
                )
    # Disable frame-pointer omission (ie frame pointers will be available on all builds)
    # XXX Must come after any other optimization compiler options
    addCcFlags( "/Oy-" )
    # Improved debugging of optimized code: http://wp.me/p1fTCO-my
    if version >= 12.0: addCcFlags( "/d2Zi+" )
    # Ensure SCons knows to clean .asm, etc
    _updateCcEmitters( env )

    def addCppDefines( *args ): env.AppendUnique( CPPDEFINES=list( args ) )
    # FIXME
    # A fresh "2008 SP1" install builds against the "2008" CRT version, but does not install that
    # older version in winsxs (or, it is removed by the redist package), causing problems.
    #if _crtRequiresManifest( version ): addCppDefines( "_BIND_TO_CURRENT_VCLIBS_VERSION=1" )
    if env["CONFIGURATION"] == "debug":
        addCppDefines( "_DEBUG" )
    else:
        addCppDefines( "NDEBUG" )

    def addLinkFlags( *args ): env.AppendUnique( LINKFLAGS=list( args ) )
    addLinkFlags(
            # Warnings-as-errors
            "/WX",
            # Large address aware (>2GB)
            "/LARGEADDRESSAWARE",
            # Disable incremental linking
            "/INCREMENTAL:NO",
            # Create a mapfile (.map), include exported functions
            "/MAP", "/MAPINFO:EXPORTS",
            )
    if env["CONFIGURATION"] == "debug":
        addLinkFlags(
                # Disable optimizations
                "/OPT:NOREF", "/OPT:NOICF",
                # Add DebuggableAttribute (because I don't know?)
                "/ASSEMBLYDEBUG",
                )
    else:
        addLinkFlags(
                # Eliminate unreferenced funcs/data, fold identical COMDATs
                "/OPT:REF", "/OPT:ICF",
                # Link-time code generation; required by /GL cc flag
                "/LTCG", 
                )
    # If the CRT for this MSVC version needs a manifest, make sure it's embedded
    if _crtRequiresManifest( version ): env["WINDOWS_EMBED_MANIFEST"] = True
    # Ensure SCons knows to clean .map, etc
    _updateLinkEmitters( env, version )

def DefineMSVSToolFunctions( numericVersion, supportedVersions ):
    """Returns (generate, exists), suitable for use as the SCons tool module functions."""

    # Determine the exact version of tool installed, if any (Express vs "commercial")
    # XXX get_vs_by_version is internal to SCons and may change in the future
    for version in supportedVersions:
        try:
            if SCons.Tool.MSCommon.vs.get_vs_by_version( version ): break
        except SCons.Errors.UserError: pass # "not supported by SCons"
    else: version = None

    def generate( env ):
        if version is None:
            raise SCons.Errors.UserError( "Visual Studio %r is not installed" % supportedVersions[0] )
        env["MSVC_VERSION"] = version
        _msvsTool.generate( env )
        _msvcTool.generate( env )
        _mslinkTool.generate( env )
        if not env.WhereIs( "$CC" ):
            raise SCons.Errors.StopError( "Visual Studio %r (%r) configuration failed" % (supportedVersions[0], env["TARGET_ARCH"]) )
        ApplyMSVSOptions( env, numericVersion )

    def exists( env ):
        return version is not None

    return generate, exists

