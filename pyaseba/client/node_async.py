import asyncio
from collections.abc import Callable
from typing import Awaitable, Self, TypeVar, cast

from .client_async import ClientAsync
from .node import Node

T = TypeVar("T")

EventCallback = Callable[[T], None]
MaybeAsyncEventCallback = EventCallback[T] | Callable[[T], Awaitable[None]]


class NodeAsync(Node):
    """
    Asynchronous alternative to :py:class:`pyaseba.client.Node`
    based on :py:class:`pyaseba.client.ClientAsync`.

    Replaces methods that blocks (and typically takes ``wait_ms: int`` as argument)
    with coroutines, like for example

    >>> node = Node()
    >>> await node.connect("tcp:port=33333")
    True

    >>> node.set("value", [1, 2, 3])
    >>> await node.get("value")
    [1, 2, 3]

    >>> await node.wait("e")
    True

    >>> await node.close()

    The reset of the interface is identical to :py:class:`pyaseba.client.Node`.
    """

    _client: ClientAsync | None

    async def connect(  # type: ignore[override]
            self,
            client: ClientAsync | None = None,
            target: str = "",
            wait_ms: int = 1000,
            max_retries: int = 3,
            node_id: int = -1) -> bool:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Node.connect`
        """
        if client:
            self._shared_client = True
        self._client = client or ClientAsync()
        if self._client.is_connected or await self._client.connect(
                target or self.target, wait_ms=wait_ms,
                max_retries=max_retries):
            node_id, conn = await self._client.wait_node(node_id=node_id)
            if conn:
                self._node_id = node_id
                self._init()
                self._start()
                # TODO: makes it crash (sometimes) when multiple thymios are connected
                # await self.update()
                self.setup()
                return True
        if not self._shared_client and not self._client.is_connected:
            self._client.close()
            self._client = None
        return False

    async def update(self  # type: ignore[override]
                     ) -> None:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Node.update`
        """
        assert (self._client)
        self._variable_values = await self._client.get_all_variables(
            self._node_id)

    async def close(  # type: ignore[override]
            self, reset: bool = False) -> None:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Node.close`
        """
        self._stop()
        if reset and self._client:
            self._client.cmd_reset(self._node_id)
        if not self._shared_client and self._client:
            self._client.close()
            self._client = None
        self._node_id = -1
        await asyncio.sleep(0.1)

    async def wait(  # type: ignore[override]
            self, name: str) -> bool:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Node.wait`
        """
        assert (self._client)
        event_name = f'event_{name}'
        if event_name not in self._events:
            return False
        e = await self._client.get_event(self._node_id, event_name)
        return e is not None

    async def get(  # type: ignore[override]
            self,
            name: str,
            cached: bool | None = None) -> int | list[int] | None:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Node.get`
        """
        assert (self._client)
        if cached is None:
            cached = self.cached
        if not cached or name not in self._variable_values:
            self._variable_values[name] = await self._client.get_variable(
                self._node_id, name)
        value = self._variable_values[name]
        if value is not None and len(value) == 1:
            return value[0]
        return value

    def set_callback(self, name: str,
                     callback: MaybeAsyncEventCallback[Self] | None) -> None:
        """
        Version of :py:meth:`pyaseba.client.Node.set_callback` that accept
        coroutines as callbacks too.
        """
        scb: EventCallback[Self] | None
        if callback is not None:
            if asyncio.iscoroutinefunction(callback):
                loop = asyncio.get_running_loop()

                def scb(node: Self) -> None:
                    loop.call_soon_threadsafe(loop.create_task, callback(node))
            else:
                scb = cast('EventCallback[Self]', callback)
        else:
            scb = None
        super().set_callback(name, scb)
