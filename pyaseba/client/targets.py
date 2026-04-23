from typing import cast


def get_target_protocol(target: str) -> str:
    return target.split(':')[0]


def _read_parameter(text: str) -> tuple[str, str] | None:
    ls = text.split('=')
    if len(ls) == 2:
        return cast('tuple[str, str]', tuple(ls))
    return None


def get_target_parameters(target: str) -> dict[str, str]:
    ls = target.split(':')
    if len(ls) != 2:
        return {}
    return dict(pair for pair in (_read_parameter(p)
                                  for p in ls[-1].split(";")) if pair)


def are_targets_compatible(target: str, other: str) -> bool:
    """
    Checks whether string may represent the same Dashel target,
    i.e., if their protocols and parameters overlap.

    :param      target:  Dashel target name
    :param      other:   Dashel target name

    :returns:   True if they represent the same Dashel target
    """
    if get_target_protocol(target) != get_target_protocol(other):
        return False
    params = get_target_parameters(target)
    other_params = get_target_parameters(other)
    return all(params[key] == other_params[key] for key in set(params) & set(other_params))
