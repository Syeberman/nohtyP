"""parse_header.py -- Parses header files similar to nohtyP.h

Author: Sye van der Veen
Date: May 19, 2014
"""

import re, copy, collections
from pycparser import c_generator, c_ast, parse_file


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
        if namematch is None:
            raise ValueError( "couldn't parse function name %r" % self.name )
        self.rootname = namematch.group( "root" )
        self.postfixes = namematch.group( "post" )

    def __str__( self ):
        return "%s (%s): %r" % (
                self.name, ", ".join( str( x ) for x in self.params ), self.returntype)

class ypHeader:
    """Holds information collected information from nohtyP.h and similar files."""
    def __init__( self ):
        self._functions = []
        self._root2functions = collections.OrderedDict()

    def AddFunction( self, function ):
        self._functions.append( function )

        try: self._root2functions[function.rootname].append( function )
        except KeyError: self._root2functions[function.rootname] = [function, ]

    def IterFunctions( self ):
        """Iterates over all parsed functions, in the order they were added (which is quite likely
        the order from the header file)."""
        return iter( self._functions )

    def IterFunctionRoots( self ):
        """Yields 2-tuples of the function root string and all functions with that root, in the
        order they were added."""
        for root, functions in self._root2functions.items():
            yield root, tuple( functions )

    def IterFunctionsWithRoot( self, root ):
        """Iterates over all functions that share the given root, in the order they were added."""
        return iter( self._root2functions[root] )


class ApiVisitor( c_ast.NodeVisitor ):
    """Collects information from nohtyP.h."""
    def __init__( self ):
        self.header = ypHeader()

    def visit_Decl( self, node ):
        if isinstance( node.type, c_ast.FuncDecl ):
            self.header.AddFunction( ypFunction.from_Decl( node ) )


def ParseHeader( filepath ):
    """Parses nohtyP.h and similar files.  The file must have alredy been preprocessed."""
    ast = parse_file( filepath )
    visitor = ApiVisitor( )
    visitor.visit( ast )
    return visitor.header

