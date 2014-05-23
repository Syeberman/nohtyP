"""checkapi.py -- Static analysis and information for nohtyP.h

Author: Sye van der Veen
Date: May 19, 2014
"""

import sys, re, copy

# FIXME remove these hard-coded paths
CppPath = "../pycparser/utils/cpp"
CppArgs = "-I../pycparser/utils/fake_libc_include"
sys.path.append( "../pycparser" )
from pycparser import c_parser, c_generator, c_ast, parse_file


re_FuncName = re.compile( r"^(?P<root>yp_([ois]2[ois]_)?([a-z_]+|(asu?int\d+)|(asfloat\d+)))"
        "(?P<post>(CF?)?(LF?)?(NV?)?(KV?)?E?D?X?\d?)$" )
Generator = c_generator.CGenerator( )


class RemoveDeclNameVisitor( c_ast.NodeVisitor ):
    def visit_TypeDecl( self, node ):
        node.declname = ""
        self.generic_visit( node )
RemoveDeclName = RemoveDeclNameVisitor( ).visit
def ConvertToAbstractType( declnode ):
    """Returns a deep copy of declnode converted to an abstract type (no declnames)."""
    abstract = copy.deepcopy( declnode )
    RemoveDeclName( abstract )
    return abstract


class ypParameter:
    @classmethod
    def from_Decl( cls, declnode ):
        """Creates a ypParameter based on the Decl node."""
        self = cls( )
        if isinstance( declnode, c_ast.EllipsisParam ):
            self.name = self.type = "..."
        else:
            self.name = declnode.name
            self.type = Generator.visit( ConvertToAbstractType( declnode ) )
        return self

    def __str__( self ):
        return "%r: %r" % (self.name, self.type)


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
        self._complete( )

    def _complete( self ):
        """Common initialization for ypFunction."""
        namematch = re_FuncName.match( self.name )
        assert namematch is not None, self.name
        self.rootname = namematch.group( "root" )
        self.postfixes = namematch.group( "post" )
        print( self )

    def __str__( self ):
        return "%s %s %r (%s)" % (self.name, self.rootname, self.postfixes,
                ", ".join( str( x ) for x in self.params ))


class ApiVisitor( c_ast.NodeVisitor ):
    """Collects information from nohtyP.h."""
    def __init__( self ):
        self.entities = []

    def visit_Decl( self, node ):
        if isinstance( node.type, c_ast.FuncDecl ):
            self.entities.append( ypFunction.from_Decl( node ) )
            # node.show( )


# TODO Checks to implement
#   - ellipsis preceeded by int n
#   - ellipsis is in an N or K (va_list is NV or KV)
#   - exc is always ypObject ** and used in C, F, L, and E
#   - the count equals the actual arg count (minus exc)
#   - C is used iff it contains a C type (an int?)
#   - F is used iff it contains a float
#   - L doesn't use ypObject *
#   - report the functions that belong to the same group, and which is the unadorned version


def CheckApi( filename ):
    """Reports potential problems in nohtyP.h (and related files)."""
    # TODO improve cpp_args
    ast = parse_file( filename, use_cpp=True, cpp_path=CppPath, cpp_args=CppArgs )
    v = ApiVisitor( )
    v.visit( ast )


if __name__ == "__main__":
    CheckApi( "nohtyP.h" )

