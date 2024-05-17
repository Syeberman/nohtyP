"""parse_header.py -- Parses header files similar to nohtyP.h

Author: Sye van der Veen
Date: May 19, 2014
"""

import copy
from typing import Callable, TypeVar
from pycparser import c_generator, c_ast, parse_file
import re

K = TypeVar('K')
V = TypeVar('V')


def setdefault_call(d: dict[K, V], key: K, default_callable: Callable[[], V]):
    try:
        return d[key]
    except KeyError:
        default = d[key] = default_callable()
        return default


class RemoveDeclNameVisitor(c_ast.NodeVisitor):
    def visit_TypeDecl(self, node: c_ast.TypeDecl):
        node.declname = ""
        self.generic_visit(node)


RemoveDeclName = RemoveDeclNameVisitor().visit
TypeNameGenerator = c_generator.CGenerator()


def GenerateAbstractTypeName(declnode: c_ast.Node) -> str:
    """Returns declnode as an abstract type name.  Should only be called on a Decl node or on a
    FuncDecl's type node."""
    abstract = copy.deepcopy(declnode)
    RemoveDeclName(abstract)
    if not isinstance(abstract, c_ast.Decl):
        abstract = c_ast.Decl(
            name=None,
            quals=[],
            align=None,
            storage=[],
            funcspec=[],
            type=abstract,
            init=None,
            bitsize=None,
        )
    return TypeNameGenerator.visit(abstract)


class ypParameter:

    name: str
    type: str
    input: bool
    output: bool

    @classmethod
    def from_Decl(cls, declnode: c_ast.Decl, param_index):
        """Creates a ypParameter based on the Decl node."""
        self = cls()
        if isinstance(declnode, c_ast.EllipsisParam):
            self.name = self.type = "..."
        else:
            self.name = declnode.name
            self.type = GenerateAbstractTypeName(declnode)

        # If determining input/output gets too hairy we'll have to annotate the header somehow
        if "**" in self.type:
            # pointers to pointers are outputs
            self.input, self.output = False, True
        else:
            # all other parameters are inputs
            self.input, self.output = True, False

        return self

    def __str__(self):
        return "%s: %r" % (self.name, self.type)


class ypFunction:
    _re_name = re.compile(
        r"^(?P<root>yp_([ois]2[ois]_)?([a-z_]+|(asu?int\d+)|(asfloat\d+)))"
        r"(?P<post>(CF?)?(LF?)?(NV?)?(KV?)?E?D?X?(?P<post_incnt>\d?))$"
    )

    name: str
    params: list[ypParameter]
    returntype: str

    @classmethod
    def from_Decl(cls, declnode: c_ast.Decl):
        """Creates a ypFunction based on the Decl node."""
        paramsnode = declnode.type.args
        returnnode = declnode.type.type

        self = cls()
        self.name = declnode.name
        self.params = []
        if (
            len(paramsnode.params) > 1
            or getattr(paramsnode.params[0], "name", "") is not None
        ):
            for i, pdeclnode in enumerate(paramsnode.params):
                self.params.append(ypParameter.from_Decl(pdeclnode, i))
        self.returntype = GenerateAbstractTypeName(returnnode)
        self._complete()
        return self

    def _complete(self):
        """Common initialization for ypFunction."""
        namematch = ypFunction._re_name.match(self.name)
        if namematch is None:
            raise ValueError("couldn't parse function name %r" % self.name)
        self.rootname = namematch.group("root")
        self.postfixes = namematch.group("post")
        post_incnt = namematch.group("post_incnt")
        self.postfix_parameter_count = int(post_incnt) if post_incnt else None

    def __str__(self):
        return "%s (%s): %r" % (
            self.name,
            ", ".join(str(x) for x in self.params),
            self.returntype,
        )


class ypHeader:
    """Holds information collected information from nohtyP.h and similar files."""

    def __init__(self):
        self.funcs = list[ypFunction]()
        self.name2funcs = dict[str, list[ypFunction]]()  # a few functions are listed twice or more
        self.root2funcs = dict[str, list[ypFunction]]()

    def add_func(self, func: ypFunction):
        self.funcs.append(func)
        setdefault_call(self.name2funcs, func.name, list).append(func)
        setdefault_call(self.root2funcs, func.rootname, list).append(func)


class ApiVisitor(c_ast.NodeVisitor):
    """Collects information from nohtyP.h."""

    def __init__(self):
        self.header = ypHeader()

    def visit_Decl(self, node: c_ast.Decl):
        if isinstance(node.type, c_ast.FuncDecl):
            self.header.add_func(ypFunction.from_Decl(node))


def parse_header(filepath: str):
    """Parses nohtyP.h and similar files.  The file must have already been preprocessed."""
    ast = parse_file(filepath)
    visitor = ApiVisitor()
    visitor.visit(ast)
    return visitor.header
