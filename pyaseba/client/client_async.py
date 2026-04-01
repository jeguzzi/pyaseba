import asyncio
from collections.abc import Callable
from functools import partial
from typing import Protocol, SupportsIndex, SupportsInt

from ._client_impl import Client, Event
from .msgs import Message


class WaitCallable[R](Protocol):

    def __call__(self, wait_ms: SupportsInt | SupportsIndex = 0, callback: Callable[[R], None] | None = None) -> R | None:
        ...


async def call_async[R](f: WaitCallable[R]) -> R:
    loop = asyncio.get_running_loop()
    future: asyncio.Future[R] = loop.create_future()

    def cb(arg: R) -> None:
        if not future.cancelled():
            loop.call_soon_threadsafe(future.set_result, arg)

    f(wait_ms=0, callback=cb)
    return await future


class ClientAsync(Client):

    async def get_message(self, #type: ignore[override]
                          node: SupportsInt | SupportsIndex = -1,
                          type: SupportsInt | SupportsIndex = -1) -> Message:
        return await call_async(partial(super().get_message, node=node, type=type))

    async def get_event(self, #type: ignore[override]
                        node: SupportsInt | SupportsIndex, name: str) -> Event:
        return await call_async(partial(super().get_event, node=node, name=name))

    async def get_variable(self, #type: ignore[override]
                           node: SupportsInt | SupportsIndex, name: str) -> list[int]:
        return await call_async(partial(super().get_variable, node=node, name=name))

    async def get_all_variables(self, #type: ignore[override]
                           node: SupportsInt | SupportsIndex) -> dict[str, int]:
        return await call_async(partial(super().get_all_variables, node=node))

    async def wait_node_connection(self, #type: ignore[override]
                                   node: SupportsInt | SupportsIndex = -1) -> int:
        return await call_async(partial(super().wait_node_connection, node=node))

    async def wait_node_disconnection(self, #type: ignore[override]
                                      node: SupportsInt | SupportsIndex = -1) -> int:
        return await call_async(partial(super().wait_node_disconnection, node=node))

    async def wait_disconnection(self) -> bool: #type: ignore[override]
        return await call_async(partial(super().wait_disconnection))

    async def connect(self, #type: ignore[override]
                      target: str,
                      wait_ms: SupportsInt | SupportsIndex = 1000,
                      max_retries: SupportsInt | SupportsIndex = 3) -> bool:
        failed = False
        connected = False
        max_retries = int(max_retries)
        wait_ms = int(wait_ms)
        max_retries = max(max_retries, 0)
        while max_retries >= 0:
            connected = self._connect(target)
            if connected:
                break
            max_retries -= 1
            failed = True
            await asyncio.sleep(wait_ms * 1e-3)
        if connected:
            if failed:
                # HACK: else coppelia-sim aseba not connected if started after python
                await asyncio.sleep(1)
            self._start()
        return connected
