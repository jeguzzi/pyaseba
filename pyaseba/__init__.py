import logging

from .client import Client, ClientAsync
from .client import Description as ClientDescription
from .client._client_impl import _set_logger_level as _set_client_logger_level
from .network import Description as NetworkDescription
from .network import Network, Node
from .network._network_impl import \
    _set_logger_level as _set_network_logger_level


def get_logger() -> logging.Logger:
    """
    Gets the pyaseba logger.

    :returns:   The logger.
    """
    return logging.getLogger("pyaseba")


def set_logger_level(level: int | str) -> None:
    """
    Sets the log level. If negative, it disables logging.

    :param      level:  The level, like :py:attr:`logging.INFO`
    """
    if isinstance(level, str):
        level = logging.getLevelNamesMapping().get(level.upper(), -1)
    if level < 0:
        name = "off"
    if level < logging.DEBUG:
        name = "trace"
    elif level < logging.INFO:
        name = "debug"
    elif level < logging.WARN:
        name = "info"
    elif level < logging.ERROR:
        name = "warning"
    elif level < logging.CRITICAL:
        name = "error"
    else:
        name = "critical"
    _set_network_logger_level(name)
    _set_client_logger_level(name)
    if level < 0:
        get_logger().disabled = True
    else:
        get_logger().disabled = False
        get_logger().setLevel(level)


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
