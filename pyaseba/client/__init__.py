from ._client_impl import (Client, Description, Event, Message,
                           scan_serial_ports, complete_target)
from .client_async import ClientAsync, wrap_callback
from .node import Node, NodeAsync, EventSpec


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
