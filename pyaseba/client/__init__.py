from ._client_impl import (Client, Description, Event, Message,
                           scan_serial_ports, complete_target)
from .client_async import ClientAsync
from .node import Node
from .node_async import NodeAsync


def print_description(node_id: int, description: Description) -> None:
    """
    Pretty-prints a node description.

    :param      node_id:      The node id
    :param      description:  The node description
    """
    title = f"Node {node_id}"
    title += "\n" + len(title) * "="
    print(title)
    if description.variables:
        print("\nVariables\n---------")
        for name, (index, size) in description.variables.items():
            print(f"- {name}[{size}]")
    if description.local_events:
        print("\nLocal events\n------------")
        for name, desc in description.local_events.items():
            if desc:
                print(f"- {name}: {desc}")
            else:
                print(f"- {name}")
    if description.user_events:
        print("\nUser events\n------------")
        for name, size in description.user_events.items():
            print(f"- {name} [{size}]")
    if description.functions:
        print("\nFunctions\n---------")
        for name, (desc, args) in description.functions.items():
            argument = ', '.join(f'{name}[{size}]' for name, size in args)
            if desc:
                print(f"- {name}({argument}): {desc}")
            else:
                print(f"- {name}({argument})")
    print()


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
    'complete_target', 'print_description'
]
