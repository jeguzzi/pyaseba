from ._client_impl import (Client, Description, Event, Message, _init_logger,
                           complete_target, scan_serial_ports)

try:
    from ._client_impl import DeviceInfoType  # noqa: F401
except ImportError:
    pass
from .client_async import ClientAsync, wrap_callback
from .node import EventSpec, Node, NodeAsync

_init_logger()


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


__all__ = [
    'Client', 'ClientAsync', 'Description', 'Event', 'Message',
    'scan_serial_ports', 'find_serial_targets', 'Node', 'NodeAsync',
    'complete_target', 'EventSpec', 'wrap_callback'
]
