# XXX NOT a SCons tool module; instead, a library for the msvs_* tool modules

import os, os.path, functools
import SCons.Errors
import SCons.Tool
import SCons.Tool.MSCommon.vs
import SCons.Warnings


# Disables "MSVC_USE_SCRIPT set to False" warnings.  Unfortunately, also disables "No version of 
# Visual Studio compiler found" et al, but these are actually errors and they're trapped
# separately.
SCons.Warnings.suppressWarningClass( SCons.Warnings.VisualCMissingWarning )

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
    # TODO Remove these asserts once we've gotten this right
    assert len( source ) == 1
    assert os.path.splitext( target[0].path )[1] == ".obj"
    s_base = os.path.splitext( source[0].path )[0]
    for ext in (".asm", ): env.Clean( target[0], s_base+ext )
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


# TODO Any new options in VS 14.0 (aka 2015) that we can take advantage of?
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
    if env["CONFIGURATION"] == "debug":
        addCppDefines( "_DEBUG" )
    else:
        addCppDefines( "NDEBUG" )

    # CCPP is the preprocessor-only mode for CC, the C compiler (compare with SHCC et al)
    # TODO See _MSVC_OUTPUT_FLAG: the string below may not work in batch mode
    # TODO Create PPCC, PPCFLAGS, PPCCFLAGS just like SHCC/etc, and contribute back to SCons?
    # TODO For SCons: be smart and when passed a preprocessed file, compiler skips certain options?
    env['PPCCCOM'] = '${TEMPFILE("$CC /P /Fi$TARGET /c $CHANGED_SOURCES $CFLAGS $CCFLAGS $_CCCOMCOM","$CCCOMSTR")}'

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
            # Version stamp
            "/VERSION:${NOHTYP_MAJOR}.${NOHTYP_MINOR}"
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
        if version is None: raise SCons.Errors.UserError( "Visual Studio %r is not installed" % supportedVersions[0] )
        if env["TARGET_OS"] != "win32": raise SCons.Errors.StopError( "Visual Studio doesn't build for OS %r" % env["TARGET_OS"] )
        # TARGET_ARCH is verified by vcvars*.bat
        if env["CONFIGURATION"] not in ("release", "debug"): raise SCons.Errors.StopError( "Visual Studio doesn't support the %r configuration (yet)" % env["CONFIGURATION"] )
        env["MSVC_VERSION"] = version

        # Caching INCLUDE, LIB, etc in site_toolsconfig.py bypasses the slow vcvars*.bat calls on
        # subsequent builds
        toolsConfig = env["TOOLS_CONFIG"]
        var_names = ("INCLUDE", "LIB", "LIBPATH", "PATH")
        compilerName = env["COMPILER"].name
        compilerEnv_name = "%s_%s_ENV" % (compilerName.upper( ), env["TARGET_ARCH"].upper( ))
        compilerEnv = toolsConfig.get( compilerEnv_name, {} )
        if compilerEnv is None:
            raise SCons.Errors.StopError( "Visual Studio %r (%r) disabled in %s" % (supportedVersions[0], env["TARGET_ARCH"], toolsConfig.basename) )

        # If there was an entry in site_toolsconfig.py, then explicitly use that environment
        if compilerEnv:
            env["MSVC_USE_SCRIPT"] = None # disable autodetection, vcvars*.bat, etc
            for var_name in var_names:
                env.PrependENVPath( var_name, compilerEnv[var_name], delete_existing=False )

        # Configures the compiler, including possibly autodetecting the required environment
        _msvsTool.generate( env )
        _msvcTool.generate( env )
        _mslinkTool.generate( env )
        if not env.WhereIs( "$CC" ):
            raise SCons.Errors.StopError( "Visual Studio %r (%r) configuration failed" % (supportedVersions[0], env["TARGET_ARCH"]) )

        # Add an entry for this compiler in site_toolsconfig.py if it doesn't already exist
        if not compilerEnv:
            compilerEnv = {}
            for var_name in var_names: 
                compilerEnv[var_name] = env["ENV"][var_name]
            toolsConfig.update( {compilerEnv_name: compilerEnv} )

        ApplyMSVSOptions( env, numericVersion )

    def exists( env ):
        # We rely on generate to tell us if a tool is available
        return True

    return generate, exists

