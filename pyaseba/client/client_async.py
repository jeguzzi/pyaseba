import asyncio
from collections.abc import Callable
from functools import partial
from typing import Any, Awaitable, Protocol, Unpack, cast

from . import msgs
from ._client_impl import Client, Description, Event, Message, complete_target

type Callback[*Ts] = Callable[[*Ts], None]
type AsyncCallback[*Ts] = Callable[[*Ts], Awaitable[None]]
type MaybeAsyncCallback[*Ts] = Callback[*Ts] | AsyncCallback[*Ts]
type DescriptionFragment = msgs.Description | msgs.LocalEventDescription | msgs.NamedVariableDescription | msgs.NativeFunctionDescription


def wrap_callback[*Ts](callback: MaybeAsyncCallback[*Ts] | None) -> Callback[*Ts]:
    """
    If callback is a coroutine, it returns a new callback
    that schedules the original callback,
    else it returns the orginal callback.

    :param callback: The callback to be possibly wrapped
    :returns: The wrapped callback.
    """
    if asyncio.iscoroutinefunction(callback):
        loop = asyncio.get_running_loop()
        def cb(*args: *Ts) -> None:
            loop.call_soon_threadsafe(loop.create_task, callback(*args))
        return cb
    return cast('Callback[*Ts]', callback)


class WaitCallable[T, R](Protocol):

    def __call__(self, wait_ms: int = 0, callback: Callable[[T], None] | None = None) -> R:
        ...

class WaitCallableTuple[*Ts](Protocol):

    def __call__(self, wait_ms: int = 0, callback: Callable[[*Ts], None] | None = None) -> tuple[*Ts]:
        ...

class WaitCallablePartial[T](Protocol):

    def __call__(self, wait_ms: int = 0, callback: Callable[[T, bool], None] | None = None) -> T:
        ...

async def call_async[T, R](f: WaitCallable[T, R], wait_ms: int = 0) -> R:
    loop = asyncio.get_running_loop()
    future: asyncio.Future[T] = loop.create_future()

    def cb(arg: T) -> None:
        if not future.cancelled():
            loop.call_soon_threadsafe(future.set_result, arg)
    r = f(wait_ms=0, callback=cb)
    if wait_ms > 0:
        done, _ = await asyncio.wait((future, ), timeout=wait_ms * 1e-3)
        return cast(R, future.result()) if done else r
    return cast(R, await future)

async def call_async_tuple[*Ts](f: WaitCallableTuple[*Ts], wait_ms: int = 0) -> tuple[*Ts]:
    loop = asyncio.get_running_loop()
    future: asyncio.Future[tuple[*Ts]] = loop.create_future()

    def cb(*arg: Unpack[Ts]) -> None:
        if not future.cancelled():
            loop.call_soon_threadsafe(future.set_result, arg)

    r = f(wait_ms=0, callback=cb)
    if wait_ms > 0:
        done, _ = await asyncio.wait((future, ), timeout=wait_ms * 1e-3)
        return future.result() if done else r
    return await future


