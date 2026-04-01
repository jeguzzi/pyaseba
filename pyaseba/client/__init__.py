import contextlib
from collections.abc import AsyncIterator, Iterator

from ._client_impl import Description, Event, Client
from .msgs import CmdMessage, Message, UserMessage
from .client_async import ClientAsync


@contextlib.contextmanager
def connect(target: str,
            wait_ms: int = 1000,
            max_retries: int = 3) -> Iterator[Client]:
    client = Client()
    try:
        client.connect(target, wait_ms=wait_ms, max_retries=max_retries)
        yield client
    finally:
        client.close()


@contextlib.asynccontextmanager
async def connect_async(target: str,
                        wait_ms: int = 1000,
                        max_retries: int = 3) -> AsyncIterator[ClientAsync]:
    client = ClientAsync()
    try:
        await client.connect(target)
        yield client
    finally:
        client.close()


__all__ = [
    'Client', 'ClientAsync', 'Description', 'Event', 'Message', 'CmdMessage',
    'UserMessage'
]
