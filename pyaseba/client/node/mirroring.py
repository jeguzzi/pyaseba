import dataclasses as dc
from collections.abc import Collection, Iterable, Sequence
from itertools import chain
from typing import NotRequired, TypedDict

from .._client_impl import Description, Event
from .utils import matches


@dc.dataclass
class EventSpec:
    """
    Describes how :py:class:`Node` should mirror local events.
    """
    variables: Sequence[str] = ()
    """Which variables to synchronize when the event is emitted"""
    use_counter: bool = False
    """Whether to append a counter to the payload"""
    external_counter: str = ''
    """If not empty, it select a variable to use as a counter"""
    window: int = 1
    """The length of the window over which to average variables. If less or equal 0,
    it will ignore the event"""
    preamble: str = ''
    """Aseba code to prepend to the event code"""
    epilog: str = ''
    """Aseba code to append to the event code"""


class EventSpecUpdate(TypedDict):
    variables: NotRequired[Sequence[str]]
    use_counter: NotRequired[bool]
    external_counter: NotRequired[str]
    window: NotRequired[int]


@dc.dataclass
class MirroringConfig:
    events: dict[str, EventSpec] = dc.field(default_factory=dict)
    """
    Local events that should be mirrored
    """
    function_include: list[str] = dc.field(default_factory=list)
    """
    Regular expressions for local functions to be exposed
    """
    function_exclude: list[str] = dc.field(default_factory=list)
    """
    Regular expressions that prevent local functions from being exposed
    """
    script_inits: list[str] = dc.field(default_factory=list)
    """Aseba code to add to the preamble (should not define variables)"""

    def configure_events(self, **events: EventSpecUpdate) -> None:
        for name, kwargs in events.items():
            if name in self.events:
                self.events[name] = dc.replace(self.events[name], **kwargs)


class Mirroring:

    def __init__(self, description: Description, config: MirroringConfig) -> None:
        self._description = description
        self._config = config
        self._code_events: dict[str, int] = {}
        self._events: dict[str, str] = {}
        self._user_events: dict[str, str] = {}
        # {event name: [(variable name, variable size), ...]}
        self._events_variables: dict[str, list[tuple[str, int]]] = {}
        self._counters: list[str] = []
        self._windows: list[tuple[str, int]] = []
        self._functions: dict[str, str] = {}
        self._code = ""
        self._buffer_name: str = ''
        self._id_name: str = ''
        for name, (index, size) in description.variables.items():
            if index == 0:
                self._id_name = name
            if index == 2:
                self._buffer_name = name
        for name, spec in config.events.items():
            self._add_event(name, spec)
        for name, (_, vs) in description.functions.items():
            match = matches(name, config.function_include,
                            config.function_exclude)
            if match:
                self._add_function(name, [v[1] for v in vs])
        temp_defs = ("var temp", )
        couter_defs = (f"var {v}" for v in self._counters)
        window_defs = (f"var {v}[{s}]" for v, s in self._windows)
        couter_inits = (f"{v}=0" for v in self._counters)
        window_inits = (f"call math.fill({v}, 0)" for v, s in self._windows)
        parts = chain(temp_defs, couter_defs, window_defs, couter_inits,
                      window_inits, config.script_inits, [self._code])
        self._code = "\n".join(x for x in parts if x)

    def _add_function(self, name: str, argument_sizes: Iterable[int]) -> None:
        event_name = f'call_{name}'
        arguments = []
        i = 1
        for size in argument_sizes:
            arguments.append(f'{self._buffer_name}[{i}:{i + size - 1}]')
            i += size
        self._code += f"""
onevent {event_name}
if {self._id_name} == {self._buffer_name}[0] then
call {name}({','.join(arguments)})
end
"""
        message_size = sum(argument_sizes)
        self._functions[name] = event_name
        self._code_events[event_name] = message_size

    def _add_event(self, name: str, spec: EventSpec) -> None:
        if spec.window <= 0:
            return
        event_name = f'event_{name}'
        counter = ''
        vs = self._description.variables
        variables = [(n, vs[n][1]) for n in spec.variables]
        message_size = sum(s for _, s in variables)
        all_variables = [n for n, s in variables]
        if spec.use_counter:
            message_size += 1
            if spec.external_counter:
                all_variables.append(spec.external_counter)
            else:
                counter = event_name
                all_variables.append(counter)
                self._counters.append(counter)
        counter_code = f"{counter}+=1" if counter else ""
        if variables:
            event_variables = f"[{','.join(all_variables)}]"
        else:
            event_variables = ""
        if spec.window == 1 or not event_variables:
            code = f"""
onevent {name}
{spec.preamble}
emit {event_name} {event_variables}
{counter_code}
{spec.epilog}
"""
        else:
            self._windows.append((f"{name}_payload", message_size))
            self._counters.append(f"{name}_window")
            code = f"""
onevent {name}
{spec.preamble}
call math.add({name}_payload, {event_variables}, {name}_payload)
{counter_code}
{name}_window++
if {name}_window == {spec.window} then
  for temp in 0:{message_size - 1} do
      {name}_payload[temp] /= {spec.window}
  end
  {name}_window = 0
  emit {event_name} {name}_payload
  call math.fill({name}_payload, 0)
end
{spec.epilog}
"""
        code = "\n".join(s for s in code.splitlines() if s.strip())
        if code:
            self._code += "\n\n" + code
        self._events[event_name] = name
        self._user_events[name] = event_name
        self._events_variables[event_name] = variables
        self._code_events[event_name] = message_size

    def read_variables(self, event: Event) -> dict[str, list[int]]:
        rs: dict[str, list[int]] = {}
        i = 0
        for name, size in self._events_variables.get(event.name, ()):
            rs[name] = event.data[i:i + size]
            i += size
        return rs

    def get_event_name(self, name: str) -> str:
        return self._user_events.get(name, '')

    def get_function_name(self, name: str) -> str:
        return self._functions.get(name, '')

    @property
    def code(self) -> str:
        return self._code
