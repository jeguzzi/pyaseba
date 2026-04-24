from typing import cast

from ._client_impl import scan_serial_ports


def find_serial_targets(name: str) -> list[str]:
    """
    Scans for Dashel serial targets that contains a given name.

    :param      name:  The name

    :returns:   A list of Dashel targets
    """
    targets: list[str] = []
    for i, (device, desc) in scan_serial_ports().items():
        if name in desc:
            targets.append(f'ser:device={device}')
    return targets


def get_target_protocol(target: str) -> str:
    """
    Reads the protocol string from a Dashel target

    >>> get_target_protocol("tcp:port=33333")
    'tcp'

    :param target: A Dashel target
    :return: The protocol string
    """
    return target.split(':')[0]


def _read_parameter(text: str) -> tuple[str, str] | None:
    ls = text.split('=')
    if len(ls) == 2:
        return cast('tuple[str, str]', tuple(ls))
    return None


def get_target_parameters(target: str) -> dict[str, str]:
    """
    Reads the parameters from a Dashel target

    >>> get_target_protocol("tcp:port=33333;host=localhost")
    {'port': '33333', 'host': 'localhost'}

    :param target: A Dashel target
    :return: A dictionary of parameters
    """
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
    return all(params[key] == other_params[key]
               for key in set(params) & set(other_params))
