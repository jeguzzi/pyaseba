import asyncio
import contextlib
from collections.abc import AsyncIterator, Iterator, Sequence

from ._client_impl import Client, Description, Event, scan_serial_ports
from .client_async import ClientAsync
from .msgs import CmdMessage, Message, UserMessage


@contextlib.contextmanager
def connect(target: str = '',
            targets: Sequence[str] = (),
            wait_ms: int = 1000,
            max_retries: int = 3,
            ping: bool = True,
            port: int = -1) -> Iterator[Client]:
    client = Client(port=port, query=ping)
    targets = list(targets)
    if target:
        targets.append(target)
    try:
        for targets in targets:
            client.connect(target,
                           wait_ms=wait_ms,
                           max_retries=max_retries,
                           ping=ping)
        yield client
    finally:
        client.close()


@contextlib.asynccontextmanager
async def connect_async(target: str = '',
                        targets: Sequence[str] = (),
                        wait_ms: int = 1000,
                        max_retries: int = 3,
                        ping: bool = True,
                        port: int = -1) -> AsyncIterator[ClientAsync]:
    client = ClientAsync(port=port, query=ping)
    targets = list(targets)
    if target:
        targets.append(target)
    try:
        await asyncio.gather(
            *[client.connect(target, ping=ping) for target in targets])
        yield client
    finally:
        client.close()


def find_serial_targets(name: str) -> list[str]:
    targets: list[str] = []
    for i, (device, desc) in scan_serial_ports().items():
        if name in desc:
            targets.append(f'ser:device={device}')
    return targets


__all__ = [
    'Client', 'ClientAsync', 'Description', 'Event', 'Message', 'CmdMessage',
    'UserMessage', 'scan_serial_ports', 'connect', 'connect_async',
    'find_serial_targets'
]
