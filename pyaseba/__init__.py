import contextlib
from collections.abc import AsyncIterator, Iterator

from .msgs import CmdMessage, Message, UserMessage
from .network_async import NetworkAsync
from .pyaseba import Description, Event, Network


@contextlib.contextmanager
def open(target: str,
         wait_ms: int = 1000,
         max_retries: int = 3) -> Iterator[Network]:
    network = Network()
    try:
        network.connect(target, wait_ms=wait_ms, max_retries=max_retries)
        yield network
    finally:
        network.close()


@contextlib.asynccontextmanager
async def open_async(target: str,
                     wait_ms: int = 1000,
                     max_retries: int = 3) -> AsyncIterator[NetworkAsync]:
    network = NetworkAsync()
    try:
        await network.connect(target)
        yield network
    finally:
        network.close()


__all__ = [
    'Network', 'NetworkAsync', 'Description', 'Event', 'Message', 'CmdMessage',
    'UserMessage'
]
