# import time
from collections.abc import Callable, Collection, Iterable, Sequence
from functools import Placeholder, partial
from typing import Any, NamedTuple, Self, TypeVar

from ._client_impl import Client, Event

T = TypeVar("T")

EventCallback = Callable[[T], None]


def make_property(n: str) -> property:

    def getter(self: Node) -> int | list[int]:
        v = self._variable_values.get(n)
        assert (v is not None)
        if len(v) == 1:
            return v[0]
        return v

    def setter(self: Node, vs: int | list[int]) -> None:
        if not isinstance(vs, Sequence):
            vs = [vs]
        self._next_variables_values[n] = vs

    return property(fget=getter, fset=setter, doc="TODO")


class EventSpec(NamedTuple):
    variables: Sequence[str] = ()
    use_counter: bool = False
    external_counter: str = ''


class Node:

    events: dict[str, EventSpec] = {}
    function_prefixes: Collection[str] = ()
    function_exclude: Collection[str] = ()
    target = ""
    properties: Collection[str] = ()
    functions: Collection[str] = ()
    control_event = ""

    def __init__(self, cached: bool = False) -> None:
        self._code = ""
        self.cached = cached
        self._client: Client | None = None
        self._node_id = -1
        self._shared_client = False
        self._code_events: list[tuple[str, int]] = []
        self._events: dict[str, str] = {}
        self._events_variables: dict[str, list[str]] = {}
        self._counters: list[str] = []
        self._variable_values: dict[str, list[int]] = {}
        self._variable_sizes: dict[str, int] = {}
        self._event_callbacks: dict[str, EventCallback[Self]] = {}
        self._functions: dict[str, str] = {}
        self._next_variables_values: dict[str, list[int]] = {}

    def _init(self) -> None:
        assert (self._client)
        description = self._client.get_description(self._node_id)
        assert (description)
        self._client.add_event_callback(callback=self._event_cb)
        self._variable_values = {k: [] for k, _ in description.variables}
        self._variable_sizes = dict(description.variables)
        for name, spec in self.events.items():
            self._add_event(name, spec)
        for name, _, vs in description.functions:
            prefix = name.split('.')[0]
            if prefix in self.function_prefixes and not any(
                    e in name for e in self.function_exclude):
                self._add_function(name, [v[1] for v in vs])
        var_def = "\n".join(f"var {v}" for v in self._counters)
        var_init = "\n".join(f"{v}=0" for v in self._counters)
        self._code = f"""
{var_def}
{var_init}
{self._code}
"""
        # print(self._code)

    def setup(self) -> None:
        pass

    def connect(self,
                client: Client | None = None,
                target: str = "",
                wait_ms: int = 5000,
                max_retries: int = 3,
                node_id: int = -1) -> bool:
        if client:
            self._shared_client = True
        self._client = client or Client()
        if self._client.is_connected or self._client.connect(
                target or self.target, wait_ms=wait_ms,
                max_retries=max_retries):
            node = self._client.wait_node_connection(wait_ms=wait_ms, node=node_id)
            if node is not None:
                self._node_id = node
                self._init()
                self._start()
                # time.sleep(1)
                self.update(wait_ms=wait_ms)
                self.setup()
                return True
        if not self._shared_client:
            self._client.close()
            self._client = None
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

    def _update_variables(self, event: Event) -> None:
        if event.name not in self._events_variables:
            return
        i = 0
        for name in self._events_variables[event.name]:
            if name not in self._variable_sizes or name not in self._variable_values:
                continue
            size = self._variable_sizes[name]
            self._variable_values[name] = event.data[i:i + size]
            i += size

    def _call_callbacks(self, event: Event) -> None:
        if event.name in self._events:
            name = self._events[event.name]
            if name in self._event_callbacks:
                self._event_callbacks[name](self)  # type: ignore[arg-type]

    def _extra_event_cb(self, event: Event) -> None:
        pass

    def _event_cb(self, event: Event) -> None:
        if event.source != self._node_id:
            return
        self._update_variables(event)
        self._extra_event_cb(event)
        self._call_callbacks(event)

    def _add_event(self, name: str, spec: EventSpec) -> None:
        event_name = f'event_{name}'
        counter = ''
        variables = list(spec.variables)
        message_size = sum(self._variable_sizes[v] for v in variables)
        all_variables = variables
        if spec.use_counter:
            message_size += 1
            if spec.external_counter:
                all_variables.append(spec.external_counter)
            else:
                counter = event_name
                all_variables.append(counter)
                self._counters.append(counter)
        if variables:
            event_variables = f"[{','.join(all_variables)}]"
        else:
            event_variables = ""
        self._code += f"""
onevent {name}
emit {event_name} {event_variables}
"""
        if counter:
            self._code += f"{counter}+=1\n"
        self._events[event_name] = name
        self._events_variables[event_name] = variables
        self._code_events.append((event_name, message_size))

    def _start(self) -> None:
        assert (self._client)
        self._client.load_script(node=self._node_id,
                                  script=self._code,
                                  events=self._code_events)
        self._client.run(self._node_id)

    def _stop(self) -> None:
        assert (self._client)
        self._client.stop(self._node_id)

    def close(self, reset: bool = False) -> None:
        assert (self._client)
        self._stop()
        if reset:
            self._client.reset(self._node_id)
        if not self._shared_client:
            self._client.close()
            self._client = None
        self._node_id = -1

    def set_callback(self, name: str, callback: EventCallback[Self] | None) -> None:
        if callback is None:
            if name in self._event_callbacks:
                del self._event_callbacks[name]
        else:
            self._event_callbacks[name] = callback

    def get_callback(self, name: str) -> EventCallback[Self] | None:
        return self._event_callbacks.get(name)

    def emit(self, name: str, *args: int) -> None:
        assert (self._client)
        self._client.emit_event(self._node_id, name, args)

    def call(self, name: str, *args: int) -> None:
        assert (self._client)
        # print('call', self._node_id, self._functions[name], args)
        if name in self._functions:
            self._client.emit_event(self._node_id, self._functions[name],
                                     args)
    def set(self,
            name: str,
            value: Sequence[int] | int,
            cached: bool | None = None) -> None:
        assert (self._client)
        if cached is None:
            cached = self.cached
        if not isinstance(value, Sequence):
            value = [value]
        else:
            value = list(value)
        if not cached:
            # print('set_variable', self._node_id, name, value)
            self._client.set_variable(self._node_id, name, value)
            # if name in self._next_variables_values:
            #     del self._next_variables_values[name]
        else:
            self._next_variables_values[name] = value

    def sync(self) -> None:
        assert (self._client)
        for name, value in self._next_variables_values.items():
            self.set(name, value, cached=False)
        self._next_variables_values.clear()

    def update(self, wait_ms: int = 1000) -> None:
        assert (self._client)
        vs = self._client.get_all_variables(self._node_id, wait_ms)
        if vs is not None:
            self._variable_values = vs
        else:
            print('Could not update variables')

    def get(self,
            name: str,
            wait_ms: int = 1000,
            cached: bool | None = None) -> int | list[int] | None:
        assert (self._client)
        if cached is None:
            cached = self.cached
        if not cached or name not in self._variable_values:
            v = self._client.get_variable(self._node_id,
                                           name,
                                           wait_ms=wait_ms)
            self._variable_values[name] = v or []
        value = self._variable_values[name]
        if len(value) == 1:
            return value[0]
        return value

    def __init_subclass__(cls, **kwargs: Any) -> None:
        super().__init_subclass__(**kwargs)
        for v in cls.properties:
            n = v.replace('.', '_')
            setattr(cls, n, make_property(v))
        for f in cls.functions:
            n = f"call_{f.replace('.', '_')}"
            setattr(
                cls,
                n,
                partial(
                    cls.call,
                    Placeholder,  # type: ignore[arg-type]
                    f))
        for e in cls.events:
            n = f"on_{e.replace('.', '_')}"
            setattr(
                cls,
                n,
                property(
                    fget=partial(
                        cls.get_callback,
                        Placeholder,  # type: ignore[type-var]
                        e),
                    fset=partial(
                        cls.set_callback,
                        Placeholder,  # type: ignore[type-var]
                        e)))

    def set_control_period(self, time_step: float, event: str = "") -> None:
        pass

    def set_controller(self,
                       callback: Callable[[Self, float], None],
                       time_step: float = 0.1,
                       event: str = "") -> None:
        event = event or self.control_event
        if not event:
            raise RuntimeError("No event set as control trigger")
        if time_step > 0:
            def cb(node: Self) -> None:
                callback(node, time_step)
                node.sync()
            self.set_control_period(time_step, event=event)
            self.set_callback(event, cb)
        else:
            self.set_control_period(0, event=event)
            self.set_callback(event, None)
