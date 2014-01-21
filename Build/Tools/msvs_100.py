
import os.path
import SCons.Errors
import SCons.Tool
import SCons.Tool.MSCommon.vs

# Without a toolpath argument this will find SCons' tool module
_msvsTool = SCons.Tool.Tool( "msvs" )
_msvcTool = SCons.Tool.Tool( "msvc" )
_mslinkTool = SCons.Tool.Tool( "mslink" )

# Determine the exact version of tool installed, if any.  For nohtyP's purposes, we don't
# distinguish between the regular and Express versions.
# XXX get_vs_by_version is internal to SCons and may change in the future
_msvsSupportedVersions = ("10.0", "10.0Exp")
for _msvsVersion in _msvsSupportedVersions:
    try:
        if SCons.Tool.MSCommon.vs.get_vs_by_version( _msvsVersion ): break
    except SCons.Errors.UserError: pass # "not supported by SCons"
else: _msvsVersion = None

def generate( env ):
    if _msvsVersion is None:
        raise SCons.Errors.UserError( "Visual Studio version %r is not installed" % _msvsSupportedVersions[0] )
    env["MSVC_VERSION"] = _msvsVersion
    env["yp_COMPILER"] = os.path.splitext( os.path.basename( __file__ ) )[0]
    _msvsTool.generate( env )
    _msvcTool.generate( env )
    _mslinkTool.generate( env )

    # TODO Options for warning levels, debug/release, etc

def exists( env ):
    return _msvsVersion is not None


