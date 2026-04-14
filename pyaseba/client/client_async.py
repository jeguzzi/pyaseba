import asyncio
from collections.abc import Callable
from functools import partial
from typing import Any, Protocol, Unpack, cast

from ._client_impl import Client, Description, Event, Message, complete_target


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
    """
    Offers an asynchronous alternative to :py:class:`pyaseba.client.Client`
    replacing methods that blocks (and typically takes ``wait_ms: int`` as argument)
    with coroutines.

    The reset of the interface is identical to :py:class:`pyaseba.client.Client`.
    """

    async def scan(self, #type: ignore[override]
                   number: int = -1,
                   wait_ms: int = 1000) -> dict[int, set[int]]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.scan`
        """
        r = await call_async_partial(partial(super().scan, number=number), wait_ms=wait_ms)
        return r or {}

    async def query_description(self, #type: ignore[override]
                                node_id: int,
                                include: set[int] = set(),
                                exclude: set[int] = set()) -> Description:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.query_description`
        """
        return await call_async(partial(super().query_description, node_id=node_id, include=include, exclude=exclude))

    async def get_message(self, #type: ignore[override]
                          node_id: int = -1,
                          type: int = -1,
                          include: set[int] = set(),
                          exclude: set[int] = set()) -> tuple[Message, int]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.get_message`
        """
        msg, target = await call_async_tuple(partial(super().get_message, node_id=node_id, type=type, include=include, exclude=exclude))
        return cast('Message', msg), target

    async def get_event(self, #type: ignore[override]
                        node_id: int,
                        name: str,
                        include: set[int] = set(),
                        exclude: set[int] = set()) -> Event:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.get_event`
        """
        return await call_async(partial(super().get_event, node_id=node_id, name=name, include=include, exclude=exclude))


    async def get_variable(self, #type: ignore[override]
                           node_id: int,
                           name: str,
                           include: set[int] = set(),
                           exclude: set[int] = set()) -> list[int]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.get_variable`
        """
        return await call_async(partial(super().get_variable, node_id=node_id, name=name, include=include, exclude=exclude))

    async def get_all_variables(self, #type: ignore[override]
                                node_id: int,
                                include: set[int] = set(),
                                exclude: set[int] = set()) -> dict[str, list[int]]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.get_all_variables`
        """
        return await call_async(partial(super().get_all_variables, node_id=node_id, include=include, exclude=exclude))

    async def wait_nodes(self, #type: ignore[override]
                         node_ids: set[int] = set(),
                         number: int = -1,
                         wait_ms: int = 1000,
                         include: set[int] = set(),
                         exclude: set[int] = set()) -> dict[int, set[int]]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.wait_nodes`
        """
        r = await call_async_partial(partial(super().wait_nodes, node_ids=node_ids, number=number, include=include, exclude=exclude), wait_ms=wait_ms)
        return r or {}

    async def wait_connection(self, #type: ignore[override]
                              connection: int = -1) -> tuple[int, str]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.wait_connection`
        """
        return await call_async_tuple(partial(super().wait_connection, connection=connection))

    async def wait_disconnection(self, #type: ignore[override]
                                 connection: int = -1) -> tuple[int, str]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.wait_disconnection`
        """
        return await call_async_tuple(partial(super().wait_disconnection, connection=connection))

    async def wait_node(self, #type: ignore[override]
                        node_id: int = -1,
                        include: set[int] = set(),
                        exclude: set[int] = set()) -> tuple[int, int]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.wait_node`
        """
        return await call_async_tuple(partial(super().wait_node, node_id=node_id, include=include, exclude=exclude))

    async def wait_node_disconnection(self, #type: ignore[override]
                                      node_id: int = -1,
                                      include: set[int] = set(),
                                      exclude: set[int] = set()) -> tuple[int, int]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.wait_node_disconnection`
        """
        return await call_async_tuple(partial(super().wait_node_disconnection, node_id=node_id, include=include, exclude=exclude))

    async def connect(self, #type: ignore[override]
                      target: str,
                      wait_ms: int = 1000,
                      max_retries: int = 3,
                      **kwargs: Any) -> int:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.connect`
        """
        target = complete_target(target, **kwargs)
        failed = False
        connection = 0
        max_retries = int(max_retries)
        wait_ms = int(wait_ms)
        max_retries = max(max_retries, 0)
        while max_retries >= 0:
            connection = self._connect(target)
            if connection:
                break
            max_retries -= 1
            failed = True
            await asyncio.sleep(wait_ms * 1e-3)
        if connection:
            if failed:
                # HACK: else does not connect coppelia-sim aseba
                # if started after python
                await asyncio.sleep(1)
            self._start()
        return connection
