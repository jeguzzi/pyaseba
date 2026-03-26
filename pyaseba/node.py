import time
from collections.abc import Callable, Collection, Iterable, Sequence

from .pyaseba import Event, Network

EventCallback = Callable[['Node'], None]


class Node:

    events: dict[str, Sequence[str]] = {}
    function_prefixes: Collection[str] = ()
    function_exclude: Collection[str] = ()
    target = ""

    def __init__(self, cached: bool = False) -> None:
        self._code = ""
        self.cached = cached
        self._network: Network | None = None
        self._node_id = -1
        self._shared_network = False
        self._code_events: list[tuple[str, int]] = []
        self._events: dict[str, str] = {}
        self._events_variables: dict[str, list[str]] = {}
        self._variable_values: dict[str, list[int] | None] = {}
        self._variable_sizes: dict[str, int] = {}
        self._event_callbacks: dict[str, EventCallback] = {}
        self._functions: dict[str, str] = {}
        self._next_variables_values: dict[str, list[int]] = {}

    def _init(self) -> None:
        description = self._network.get_description(self._node_id)
        self._network.add_event_callback(callback=self._event_cb)
        self._variable_values = {k: None for k in description.variables}
        self._variable_sizes = dict(description.variables)
        for name, variables in self.events.items():
            self._add_event(name, variables)
        for name, _, vs in description.functions:
            prefix = name.split('.')[0]
            if prefix in self.function_prefixes and not any(
                    e in name for e in self.function_exclude):
                self._add_function(name, [v[1] for v in vs])

    def connect(self,
                network: Network | None = None,
                target: str = "",
                wait_ms: int = 5000,
                max_retries: int = 3) -> bool:
        if network:
            self._shared_network = True
        self._network = network or Network()
        if self._network.is_connected or self._network.connect(
                target or self._target, wait_ms=wait_ms,
                max_retries=max_retries):
            node = self._network.wait_node_connection(wait_ms=wait_ms)
            if node is not None:
                self._node_id = node
                self._init()
                self._start()
                return True
        if not self._shared_network:
            self._network.close()
            self._network = None
        return False

    def _add_function(self, name: str, argument_sizes: Iterable[int]) -> None:
        event_name = f'call_{name}'
        arguments = []
        i = 0
        for size in argument_sizes:
            arguments.append(f'event.args[{i}:{i + size - 1}]')
            i += size
        self._code += f"""
onevent {event_name}
call {name}({','.join(arguments)})
"""
        message_size = sum(argument_sizes)
        self._functions[name] = event_name
        self._code_events.append((event_name, message_size))

    def _event_cb(self, event: Event) -> None:
        if event.source != self._node_id:
            return
        i = 0
        for name in self._events_variables[event.name]:
            size = self._variable_sizes[name]
            self._variable_values[name] = event.data[i:i + size]
            i += size
        if event.name in self._events:
            name = self._events[event.name]
            if name in self._event_callbacks:
                self._event_callbacks[name](self)

    def _add_event(self, name: str, variables: Collection[str]) -> None:
        event_name = f'event_{name}'
        if variables:
            event_variables = f"[{','.join(variables)}]"
        else:
            event_variables = ""
        self._code += f"""
onevent {name}
emit {event_name} {event_variables}
"""
        self._events[event_name] = name
        self._events_variables[event_name] = list(variables)
        message_size = sum(self._variable_sizes[v] for v in variables)
        self._code_events.append((event_name, message_size))

    def _start(self) -> None:
        self._network.load_script(node=self._node_id,
                                  script=self._code,
                                  events=self._code_events)
        self._network.run(self._node_id)

    def _stop(self) -> None:
        self._network.stop(self._node_id)

    def close(self, reset: bool = False) -> None:
        self._stop()
        if reset:
            self._network.reset(self._node_id)
        if not self._shared_network:
            self._network.close()
            self._network = None
        self._node_id = -1
        time.sleep(0.1)

    def set_callback(self, name: str, callback: EventCallback) -> None:
        self._event_callbacks[name] = callback

    def emit(self, name: str, *args: int) -> None:
        self._network.emit_event(self._node_id, name, args)

    def call(self, name: str, *args: int) -> None:
        if name in self._functions:
            self._network.emit_event(self._node_id, self._functions[name],
                                     args)

    def set(self,
            name: str,
            value: Sequence[int] | int,
            cached: bool | None = None) -> None:
        if cached is None:
            cached = self.cached
        if not isinstance(value, Sequence):
            value = [value]
        if not cached:
            self._network.set_variable(self._node_id, name, value)
            if name in self._next_variables_values:
                del self._next_variables_values[name]
        else:
            self._next_variables_values[name] = value

    def sync(self) -> None:
        for name, value in self._next_variables_values.items():
            self.set(name, value, cached=False)

    def get(self,
            name: str,
            wait_ms: int = 1000,
            cached: bool | None = None) -> int | list[int] | None:
        if cached is None:
            cached = self.cached
        if not cached or name not in self._variable_values:
            self._variable_values[name] = self._network.get_variable(
                self._node_id, name, wait_ms=wait_ms)
        value = self._variable_values[name]
        if value is not None and len(value) == 1:
            return value[0]
        return value
