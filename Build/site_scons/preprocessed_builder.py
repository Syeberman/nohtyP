"""Used by the compilers to configure a "Preprocessed" Builder.
"""
import SCons.Tool
import SCons.Builder


# TODO Contribute this back to SCons

# See SCons/Tool/__init__.py
CPreprocessedSuffix = [".i", ".ii"]

for suffix in CPreprocessedSuffix:
    SCons.Tool.SourceFileScanner.add_scanner(suffix, SCons.Tool.CScanner)


def createPreprocessedBuilder(env):
    """This is a utility function that creates the Preprocessed Builders in an Environment if they
    are not there already.

    If they are there already, we return the existing one.

    Returns the Preprocessed builder.
    """

    try:
        preprocessed = env['BUILDERS']['Preprocessed']
    except KeyError:
        preprocessed = SCons.Builder.Builder(action={},
                                             emitter={},
                                             #prefix = '',
                                             # TODO gcc uses different suffixes for C and C++
                                             #suffix = '$CPREPROCESSEDSUFFIX',
                                             src_builder=['CFile', 'CXXFile'],
                                             source_scanner=SCons.Tool.SourceFileScanner,
                                             single_source=True)
        env['BUILDERS']['Preprocessed'] = preprocessed

        # TODO msvc preprocesses both C and C++ files with a .i extension; gcc keeps them separate
        env.SetDefault(CPREPROCESSEDSUFFIX='.i')
        env.SetDefault(CXXPREPROCESSEDSUFFIX='.ii')

    return preprocessed
