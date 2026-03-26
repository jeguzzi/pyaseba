import asyncio
from collections.abc import Callable
from typing import Awaitable

from .network_async import NetworkAsync
from .node import Node, EventCallback
from .pyaseba import Event

EventCallback = Callable[['NodeAsync'], Awaitable[None]]
MaybeAsyncEventCallback = EventCallback | Callable[['NodeAsync'],
                                                   Awaitable[None]]


class NodeAsync(Node):

    _network: NetworkAsync

    async def connect(  # type: ignore[override]
            self,
            network: NetworkAsync | None = None,
            target: str = "",
            wait_ms: int = 1000,
            max_retries: int = 3) -> bool:
        if network:
            self._shared_network = True
        self._network = network or NetworkAsync()
        if self._network.is_connected or await self._network.connect(
                target or self._target, wait_ms=wait_ms,
                max_retries=max_retries):
            node = await self._network.wait_node_connection()
            if node is not None:
                self._node_id = node
                self._init()
                self._start()
                return True
        if not self._shared_network and not self._network.is_connected:
            self._network.close()
            self._network = None
        return False

    async def close(  # type: ignore[override]
            self, reset: bool = False) -> None:
        self._stop()
        if reset:
            self._network.reset(self._node_id)
        if not self._shared_network:
            self._network.close()
            self._network = None
        self._node_id = -1
        await asyncio.sleep(0.1)

    async def get(  # type: ignore[override]
            self,
            name: str,
            cached: bool | None = None) -> int | list[int] | None:
        if cached is None:
            cached = self.cached
        if not cached or name not in self._variable_values:
            self._variable_values[name] = await self._network.get_variable(
                self._node_id, name)
        value = self._variable_values[name]
        if value is not None and len(value) == 1:
            return value[0]
        return value

    def set_callback(  # type: ignore[override]
            self, name: str, callback: MaybeAsyncEventCallback) -> None:
        scb: EventCallback
        if asyncio.iscoroutinefunction(callback):
            loop = asyncio.get_running_loop()

            def scb(node: NodeAsync) -> None:
                loop.call_soon_threadsafe(loop.create_task, callback(node))
        else:
            scb = callback
        self._event_callbacks[name] = scb
