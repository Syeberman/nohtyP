"""parse_header.py -- Parses header files similar to nohtyP.h

Author: Sye van der Veen
Date: May 19, 2014
"""

import copy
import io
from typing import Callable, TypeVar
from pycparser import c_generator, c_ast, CParser
import re

K = TypeVar("K")
V = TypeVar("V")


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
    is_input: bool
    is_output: bool

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
            self.is_input, self.is_output = False, True
        else:
            # all other parameters are inputs
            self.is_input, self.is_output = True, False

        return self

    def __str__(self):
        return "%s: %r" % (self.name, self.type)


class ypFunction:
    _re_name = re.compile(
        r"^(?P<root>yp_([ois]2[ois]_)?([a-z_]+|(asu?int\d+)|(asfloat\d+)))"
        r"(?P<post>(CF?)?(LF?)?(NV?)?(KV?)?D?X?(?P<post_paramcnt>\d?))$"
    )
    _always_succeeds_roots = frozenset(
        ("yp_incref", "yp_decref", "yp_isexception", "yp_iscallable", "yp_type")
    )
    _support_names = frozenset(
        (
            "yp_initialize",
            "yp_mem_default_malloc",
            "yp_mem_default_malloc_resize",
            "yp_mem_default_free",
        )
    )

    name: str
    params: list[ypParameter]
    returntype: str
    rootname: str
    postfixes: str
    postfix_param_count: int | None
    is_vararg: bool  # Works with variable arguments.
    is_exc: bool  # Returns exceptions through a `ypObject **exc` parameter.
    always_succeeds: bool  # Does not raise exceptions.
    is_support: bool  # Supports nohtyP (i.e. yp_initialize, yp_mem_*).

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
        postfixes = namematch.group("post")
        self.postfixes = postfixes
        post_paramcnt = namematch.group("post_paramcnt")
        self.postfix_param_count = int(post_paramcnt) if post_paramcnt else None
        self.is_vararg = "N" in postfixes or "K" in postfixes or "V" in postfixes
        self.is_exc = any(p.name == "exc" for p in self.params)
        self.always_succeeds = self.rootname in ypFunction._always_succeeds_roots
        self.is_support = self.name in ypFunction._support_names

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
        # A few functions are listed twice or more, so we store as a list.
        self.name2funcs = dict[str, list[ypFunction]]()
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
    # The "optimize" options disable the lextab.py and yacctab.py cached files.
    parser = CParser(lex_optimize=False, yacc_optimize=False)
    with io.open(filepath) as f:
        ast = parser.parse(f.read(), filepath)

    visitor = ApiVisitor()
    visitor.visit(ast)
    return visitor.header
