

def CheckEllipsisFunctions(warnings, header):
    for function in header.IterFunctions():
        name = function.name
        postfixes = function.postfixes
        vararg_postfix = ("N" in postfixes or "K" in postfixes)
        params = function.params
        vararg_params = (params and params[-1].type in ("...", "va_list"))

        # Skip non-ellipsis functions
        if not vararg_params:
            if vararg_postfix:
                warnings.append("N or K used in non-vararg function: {}".format(name))
            continue

        if not vararg_postfix:
            warnings.append("vararg function missing N or K postfix: {}".format(name))
        if function.postfix_input_count is not None:
            warnings.append("vararg function contains input count postfix: {}".format(name))

        if params[-2].type != "int" or params[-2].name != "n":
            warnings.append("must have 'int n' argument before varargs: {}".format(name))

        if "V" in postfixes:
            if params[-1].type != "va_list":
                warnings.append("V used in non-va_list function{}".format(name))
        else:
            if params[-1].type != "...":
                warnings.append("ellipsis not used in non-V function: {}".format(name))


def CheckInputCounts(warnings, header):
    for function in header.IterFunctions():
        name = function.name
        postfix_input_count = function.postfix_input_count

        # Skip functions without an input count
        if postfix_input_count is None:
            continue

        input_count = sum(1 for param in function.params if param.input)

        if postfix_input_count != input_count:
            warnings.append("input count postfix ({}) isn't correct ({}): {}".format(
                postfix_input_count, input_count, name))

