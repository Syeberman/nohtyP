from parse_header import ypHeader


def CheckEllipsisFunctions(warnings: list[str], header: ypHeader):
    for function in header.funcs:
        name = function.name
        postfixes = function.postfixes
        vararg_postfix = "N" in postfixes or "K" in postfixes
        params = function.params
        vararg_params = params and params[-1].type in ("...", "va_list")

        # Skip non-ellipsis functions
        if not vararg_params:
            if vararg_postfix:
                warnings.append("N or K used in non-vararg function: {}".format(name))
            continue

        if not vararg_postfix:
            warnings.append("vararg function missing N or K postfix: {}".format(name))
        if function.postfix_input_count is not None:
            warnings.append(
                "vararg function contains input count postfix: {}".format(name)
            )

        if params[-2].type != "int" or params[-2].name != "n":
            warnings.append(
                "must have 'int n' argument before varargs: {}".format(name)
            )

        if "V" in postfixes:
            if params[-1].type != "va_list":
                warnings.append("V used in non-va_list function{}".format(name))
            pair_name = function.rootname + postfixes.replace("V", "")
            if header.name2funcs.get(pair_name) is None:
                warnings.append("NV (or KV) missing N (or K) pair: {}".format(name))
        else:
            if params[-1].type != "...":
                warnings.append("ellipsis not used in non-V function: {}".format(name))
            pair_name = function.rootname + postfixes.replace("N", "NV").replace(
                "K", "KV"
            )
            if header.name2funcs.get(pair_name) is None:
                warnings.append("N (or K) missing NV (or KV) pair: {}".format(name))


def CheckInputCounts(warnings: list[str], header: ypHeader):
    for function in header.funcs:
        name = function.name
        postfix_input_count = function.postfix_input_count

        # Skip functions without an input count
        if postfix_input_count is None:
            continue

        input_count = sum(1 for param in function.params if param.input)

        if postfix_input_count != input_count:
            warnings.append(
                "input count postfix ({}) isn't correct ({}): {}".format(
                    postfix_input_count, input_count, name
                )
            )


# TODO In reverse, check for ypObject ** functions that are missing E versions
def CheckSetExcFunctions(warnings: list[str], header: ypHeader):
    for func in header.funcs:
        if "E" not in func.postfixes:
            continue

        name = func.name
        pair_name = func.rootname + func.postfixes.replace("E", "")

        pairs = header.name2funcs.get(pair_name)
        if pairs is None:
            warnings.append("E function missing non-E version: {}".format(name))
            continue

        # TODO If the function returns a ypObject *, does it really need exc?
        first_param = func.params[0]
        last_param = func.params[-1]
        if first_param.type != "ypObject *":
            warnings.append(
                "first parameter of E function isn't `ypObject *`: {}".format(name)
            )
        if last_param.type != "ypObject **" or last_param.name != "exc":
            warnings.append(
                "last parameter of E function isn't `ypObject **exc`: {}".format(name)
            )

        # TODO Check parameter names are the same?  But param names change...
        for pair in pairs:
            if pair.params[0].type != "ypObject **":
                warnings.append(
                    "first parameter of non-E function isn't `ypObject **`: {}".format(
                        name
                    )
                )

            for i in range(1, len(pair.params)):
                if pair.params[i].type != func.params[i].type:
                    warnings.append(
                        "different types for param `{}` from non-E function: {}".format(
                            func.params[i].name, name
                        )
                    )
