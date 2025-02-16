from parse_header import ypHeader

# TODO C functions match the non-C version in expected ways. (And L functions.) (And F.)
# TODO L functions do not reference ypObject *.


def CheckVarargFunctions(warnings: list[str], header: ypHeader):
    for func in header.funcs:
        vararg_params = func.params and func.params[-1].type in ("...", "va_list")

        if not func.is_vararg:
            if vararg_params:
                warnings.append(f"vararg function missing N or K postfix: {func.name}")
            continue
        if not vararg_params:
            warnings.append(f"N, K, or V used in non-vararg function: {func.name}")
            continue

        if func.postfix_param_count is not None:
            warnings.append(
                f"vararg function contains input count postfix: {func.name}"
            )

        count_param = "k" if "K" in func.postfixes else "n"
        if func.params[-2].type != "int" or func.params[-2].name != count_param:
            warnings.append(
                f"must have `int {count_param}` argument before varargs: {func.name}"
            )

        if "V" in func.postfixes:
            if func.params[-1].type != "va_list":
                warnings.append(f"V used in non-va_list function: {func.name}")
            pair_name = func.rootname + func.postfixes.replace("V", "")
            if header.name2funcs.get(pair_name) is None:
                warnings.append(f"NV (or KV) missing N (or K) pair: {func.name}")
        else:
            if func.params[-1].type != "...":
                warnings.append(f"ellipsis not used in non-V function: {func.name}")
            pair_name = func.rootname + func.postfixes.replace("N", "NV").replace(
                "K", "KV"
            )
            if header.name2funcs.get(pair_name) is None:
                warnings.append(f"N (or K) missing NV (or KV) pair: {func.name}")


def CheckParameterCounts(warnings: list[str], header: ypHeader):
    for func in header.funcs:
        if func.postfix_param_count is None:
            continue

        param_count = len(func.params)

        if func.postfix_param_count != param_count:
            warnings.append(
                f"parameter count postfix ({func.postfix_param_count}) isn't correct ({param_count}): {func.name}"
            )


def CheckSetExcFunctions(warnings: list[str], header: ypHeader):
    for func in header.funcs:
        if func.always_succeeds or func.is_support:
            continue

        if not func.is_exc:
            has_out_params = (
                any(p.type == "ypObject **" for p in func.params)
                or func.rootname == "yp_unpack"
            )
            if func.returntype != "ypObject *" and not has_out_params:
                warnings.append(
                    f"must return `ypObject *` or have `ypObject **` output parameter: {func.name}"
                )
            continue

        if func.returntype == "ypObject *":
            warnings.append(
                f"`ypObject **exc` not necessary, `ypObject *` is returned: {func.name}"
            )

        if "L" not in func.postfixes and func.params[0].type != "ypObject *":
            warnings.append(
                f"first parameter of non-L exc function isn't `ypObject *`: {func.name}"
            )

        exc_param = func.params[-3] if func.is_vararg else func.params[-1]
        if exc_param.type != "ypObject **" or exc_param.name != "exc":
            warnings.append(
                f"`ypObject **exc` parameter not in expected location: {func.name}"
            )
