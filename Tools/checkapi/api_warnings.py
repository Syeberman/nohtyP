from parse_header import ypHeader

# FIXME Test all the warnings.


def CheckVarargFunctions(warnings: list[str], header: ypHeader):
    for func in header.funcs:
        vararg_params = func.params and func.params[-1].type in ("...", "va_list")

        if not func.is_vararg:
            if vararg_params:
                warnings.append(
                    "vararg function missing N or K postfix: {}".format(func.name)
                )
            continue
        if not vararg_params:
            warnings.append(
                "N, K, or V used in non-vararg function: {}".format(func.name)
            )
            continue

        if func.postfix_param_count is not None:
            warnings.append(
                "vararg function contains input count postfix: {}".format(func.name)
            )

        if func.params[-2].type != "int" or func.params[-2].name != "n":
            warnings.append(
                "must have `int n` argument before varargs: {}".format(func.name)
            )

        if "V" in func.postfixes:
            if func.params[-1].type != "va_list":
                warnings.append("V used in non-va_list function{}".format(func.name))
            pair_name = func.rootname + func.postfixes.replace("V", "")
            if header.name2funcs.get(pair_name) is None:
                warnings.append(
                    "NV (or KV) missing N (or K) pair: {}".format(func.name)
                )
        else:
            if func.params[-1].type != "...":
                warnings.append(
                    "ellipsis not used in non-V function: {}".format(func.name)
                )
            pair_name = func.rootname + func.postfixes.replace("N", "NV").replace(
                "K", "KV"
            )
            if header.name2funcs.get(pair_name) is None:
                warnings.append(
                    "N (or K) missing NV (or KV) pair: {}".format(func.name)
                )


def CheckParameterCounts(warnings: list[str], header: ypHeader):
    for func in header.funcs:
        if func.postfix_param_count is None:
            continue

        param_count = len(func.params)

        if func.postfix_param_count != param_count:
            warnings.append(
                "parameter count postfix ({}) isn't correct ({}): {}".format(
                    func.postfix_param_count, param_count, func.name
                )
            )


# FIXME Check that functions that don't return ypObject* have an exc parameter.
def CheckSetExcFunctions(warnings: list[str], header: ypHeader):
    for func in header.funcs:
        if not any(p.name == "exc" for p in func.params):
            continue

        # FIXME If the function returns a ypObject * it doesn't need exc
        if "L" not in func.postfixes and func.params[0].type != "ypObject *":
            warnings.append(
                "first parameter of exc function isn't `ypObject *`: {}".format(
                    func.name
                )
            )

        exc_param = func.params[-3] if func.is_vararg else func.params[-1]
        if exc_param.type != "ypObject **" or exc_param.name != "exc":
            warnings.append(
                "`ypObject **exc` parameter not in expected location: {}".format(
                    func.name
                )
            )