async def call_async_partial[T](f: WaitCallablePartial[T], wait_ms: int) -> T:
    loop = asyncio.get_running_loop()
    future: asyncio.Future[T] = loop.create_future()

    p: T

    def cb(arg: T, complete: bool) -> None:
        nonlocal p
        if complete and not future.cancelled():
            loop.call_soon_threadsafe(future.set_result, arg)
        p = arg

    p = f(wait_ms=0, callback=cb)
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
        f = partial(super().scan, number=number)
        return await call_async_partial(f, wait_ms=wait_ms)

    async def query_description(self, #type: ignore[override]
                                node_id: int,
                                wait_ms: int = 0,
                                include: set[int] = set(),
                                exclude: set[int] = set()
                                ) -> Description | None:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.query_description`
        """
        f = partial(super().query_description, node_id=node_id, include=include, exclude=exclude)
        return await call_async(f, wait_ms=wait_ms)

    async def query_description_fragment(self, #type: ignore[override]
                                node_id: int,
                                fragment: int,
                                wait_ms: int = 0,
                                include: set[int] = set(),
                                exclude: set[int] = set()
                                ) -> DescriptionFragment | None:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.query_description_fragment`
        """
        f = partial(super().query_description_fragment, node_id=node_id, fragment=fragment, include=include, exclude=exclude)
        return await call_async(f, wait_ms=wait_ms)

    async def get_message(self, #type: ignore[override]
                          node_id: int = -1,
                          types: set[int] = set(),
                          wait_ms: int = 0,
                          include: set[int] = set(),
                          exclude: set[int] = set(),
                          pause: bool = False) -> tuple[Message | None, int]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.get_message`
        """
        f = partial(super().get_message, node_id=node_id, types=types, include=include, exclude=exclude, pause=pause)
        return await call_async_tuple(f, wait_ms=wait_ms)

    async def get_event(self, #type: ignore[override]
                        node_id: int,
                        name: str,
                        wait_ms: int = 0,
                        include: set[int] = set(),
                        exclude: set[int] = set()) -> Event | None:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.get_event`
        """
        f = partial(super().get_event, node_id=node_id, name=name, include=include, exclude=exclude)
        return await call_async(f, wait_ms=wait_ms)


    async def get_variable(self, #type: ignore[override]
                           node_id: int,
                           name: str,
                           wait_ms: int = 0,
                           include: set[int] = set(),
                           exclude: set[int] = set()) -> list[int]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.get_variable`
        """
        f = partial(super().get_variable, node_id=node_id, name=name, include=include, exclude=exclude)
        return await call_async(f, wait_ms=wait_ms)

    async def get_all_variables(self, #type: ignore[override]
                                node_id: int,
                                wait_ms: int = 0,
                                include: set[int] = set(),
                                exclude: set[int] = set()) -> dict[str, list[int]]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.get_all_variables`
        """
        f = partial(super().get_all_variables, node_id=node_id, include=include, exclude=exclude)
        return await call_async(f, wait_ms=wait_ms)

    async def get_changed_variables(self, #type: ignore[override]
                                node_id: int,
                                wait_ms: int = 0,
                                include: set[int] = set(),
                                exclude: set[int] = set()) -> list[tuple[int, list[int]]]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.get_changed_variables`
        """
        f = partial(super().get_changed_variables, node_id=node_id, include=include, exclude=exclude)
        return await call_async(f, wait_ms=wait_ms)

    async def wait_nodes(self, #type: ignore[override]
                         node_ids: set[int] = set(),
                         number: int = -1,
                         wait_ms: int = 0,
                         include: set[int] = set(),
                         exclude: set[int] = set()) -> dict[int, set[int]]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.wait_nodes`
        """
        f = partial(super().wait_nodes, node_ids=node_ids, number=number, include=include, exclude=exclude)
        return await call_async_partial(f, wait_ms=wait_ms)

    async def wait_connection(self, #type: ignore[override]
                              connection: int = 0,
                              wait_ms: int = 0) -> tuple[int, str]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.wait_connection`
        """
        f = partial(super().wait_connection, connection=connection)
        return await call_async_tuple(f, wait_ms=wait_ms)

    async def wait_disconnection(self, #type: ignore[override]
                                 connection: int = 0,
                                 wait_ms: int = 0,) -> tuple[int, str]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.wait_disconnection`
        """
        f = partial(super().wait_disconnection, connection=connection)
        return await call_async_tuple(f, wait_ms=wait_ms)

    async def wait_node(self, #type: ignore[override]
                        node_id: int = -1,
                        wait_ms: int = 0,
                        include: set[int] = set(),
                        exclude: set[int] = set()
                        ) -> tuple[int, int]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.wait_node`
        """
        f = partial(super().wait_node, node_id=node_id, include=include, exclude=exclude)
        return await call_async_tuple(f, wait_ms=wait_ms)

    async def wait_node_disconnection(self, #type: ignore[override]
                                      node_id: int = -1,
                                      wait_ms: int = 0,
                                      include: set[int] = set(),
                                      exclude: set[int] = set()) -> tuple[int, int]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Client.wait_node_disconnection`
        """
        f = partial(super().wait_node_disconnection, node_id=node_id, include=include, exclude=exclude)
        return await call_async_tuple(f, wait_ms=wait_ms)

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
