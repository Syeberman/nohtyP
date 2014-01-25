# XXX NOT a SCons tool module; instead, a library for the msvs_* tool modules

import os, os.path
import SCons.Errors
import SCons.Tool
import SCons.Tool.MSCommon.vs

# Without a toolpath argument this will find SCons' tool module
_msvsTool = SCons.Tool.Tool( "msvs" )
_msvcTool = SCons.Tool.Tool( "msvc" )
_mslinkTool = SCons.Tool.Tool( "mslink" )


def ApplyMSVSOptions( env, version ):
    """Updates env with MSVS-specific compiler options for nohtyP.  version is numeric (ie 12.0).
    """
    # FIXME look for more "ignoring unknown option" in build output
    def addCcFlags( *args ): env.AppendUnique( CCFLAGS=list( args ) )
    addCcFlags(
            # Warning level 3, warnings-as-errors
            "/W3", "/WX", 
            # Security compile- and runtime-checks (for all builds); /sdl implies /GS
            "/sdl" if version >= 11.0 else "/GS",
            # Function-level linking, disable minimal rebuild, disable runtime type info
            "/Gy", "/Gm-", "/GR-",
            # Source/assembly listing, exception handling model
            # TODO .asm not removed on clean
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
                # Whole program optimization, full speed optimizations, 
                "/GL", "/Ox",
                # Multithreaded and DLL MSVCRT
                "/MD",
                )
    # Disable frame-pointer omission (ie frame pointers will be available on all builds)
    # XXX Must come after any other optimization compiler options
    addCcFlags( "/Oy-" )
    # Improved debugging of optimized code: http://wp.me/p1fTCO-my
    if version >= 12.0: addCcFlags( "/d2Zi+" )

    def addCppDefines( *args ): env.AppendUnique( CPPDEFINES=list( args ) )
    if env["CONFIGURATION"] == "debug":
        addCppDefines( "_DEBUG" )
    else:
        addCppDefines( "NDEBUG" )
    # TODO "WIN32" "_WINDOWS" "_USRDLL" "_WINDLL" "_UNICODE" "UNICODE"?

    # TODO nohtyP.obj : MSIL .netmodule or module compiled with /GL found; restarting link
    # with /LTCG; add /LTCG to the link command line to improve linker performance


"""
VS 2013 - link debug
/OUT:"C:\OpenSource\nohtyP\Build\VS12.0\Debug\nohtyP.dll"   /MANIFEST /PROFILE       /NXCOMPAT /PDB:"C:\OpenSource\nohtyP\Build\VS12.0\Debug\nohtyP.pdb"   /DYNAMICBASE /MAPINFO:EXPORTS "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib" /LARGEADDRESSAWARE /IMPLIB:"C:\OpenSource\nohtyP\Build\VS12.0\Debug\nohtyP.lib"   /VERSION:"VersionHere" /DEBUG /DLL /MACHINE:X86 /WX                   /INCREMENTAL:NO /PGD:"C:\OpenSource\nohtyP\Build\VS12.0\Debug\nohtyP.pgd"   /SUBSYSTEM:WINDOWS /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /ManifestFile:"Debug\nohtyP.dll.intermediate.manifest"   /MAP":C:\OpenSource\nohtyP\Build\VS12.0\Debug\nohtyP.map"            /ERRORREPORT:PROMPT /NOLOGO /ASSEMBLYDEBUG /TLBID:1
VS 2013 - link release
/OUT:"C:\OpenSource\nohtyP\Build\VS12.0\Release\nohtyP.dll" /MANIFEST          /LTCG /NXCOMPAT /PDB:"C:\OpenSource\nohtyP\Build\VS12.0\Release\nohtyP.pdb" /DYNAMICBASE /MAPINFO:EXPORTS "kernel32.lib" "user32.lib" "gdi32.lib" "winspool.lib" "comdlg32.lib" "advapi32.lib" "shell32.lib" "ole32.lib" "oleaut32.lib" "uuid.lib" "odbc32.lib" "odbccp32.lib" /LARGEADDRESSAWARE /IMPLIB:"C:\OpenSource\nohtyP\Build\VS12.0\Release\nohtyP.lib" /VERSION:"VersionHere" /DEBUG /DLL /MACHINE:X86 /WX /OPT:REF /SAFESEH /INCREMENTAL:NO /PGD:"C:\OpenSource\nohtyP\Build\VS12.0\Release\nohtyP.pgd" /SUBSYSTEM:WINDOWS /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /ManifestFile:"Release\nohtyP.dll.intermediate.manifest" /MAP":C:\OpenSource\nohtyP\Build\VS12.0\Release\nohtyP.map" /OPT:ICF /ERRORREPORT:PROMPT /NOLOGO                /TLBID:1

TODO Ensure /Zi or /Z7 gets added when PDB used, and that /Fd"Debug\vc120.pdb" isn't needed
TODO /analyze? (enable /Wall when analyzing, then decide what to supress)

SCons - cl
cl /FoBuild\msvs_120\win32_amd64_debug\nohtyP.obj   /c nohtyP.c /nologo /Dyp_ENABLE_SHARED /Dyp_BUILD_CORE /Dyp_DEBUG_LEVEL=1 /I.
cl /FoBuild\msvs_120\win32_amd64_release\nohtyP.obj /c nohtyP.c /nologo /Dyp_ENABLE_SHARED /Dyp_BUILD_CORE /Dyp_DEBUG_LEVEL=0 /I.
cl /FoBuild\msvs_120\win32_x86_debug\nohtyP.obj     /c nohtyP.c /nologo /Dyp_ENABLE_SHARED /Dyp_BUILD_CORE /Dyp_DEBUG_LEVEL=1 /I.
cl /FoBuild\msvs_120\win32_x86_release\nohtyP.obj   /c nohtyP.c /nologo /Dyp_ENABLE_SHARED /Dyp_BUILD_CORE /Dyp_DEBUG_LEVEL=0 /I.

SCons - link
link /nologo /dll /out:Build\msvs_120\win32_amd64_debug\nohtyP.dll   /implib:Build\msvs_120\win32_amd64_debug\nohtyP.lib   Build\msvs_120\win32_amd64_debug\nohtyP.obj
link /nologo /dll /out:Build\msvs_120\win32_amd64_release\nohtyP.dll /implib:Build\msvs_120\win32_amd64_release\nohtyP.lib Build\msvs_120\win32_amd64_release\nohtyP.obj
link /nologo /dll /out:Build\msvs_120\win32_x86_debug\nohtyP.dll     /implib:Build\msvs_120\win32_x86_debug\nohtyP.lib     Build\msvs_120\win32_x86_debug\nohtyP.obj
link /nologo /dll /out:Build\msvs_120\win32_x86_release\nohtyP.dll   /implib:Build\msvs_120\win32_x86_release\nohtyP.lib   Build\msvs_120\win32_x86_release\nohtyP.obj

TODO http://randomascii.wordpress.com/2012/07/22/more-adventures-in-failing-to-crash-properly/
"""


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




