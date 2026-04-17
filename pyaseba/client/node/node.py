import dataclasses as dc
import re
import time
import warnings
from collections.abc import Callable, Collection, Iterable, Sequence
from functools import Placeholder, partial
from itertools import chain
from typing import Any, NotRequired, Self, TypedDict, TypeVar
import sys

from .._client_impl import Client, Description, Event, complete_target

T = TypeVar("T")

EventCallback = Callable[[T], None]


def int16(x: int) -> int:
    if x > 2**15:
        x -= 2**16
    return x


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
        self._variable_values[n] = vs

    return property(fget=getter, fset=setter, doc="TODO")


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
    """TODO"""
    preamble: str = ''
    """TODO"""
    epilog: str = ''
    """TODO"""


class EventSpecUpdate(TypedDict):
    variables: NotRequired[Sequence[str]]
    use_counter: NotRequired[bool]
    external_counter: NotRequired[str]
    window: NotRequired[int]


def _matches(name: str, include: Collection[str],
             exclude: Collection[str]) -> bool:
    return (any(re.findall(e, name) for e in include)
            and not any(re.findall(e, name) for e in exclude))


class Node:
    """
    Offers an higher-level, stateful interface to
    interact with a remote Aseba node.

    Can be used with a existent :py:class:`pyaseba.client.Client`
    or can create its own client.

    Examples:

       Using a base class, offers a slightly simpler interface to interact
       with a single remote Aseba node compared to a client that manages
       a collections remote Aseba nodes.

       >>> node = Node()
       >>> node.connect("tcp:port=33333")
       True

       >>> node.description.variables
       {"value": ...}

       For instance, setting and getting variables, does not require passing the node_id

       >>> node.set("value", [1, 2, 3])
       >>> node.get("value")
       [1, 2, 3]

       A more significant role of nodes is to provide a Pythonic
       interface to the remote Aseba node. By sub-classing it,
       we can specify:

       - Aseba variables exposed as Python properties. For example,
         assuming that the Aseba node defines variables ``a`` and ``b``:

         >>> class MyNode(Node):
         >>>     properties = ["a", "b"]
         >>>
         >>> node = MyNode()
         >>> node.connect(...)
         >>> node.a = [1, 2]
         >>> node.b
         [3, 4, 5]

         exposes them as properties, with cached values.
         Dots in the variables name are replaced by undescores
         in the properties names. For example,
         Aseba variable ``leds.top`` is linked to Python property
         ``leds_top``.


       - Aseba local events mirrored as user defined events
         that can optionally synchronize some variables.
         For example, assuming that the Aseba node defines local event ``e``:

         >>> class MyNode(Node):
         >>>     events = {"e": EventSpec(variables=["a"])
         >>>
         >>> node = MyNode()
         >>> node.connect(...)
         >>> node.wait("e")
         >>> node.set_callback("e", lambda node: ...)

         mirror it to a local event ("event_e") that we can wait
         and/or subscribe to using callbacks.

       - Aseba local functions exposed as Python methods.
         For example, assuming that the Aseba node defines
         local function ``f`` that accept two integers:

         >>> class MyNode(Node):
         >>>     functions = ["f"]
         >>>
         >>> node = MyNode()
         >>> node.connect(...)
         >>> node.call("f", 1, 2)

         or analogously

         >>> node.call_f(1, 2)

       The last two (events and functions) are realized
       by loading a executing an Aseba script
       on the remote node, which we can inspect with

       >>> print(node.script)
       onevent call_f
       call f args[1:3]
       onevent e
       emit event_e [a]
    """

    events: dict[str, EventSpec] = {}
    """
    Local events that should be mirrored
    """
    function_include: Collection[str] = (r'.*', )
    """
    Regular expressions for local functions to be exposed
    """
    function_exclude: Collection[str] = ()
    """
    Regular expressions that prevent local functions from being exposed
    """
    default_target = ""
    """Default Dashel target"""
    properties: Collection[str] = ()
    """Which variables should be exposed as Python properties"""
    functions: Collection[str] = ()
    """"Which functions should be exposed as ``call_<name>`` methods"""
    control_event = ""
    """Which local event to use to trigger the control step"""
    sync_include: Collection[str] = (r'.*', )
    """Regular expressions for variables to be included in :py:meth:`sync`"""
    sync_exclude: Collection[str] = ()
    """Regular expressions for variables to be excluded from :py:meth:`sync`"""
    script_inits: Sequence[str] = ()
    """TODO"""

    cached: bool
    """The default value of ``cached``
       used by :py:meth:`set`, :py:meth:`get`, and :py:meth:`get_all`"""

    def __init__(self, cached: bool = False) -> None:
        """
        Constructs a new instance.

        :param cached:  The default value of ``cached``
           used by :py:meth:`set`, :py:meth:`get`, and :py:meth:`get_all`
        """
        self.script_inits = list(self.script_inits)
        self.events = dict(self.events)
        self._code = ""
        self._connection = 0
        self._description: Description | None = None
        self._target = ""
        self.cached = cached
        self._client: Client | None = None
        self._node_id = -1
        self._node_id_int16 = -1
        self._buffer_name: str = ''
        self._shared_client = False
        self._code_events: dict[str, int] = {}
        self._events: dict[str, str] = {}
        self._events_variables: dict[str, list[str]] = {}
        self._counters: list[str] = []
        self._windows: list[tuple[str, int]] = []
        self._variable_values: dict[str, list[int]] = {}
        self._variable_sizes: dict[str, int] = {}
        self._event_callbacks: dict[str, EventCallback[Self]] = {}
        self._functions: dict[str, str] = {}
        self._next_variables_values: dict[str, list[int]] = {}
        self._prev_variables_values: dict[str, list[int]] = {}
        self._sync_variables: set[str] = set()

    def configure_events(self, **events: EventSpecUpdate) -> None:
        for name, kwargs in events.items():
            if name in self.events:
                self.events[name] = dc.replace(self.events[name], **kwargs)

    def __repr__(self) -> str:
        if self.connection:
            return f"<Node {self.node_id} on {self.target}>"
        return "<Node unconnected>"

    @property
    def node_id(self) -> int:
        """
        The node id
        """
        return self._node_id

    @property
    def description(self) -> Description | None:
        """
        The description
        """
        return self._description

    @property
    def target(self) -> str:
        """
        The Dashel target of the remote Aseba network
        """
        return self._target

    @property
    def connection(self) -> int:
        """
        The connected remote Aseba network
        """
        return self._connection

    @property
    def exposed_functions(self) -> list[str]:
        """
        Which Aseba functions are callable through
        :py:meth:`call`.
        """
        return list(self._functions)

    @property
    def mirrored_events(self) -> list[str]:
        """
        Which Aseba local events are await-able through
        :py:meth:`wait`.
        """
        return list(self._events.values())

    @property
    def sync_variables(self) -> set[str]:
        """
        Which Aseba variables are included in :py:meth:`synch`.
        """
        return self._sync_variables

    def _init(self) -> None:
        assert (self._client)
        description = self._client.get_description(self._node_id,
                                                   include={self.connection})
        assert (description)
        for name, (index, size) in description.variables.items():
            if index == 2:
                self._buffer_name = name
                break
        assert self._buffer_name
        self._client.add_event_callback(callback=self._event_cb)
        self._variable_values = {k: [] for k in description.variables}
        self._variable_sizes = {
            k: vs[1]
            for k, vs in description.variables.items()
        }
        self._sync_variables = {
            k
            for k in description.variables
            if _matches(k, self.sync_include, self.sync_exclude)
        }
        for name, spec in self.events.items():
            self._add_event(name, spec)
        for name, (_, vs) in description.functions.items():
            match = _matches(name, self.function_include,
                             self.function_exclude)
            if match:
                self._add_function(name, [v[1] for v in vs])
        temp_defs = ("var temp", )
        couter_defs = (f"var {v}" for v in self._counters)
        window_defs = (f"var {v}[{s}]" for v, s in self._windows)
        couter_inits = (f"{v}=0" for v in self._counters)
        window_inits = (f"call math.fill({v}, 0)" for v, s in self._windows)
        parts = chain(temp_defs, couter_defs, window_defs, couter_inits,
                      window_inits, self.script_inits, [self._code])
        self._code = "\n".join(x for x in parts if x)

    def setup(self) -> None:
        """
        Virtual method called to finalize the object when a target is successfully connected.
        The base implementation is empty.
        Override to specialize the class.
        """
        pass

    def connect(self,
                client: Client | None = None,
                target: str = "",
                wait_ms: int = 5000,
                max_retries: int = 3,
                node_id: int = -1,
                **kwargs: Any) -> bool:
        """
        Connect to a remote Aseba node through
        :py:meth:`pyaseba.client.Client.connect`

        :param client:       The client. If not provided,
                             it will instantiate a new client.
        :param target:       A valid `Dashel target <https://aseba-community.github.io/dashel/>`_.
        :param wait_ms:      Time to wait before retrying to connect in case of failure.
        :param max_retries:  Maximal number of time to try to connect before returning a failure.
        :param node_id:      The node identifier. Negative value match any id.
        :param **kwargs:     Parameters that are appended to ``target`` as ``"<key>=<value>"``.
                             For example, if target is ``"tcp"``, passing ``port=33333``
                             will result in a target ``"tcp:port=33333"``.
        :returns:            Whether the connection was successful.
        """
        target = complete_target(target or self.default_target, **kwargs)
        if client:
            self._shared_client = True
        self._client = client or Client()
        # TODO: complete support for clients already connected
        # to one or more targets
        if self._client.is_connected or self._client.connect(
                target=target, wait_ms=wait_ms, max_retries=max_retries):
            if node_id >= 0:
                self._client.cmd_reset(node_id)
            node_id, conn = self._client.wait_node(wait_ms=wait_ms,
                                                   node_id=node_id)
            if conn:
                self._connection = conn
                self._target = target
                self._node_id = node_id
                self._node_id_int16 = int16(node_id)
                self._init()
                self._start()
                self.update(wait_ms=wait_ms)
                self.setup()
                return True
        if not self._shared_client:
            self._client.close()
            self._client = None
        return False

    @property
    def script(self) -> str:
        """The loaded Aseba code (if any)"""
        return self._code

    def _add_function(self, name: str, argument_sizes: Iterable[int]) -> None:
        event_name = f'call_{name}'
        arguments = []
        i = 1
        for size in argument_sizes:
            arguments.append(f'{self._buffer_name}[{i}:{i + size - 1}]')
            i += size
        self._code += f"""
onevent {event_name}
if _id == {self._buffer_name}[0] then
call {name}({','.join(arguments)})
end
"""
        message_size = sum(argument_sizes)
        self._functions[name] = event_name
        self._code_events[event_name] = message_size

    def _update_variables(self, event: Event) -> None:
        if event.name not in self._events_variables:
            return
        i = 0
        for name in self._events_variables[event.name]:
            if name not in self._variable_sizes or name not in self._variable_values:
                continue
            size = self._variable_sizes[name]
            value = event.data[i:i + size]
            self._variable_values[name] = value
            self._prev_variables_values[name] = value
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
        if spec.window <= 0:
            return
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
        self._events_variables[event_name] = variables
        self._code_events[event_name] = message_size

    def _start(self) -> None:
        assert (self._client)
        try:
            self._client.load_script(node_id=self._node_id,
                                     script=self._code,
                                     events=self._code_events)
        except Exception as e:
            m = re.search(r"Error at Line: (\d*)", str(e))
            if m:
                ln = int(m.group(1))
                line = self._code.splitlines()[ln]
                print(line, file=sys.stderr)
            raise e
        self._client.cmd_run(self._node_id)
        self._description = self._client.get_description(
            self._node_id, include={self.connection})

    def _stop(self) -> None:
        assert (self._client)
        self._client.cmd_stop(self._node_id, include={self.connection})

    def _reset(self) -> None:
        assert self._client
        self._client.cmd_reset(self._node_id, include={self.connection})

    def close(self, reset: bool = False) -> None:
        """
        Closes the node and the client, if it was created by the node.

        :param      reset:  Whether to reset the remote node.
        """
        assert self._client
        if reset:
            self._reset()
        else:
            self._stop()
        time.sleep(0.1)
        if not self._shared_client:
            self._client.close()
            self._client = None
        self._connection = 0
        self._target = ''

    def wait(self, name: str, wait_ms: int = 1000) -> bool:
        """
        Waits for an mirrored local event

        :param      name:     The name of the local event
        :param      wait_ms:  The maximal time to wait.

        :returns:   Whether an event has been received before the deadline.
        """
        assert (self._client)
        event_name = f'event_{name}'
        if event_name not in self._events:
            return False
        e = self._client.get_event(self._node_id,
                                   event_name,
                                   include={self.connection},
                                   wait_ms=wait_ms)
        return e is not None

    def set_callback(self, name: str,
                     callback: EventCallback[Self] | None) -> None:
        """
        Sets a callback for when a mirrored local event is emitted

        :param      name:      The name of the local event
        :param      callback:  The callback
        """
        if callback is None:
            if name in self._event_callbacks:
                del self._event_callbacks[name]
        else:
            self._event_callbacks[name] = callback

    def get_callback(self, name: str) -> EventCallback[Self] | None:
        """
        Gets the callback associated to a mirrored local event.

        :param      name:  The name of the local event

        :returns:   The callback, if any.
        """
        return self._event_callbacks.get(name)

    def emit(self, name: str, *args: int) -> None:
        """
        Emits an event

        :param name:   The name of the (user) event
        :param *args:  The payload
        """
        assert (self._client)
        self._client.emit_event(self._node_id,
                                name,
                                args,
                                include={self.connection})

    def call(self, name: str, *args: int) -> None:
        """
        Calls a local function by emitting the
        corresponding user event.

        :param name:  The name of the local function
        :param *args:  The arguments
        """
        assert (self._client)
        if name in self._functions:
            data = [self._node_id_int16] + list(args)
            self._client.emit_event(self._node_id,
                                    self._functions[name],
                                    data,
                                    include={self.connection})

    def set(self,
            name: str,
            value: Sequence[int] | int,
            cached: bool | None = None) -> None:
        """
        Sets the value of an Aseba variable

        :param      name:    The name of the variable
        :param      value:   The value of the variable
        :param      cached:  Whether to forward the change
                             to the remote node immediately. If not,
                             users need to call :py:meth:`sync`
                             to forward changes.
        """
        assert (self._client)
        if cached is None:
            cached = self.cached
        if not isinstance(value, Sequence):
            value = [value]
        else:
            value = list(value)
        if not cached:
            self._client.set_variable(self._node_id,
                                      name,
                                      value,
                                      include={self.connection})
            # if name in self._next_variables_values:
            #     del self._next_variables_values[name]
        else:
            self._next_variables_values[name] = value
        self._variable_values[name] = value

    def sync(self) -> None:
        """
        Forwards cached variables changes to the remote node.

        Only variables selected by :py:attr:`sync_include`
        and not excluded by :py:attr:`sync_exclude` are forwarded.
        """
        assert (self._client)
        for name in self._sync_variables:
            if name in self._next_variables_values:
                value = self._next_variables_values[name]
                prev = self._prev_variables_values.get(name)
                if value != prev:
                    self.set(name, value, cached=False)
                    self._prev_variables_values[name] = value
        self._next_variables_values.clear()

    def get_all(self,
                wait_ms: int = 1000,
                cached: bool | None = None) -> dict[str, list[int]]:
        """
        Gets all variable.

        :param      wait_ms:  The maximal time to wait in milliseconds.
        :param      cached:   Whether to return cached values if present.
                              If false, it will query the remote object
                              for an updated value.
                              If not set, it will default to :py:attr:`cached`.
        """
        assert (self._client)
        if cached is None:
            cached = self.cached
        if not cached:
            self.update(wait_ms)
        return self._variable_values

    def update(self, wait_ms: int = 1000) -> None:
        """
        Refresh the cache with the current (remote) value of all variables.

        :param      wait_ms:  The maximal time to wait in milliseconds.
        """
        assert (self._client)
        vs = self._client.get_all_variables(self._node_id,
                                            wait_ms=wait_ms,
                                            include={self.connection})
        if vs:
            self._variable_values = vs
            self._prev_variables_values = dict(vs)
        else:
            warnings.warn('Could not update variables')

    def get(self,
            name: str,
            wait_ms: int = 1000,
            cached: bool | None = None) -> int | list[int] | None:
        """
        Gets the value of an Aseba variable

        :param      name:     The variable name
        :param      wait_ms:  The maximal time to wait in milliseconds.
        :param      cached:   Whether to return cached values if present.
                              If false, it will query the remote object
                              for an updated value.
                              If not set, it will default to :py:attr:`cached`.
        :type       cached:   { type_description }

        :returns:   ``None`` if no value is available. If the value has size 1,
                    it returns an integer scalar, else a list of integer.
        """
        assert (self._client)
        if cached is None:
            cached = self.cached
        if not cached or name not in self._variable_values:
            v = self._client.get_variable(self._node_id,
                                          name,
                                          wait_ms=wait_ms,
                                          include={self.connection})
            self._variable_values[name] = v
            self._prev_variables_values[name] = v
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
        """
        A virtual method to setup the controller
        The base implementation is empty.
        Override to specialize the class.

        :param      time_step:  The time step
        :param      event:      The name of the local event that
                                should trigger a control step.

        """
        pass

    def set_controller(self,
                       callback: Callable[[Self, float], None],
                       time_step: float = 0.1,
                       event: str = "") -> None:
        """
        Setup a controller

        :param      callback:   The callback called at each control step
        :param      time_step:  The control time step
        :param      event:      The event that should trigger a control step
        """
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
