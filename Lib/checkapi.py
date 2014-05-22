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
    abstract = copy.deepcopy( declnode )
    RemoveDeclName( abstract )
    return abstract


class ypParameter:
    @classmethod
    def from_Decl( cls, declnode ):
        declnode.show( )
        self = cls( )
        self.name = declnode.name
        self.type = Generator.visit( ConvertToAbstractType( declnode ) )
        return self
    
    def __str__( self ):
        return "%r %r" % (self.type, self.name)


class ypFunction:
    @classmethod
    def from_Decl( cls, declnode ):
        paramsnode = declnode.type.args
        returnnode = declnode.type.type

        self = cls( )
        self.name = declnode.name
        self.params = []
        for pdeclnode in paramsnode.params:
            self.params.append( ypParameter.from_Decl( pdeclnode ) )
        self._complete( )

    def _complete( self ):
        """Common initialization"""
        namematch = re_FuncName.match( self.name )
        assert namematch is not None, self.name
        self.rootname = namematch.group( "root" )
        self.postfixes = namematch.group( "post" )
        print( self.name, self.rootname, self.postfixes, *self.params )



class ApiVisitor( c_ast.NodeVisitor ):
    def __init__( self ):
        self.entities = []

    def visit_Decl( self, node ):
        if isinstance( node.type, c_ast.FuncDecl ):
            self.entities.append( ypFunction.from_Decl( node ) )
            # node.show( )


def CheckApi( filename ):
    # TODO improve cpp_args
    ast = parse_file( filename, use_cpp=True, cpp_path=CppPath, cpp_args=CppArgs )
    v = ApiVisitor( )
    v.visit( ast )


if __name__ == "__main__":
    CheckApi( "nohtyP.h" )

