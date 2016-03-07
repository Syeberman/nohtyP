"""Global variables from this file are automatically added to the SConScript (i.e. make.scons)
"""


from root_environment import SconscriptLog, RootEnv


# This ensures that targets like "test" will fail if Python 3 isn't available
def AliasIfNotEmpty( alias, targets=(), action=() ):
    """Creates or updates the alias iff targets or action is not empty.  Empty aliases are normally
    successful no-ops when specified as build targets on the command line: this ensures that aliases
    that don't actually do anything will cause build failures.
    """
    targets = Flatten( targets )
    action = Flatten( action )
    if not targets and not action: return alias
    return Alias( alias, targets, action )





def TODONameThis():
    # TODO Just like createObjBuilders, this needs to be called for each tool
    SourceFileScanner = rootEnv['BUILDERS']['Object'].source_scanner
    rootEnv['BUILDERS']['CPreprocessed'] = SCons.Builder.Builder(
        action = {},
        emitter = {},
        suffix = '$CPREPROCESSEDSUFFIX',
        src_builder = 'CFile',
        source_scanner = SourceFileScanner,
        single_source = 1
    )
    rootEnv['BUILDERS']['CXXPreprocessed'] = SCons.Builder.Builder(
        action = {},
        emitter = {},
        suffix = '$CXXPREPROCESSEDSUFFIX',
        src_builder = 'CXXFile',
        source_scanner = SourceFileScanner,
        single_source = 1
    )
    rootEnv.SetDefault( CPREPROCESSEDSUFFIX = '.i' )
    rootEnv.SetDefault( CXXPREPROCESSEDSUFFIX = '.ii' )

