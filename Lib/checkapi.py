"""checkapi.py -- Static analysis and information for nohtyP.h

Author: Sye van der Veen
Date: May 19, 2014
"""

# FIXME should yp_popvalue* only return an exception...or should yp_send discard the iterator?
#   (it's inconsistent)

import sys, re, copy, collections

# FIXME remove these hard-coded paths
CppPath = "../pycparser/utils/cpp"
CppArgs = "-I../pycparser/utils/fake_libc_include"
sys.path.append( "../pycparser" )
from pycparser import c_parser, c_generator, c_ast, parse_file


##
## Header file parsing
##

re_FuncName = re.compile( r"^(?P<root>yp_([ois]2[ois]_)?([a-z_]+|(asu?int\d+)|(asfloat\d+)))"
        "(?P<post>(CF?)?(LF?)?(NV?)?(KV?)?E?D?X?\d?)$" )

class RemoveDeclNameVisitor( c_ast.NodeVisitor ):
    def visit_TypeDecl( self, node ):
        node.declname = ""
        self.generic_visit( node )
RemoveDeclName = RemoveDeclNameVisitor( ).visit
TypeNameGenerator = c_generator.CGenerator( )
def GenerateAbstractTypeName( declnode ):
    """Returns declnode as an abstract type name.  Should only be called on a Decl node or on a
    FuncDecl's type node."""
    abstract = copy.deepcopy( declnode )
    RemoveDeclName( abstract )
    if not isinstance( abstract, c_ast.Decl ):
        abstract = c_ast.Decl( name=None, quals=[], storage=[], funcspec=[], type=abstract, 
                init=None, bitsize=None )
    return TypeNameGenerator.visit( abstract )

class ypParameter:
    @classmethod
    def from_Decl( cls, declnode ):
        """Creates a ypParameter based on the Decl node."""
        self = cls( )
        if isinstance( declnode, c_ast.EllipsisParam ):
            self.name = self.type = "..."
        else:
            self.name = declnode.name
            self.type = GenerateAbstractTypeName( declnode )
        return self

    def __str__( self ):
        return "%s: %r" % (self.name, self.type)

class ypFunction:
    @classmethod
    def from_Decl( cls, declnode ):
        """Creates a ypFuncion based on the Decl node."""
        paramsnode = declnode.type.args
        returnnode = declnode.type.type

        self = cls( )
        self.name = declnode.name
        self.params = []
        if len( paramsnode.params ) > 1 or getattr( paramsnode.params[0], "name", "" ) is not None:
            for pdeclnode in paramsnode.params:
                self.params.append( ypParameter.from_Decl( pdeclnode ) )
        self.returntype = GenerateAbstractTypeName( returnnode )
        self._complete( )
        return self

    def _complete( self ):
        """Common initialization for ypFunction."""
        namematch = re_FuncName.match( self.name )
        assert namematch is not None, self.name
        self.rootname = namematch.group( "root" )
        self.postfixes = namematch.group( "post" )

    def __str__( self ):
        return "%s (%s): %r" % (
                self.name, ", ".join( str( x ) for x in self.params ), self.returntype)

class ApiVisitor( c_ast.NodeVisitor ):
    """Collects information from nohtyP.h."""
    def __init__( self ):
        self.functions = []

    def visit_Decl( self, node ):
        if isinstance( node.type, c_ast.FuncDecl ):
            self.functions.append( ypFunction.from_Decl( node ) )


##
## API consistency checks
##

# TODO Checks to implement
#   - ellipsis preceeded by int n
#   - ellipsis is in an N or K (va_list is NV or KV)
#   - every (with exceptions) N or K has a NV or KV
#   - every N or K has a variant that takes a ypObject* in its place
#   - exc is always ypObject ** and used in C, F, L, and E
#   - E functions match their originals, except:
#       - first arg is ypObject*, not ypObject**
#       - ypObject** exc is append to arg list, unless orig returns ypObject* (ie yp_popE)
#   - all X functions return a ypObject *
#   - the count equals the actual arg count (minus exc)
#   - C is used iff it contains a C type (an int?)
#   - F is used iff it contains a float
#   - L doesn't use ypObject *
#   - a whitelist of X functions?
#   - determine whether the function mutates or not based on certain patterns (push, pop, set, del,
#   i*, etc, or first param is iterator, file, etc) and ensure ypObject** is used iff it mutates
#   - the "unadorned" version is the one with the fewest arguments (but not zero)

# TODO nohtyP.c checks to implement (in a separate script)
#   - ensure no reference leaks
#   - for functions that return yp_None for success or exactly one exception type, convert to a
#   boolean (on the assumption that it's quicker to test)

def ReportOnVariants( header, *, print=print ):
    """Report the functions that belong to the same group, and which is the unadorned version."""
    root2funcs = collections.defaultdict( list )
    for func in header.functions:
        root2funcs[func.rootname].append( func )

    for root, funcs in sorted( root2funcs.items( ) ):
        if len( funcs ) < 2: continue
        #if not any( x.name[-1].isdigit( ) for x in funcs ): continue
        funcs.sort( key=lambda x: x.postfixes )
        print( root )
        for func in funcs: print( "    ", func )
        print( )


def CheckApi( filename, *, print=print ):
    """Reports potential problems in nohtyP.h (and related files)."""
    # TODO improve cpp_args
    ast = parse_file( filename, use_cpp=True, cpp_path=CppPath, cpp_args=CppArgs )
    header = ApiVisitor( )
    header.visit( ast )

    ReportOnVariants( header, print=print )


if __name__ == "__main__":
    CheckApi( "nohtyP.h" )

