import asyncio
from collections.abc import Callable
from functools import partial
from typing import Protocol, Unpack, cast

from ._client_impl import Client, Event, Description
from .msgs import Message


class WaitCallable[T](Protocol):

    def __call__(self, wait_ms: int = 0, callback: Callable[[T], None] | None = None) -> T | None:
        ...

class WaitCallableTuple[*Ts](Protocol):

    def __call__(self, wait_ms: int = 0, callback: Callable[[*Ts], None] | None = None) -> tuple[*Ts] | None:
        ...

class WaitCallablePartial[T](Protocol):

    def __call__(self, wait_ms: int = 0, callback: Callable[[T, bool], None] | None = None) -> T | None:
        ...

async def call_async[T](f: WaitCallable[T]) -> T:
    loop = asyncio.get_running_loop()
    future: asyncio.Future[T] = loop.create_future()

    def cb(arg: T) -> None:
        if not future.cancelled():
            loop.call_soon_threadsafe(future.set_result, arg)

    f(wait_ms=0, callback=cb)
    return await future

async def call_async_tuple[*Ts](f: WaitCallableTuple[*Ts]) -> tuple[*Ts]:
    loop = asyncio.get_running_loop()
    future: asyncio.Future[tuple[*Ts]] = loop.create_future()

    def cb(*arg: Unpack[Ts]) -> None:
        if not future.cancelled():
            loop.call_soon_threadsafe(future.set_result, arg)

    f(wait_ms=0, callback=cb)
    return await future

async def call_async_partial[T](f: WaitCallablePartial[T], wait_ms: int) -> T | None:
    loop = asyncio.get_running_loop()
    future: asyncio.Future[T] = loop.create_future()

    p: T | None = None

    def cb(arg: T, complete: bool) -> None:
        nonlocal p
        if complete and not future.cancelled():
            loop.call_soon_threadsafe(future.set_result, arg)
        p = arg

    f(wait_ms=max(0, wait_ms), callback=cb)
    try:
        return await asyncio.wait_for(future, timeout=wait_ms*1e-3)
    except TimeoutError:
        pass
    return p

class ClientAsync(Client):

    async def scan(self, number: int = -1, wait_ms: int = 1000) -> set[int]:  # type: ignore[override]
        # self.ping()
        # nodes: set[int] = set()
        # async def f():
        #     while number <=0 or len(nodes) < number:
        #         r = await self.get_message()
        #         if r is not None:
        #             msg, _ = r
        #             if msg.type == 0x900C:
        #                 nodes.add(msg.source)
        # wait_ms = max(0, wait_ms)
        # try:
        #     await asyncio.wait_for(f(), timeout=wait_ms*1e-3)
        # except TimeoutError:
        #     pass
        # return nodes
        r = await call_async_partial(partial(super().scan, number=number), wait_ms=wait_ms)
        return r or set()

    async def _query(self, #type: ignore[override]
                          node: int) -> Description:
        return await call_async(partial(super()._query, node=node))

    async def get_message(self, #type: ignore[override]
                          node: int = -1,
                          type: int = -1) -> tuple[Message, int]:
        msg, target = await call_async_tuple(partial(super().get_message, node=node, type=type))
        return cast('Message', msg), target

    async def get_event(self, #type: ignore[override]
                        node: int, name: str) -> Event:
        return await call_async(partial(super().get_event, node=node, name=name))


    async def get_variable(self, #type: ignore[override]
                           node: int, name: str) -> list[int]:
        return await call_async(partial(super().get_variable, node=node, name=name))

    async def get_all_variables(self, #type: ignore[override]
                           node: int) -> dict[str, list[int]]:
        return await call_async(partial(super().get_all_variables, node=node))

    async def wait_nodes(self, #type: ignore[override]
                                   nodes: set[int] = set(), number: int = -1, wait_ms: int = 1000) -> set[int]:
        # wait_ms = max(0, wait_ms)
        # loop = asyncio.get_running_loop()
        # future: asyncio.Future[None] = loop.create_future()
        # c_nodes: set[int] = set()

        # def cb(nodes: set[int], done: bool) -> None:
        #     nonlocal c_nodes
        #     if not future.cancelled():
        #         c_nodes = nodes
        #         if done:
        #             loop.call_soon_threadsafe(future.set_result, None)

        # super().wait_nodes(nodes=nodes, number=number, wait_ms=0, callback=cb)
        # try:
        #     await asyncio.wait_for(future, timeout=wait_ms*1e-3)
        # except TimeoutError:
        #     pass
        # return c_nodes
        r = await call_async_partial(partial(super().wait_nodes, number=number), wait_ms=wait_ms)
        return r or set()

    async def wait_target_connection(self, #type: ignore[override]
                                     index: int = -1) -> tuple[int, str]:
        return await call_async_tuple(partial(super().wait_target_connection, index=index))

    async def wait_target_disconnection(self, #type: ignore[override]
                                     index: int = -1) -> tuple[int, str]:
        return await call_async_tuple(partial(super().wait_target_disconnection, index=index))

    async def wait_node_connection(self, #type: ignore[override]
                                   node: int = -1) -> int:
        return await call_async(partial(super().wait_node_connection, node=node))

    async def wait_node_disconnection(self, #type: ignore[override]
                                      node: int = -1) -> int:
        return await call_async(partial(super().wait_node_disconnection, node=node))

    async def connect(self, #type: ignore[override]
                      target: str,
                      wait_ms: int = 1000,
                      max_retries: int = 3,
                      ping: bool = True) -> bool:
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
            self._start(ping=ping)
        return connected
