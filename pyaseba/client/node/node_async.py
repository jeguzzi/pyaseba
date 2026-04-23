import asyncio
from typing import Self, TypeVar

from ..client_async import ClientAsync, MaybeAsyncCallback, wrap_callback
from .node import Node, int16

T = TypeVar("T")


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
            if node_id >= 0:
                self._client.cmd_reset(node_id)
            node_id, conn = await self._client.wait_node(node_id=node_id,
                                                         wait_ms=wait_ms)
            if conn:
                self._connection = conn
                self._target = target
                self._node_id = node_id
                self._node_id_int16 = int16(node_id)
                self._init()
                self._start()
                await self.update(wait_ms=wait_ms)
                self.setup()
                return True
        if not self._shared_client and not self._client.is_connected:
            self._client.close()
            self._client = None
        return False

    async def get_all(  # type: ignore[override]
            self,
            wait_ms: int = 1000,
            cached: bool | None = None) -> dict[str, list[int]]:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Node.get_all`
        """
        assert (self._client)
        if cached is None:
            cached = self.cached
        if not cached:
            await self.update(wait_ms)
        return self._variable_values

    async def update(  # type: ignore[override]
            self, wait_ms: int = 1000) -> None:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Node.update`
        """
        assert self._client
        vs = await self._client.get_all_variables(self._node_id,
                                                  wait_ms=wait_ms)
        if vs:
            self._variable_values = vs

    async def close(  # type: ignore[override]
            self, reset: bool = False) -> None:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Node.close`
        """
        if reset:
            self._reset()
        else:
            self._stop()
        await asyncio.sleep(0.1)
        if not self._shared_client and self._client:
            self._client.close()
            self._client = None
        self._node_id = -1

    async def wait(  # type: ignore[override]
            self, name: str, wait_ms: int = 0) -> bool:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Node.wait`
        """
        assert (self._client)
        event_name = f'event_{name}'
        if event_name not in self._events:
            return False
        e = await self._client.get_event(self._node_id,
                                         event_name,
                                         wait_ms=wait_ms)
        return e is not None

    async def get(  # type: ignore[override]
            self,
            name: str,
            wait_ms: int = 0,
            cached: bool | None = None) -> int | list[int] | None:
        """
        Asynchronous version of :py:meth:`pyaseba.client.Node.get`
        """
        assert (self._client)
        if cached is None:
            cached = self.cached
        if not cached or name not in self._variable_values:
            self._variable_values[name] = await self._client.get_variable(
                self._node_id, name, wait_ms=wait_ms)
        value = self._variable_values[name]
        if value is not None and len(value) == 1:
            return value[0]
        return value

    def set_callback(self, name: str,
                     callback: MaybeAsyncCallback[Self] | None) -> None:
        """
        Version of :py:meth:`pyaseba.client.Node.set_callback` that accept
        coroutines as callbacks too.
        """
        scb = wrap_callback(callback) if callback else None
        super().set_callback(name, scb)
