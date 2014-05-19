"""checkapi.py -- Static analysis and information for nohtyP.h

Author: Sye van der Veen
Date: May 19, 2014
"""

import sys

# FIXME remove these hard-coded paths
CppPath = "../pycparser/utils/cpp"
CppArgs = "-I../pycparser/utils/fake_libc_include"
sys.path.append( "../pycparser" )
from pycparser import c_parser, c_ast, parse_file


class ApiVisitor( c_ast.NodeVisitor ):
    def __init__( self ):
        pass

    def visit_Decl( self, node ):
        if isinstance( node.type, c_ast.FuncDecl ):
            node.show( )


def CheckApi( filename ):
    # TODO improve cpp_args
    ast = parse_file( filename, use_cpp=True, cpp_path=CppPath, cpp_args=CppArgs )
    v = ApiVisitor( )
    v.visit( ast )


if __name__ == "__main__":
    CheckApi( "nohtyP.h" )

