from .client import Client, ClientAsync
from .network import Network, Node

from .client import Description as ClientDescription
from .network import Description as NetworkDescription


def print_description(
        node_id: int,
        description: ClientDescription | NetworkDescription,
        prefix: str = '') -> None:
    """
    Pretty-prints a node description.

    :param      node_id:      The node id
    :param      description:  The node description
    :param      prefix:       Prepended to the title.
    """
    title = f"{prefix}Node {node_id}"
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
    if isinstance(description, ClientDescription) and description.user_events:
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


__all__ = ['Client', 'ClientAsync', 'Network', 'Node', 'print_description']
