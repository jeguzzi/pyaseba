import dataclasses as dc
import logging
import re
import time
import warnings
from collections.abc import Callable, Collection, Sequence
from typing import Any, Protocol, Self, TypeVar

from .._client_impl import Client, Description, Event, complete_target
from ..targets import are_targets_compatible
from .mirroring import Mirroring, MirroringConfig
from .utils import int16, matches

T = TypeVar("T")

EventCallback = Callable[[T], None]


class ExposedFunctionMethod(Protocol):

    def __call__(_, self: 'Node', *args: int) -> None:
        ...


def make_property_with_variable(name: str) -> property:

    def getter(self: Node) -> int | list[int]:
        v = self._variable_values.get(name)
        assert (v is not None)
        if len(v) == 1:
            return v[0]
        return v

    def setter(self: Node, vs: int | list[int]) -> None:
        if not isinstance(vs, Sequence):
            vs = [vs]
        self._next_variables_values[name] = vs
        self._variable_values[name] = vs

    return property(fget=getter, fset=setter, doc=f"Aseba variable {name}")


def make_method_with_function(name: str) -> ExposedFunctionMethod:

    def f(self: Node, *args: int) -> None:
        self.call(name, *args)

    f.__doc__ = f"""
    Calls native function {name}

    :param args: The argument passed to the function.
"""

    return f


def make_property_with_event(name: str) -> property:

    def getter(self: Node) -> EventCallback[Node] | None:
        return self.get_callback(name)

    def setter(self: Node, callback: EventCallback[Node] | None) -> None:
        self.set_callback(name, callback)

    return property(fget=getter,
                    fset=setter,
                    doc=f"Callback for Aseba event {name}")


class Node:
    """
    A class to interact with a single remote Aseba node that
    1) has a simpler interface compared to :py:class:`pyaseba.client.Client`,
    2) uses a cache for Aseba variables,
    3) exposes Aseba native functions and events,
    4) access Aseba variables, events and functions by name through generic methods
    or through specific attributes.

    Under the hood, it uses :py:class:`pyaseba.client.Client`
    to interact with a single Aseba node, either reusing
    an existent client or instantiating one when required.

    Compared to :py:class:`pyaseba.client.Client`,
    which manages a collections remote Aseba nodes,
    it simplify the interface by keeping track of the
    network and id of a single remote node.

    Nodes can be configured to execute an Aseba script
    that exposes remote local events and native functions and keeps
    variables up-to-date. When local events are emitted by the
    remote node, a custom event, together with updated Aseba variables,
    is broadcasted; once it is received, the value local variables
    are automatically updated. The script also reacts to custom events that
    in turns call native functions.

    Nodes also provide a interface to access variables, events and functions
    by specific attribute instead of by name, where dots in the variables
    name are replaced by underscores in the attribute names.
    For example, Aseba variable ``leds.top`` would be linked to Python
    property ``leds_top``.


    Examples:

       >>> node = Node()
       >>> node.connect("tcp:port=33333")
       True
       >>> node.description.variables
       {"value": ...}

       Setting and getting variables does not require passing ``node_id``

       >>> node.set("value", [1, 2, 3])
       >>> node.get("value")
       [1, 2, 3]

       Assuming that the remote node defines local Aseba event ``e``

       >>> class MyNode(Node):
       >>>     mirroring_config = MirroringConfig(
       ...         events = {"e": EventSpec(variables=["a"])

       after mirroring starts, the node will receive notifications
       when ``e`` is emitted

       >>> node = MyNode()
       >>> node.connect(..., start_mirroring=True)
       >>> node.wait("e")
       True
       >>> node.set_callback("e", lambda node: ...)

       and ``a`` will be updated without further explicit synchronization.

       >>> node.get("a", cached=False)
       ...

       Assuming that the Aseba node defines local function ``f``
       that accepts two integers,

       >>> class MyNode(Node):
       >>>     mirroring_config = MirroringConfig(function_include=['f'])

       after mirroring starts, the node will be able to call
       the function by name

       >>> node = MyNode()
       >>> node.connect(..., start_mirroring=True)
       >>> node.call("f", 1, 2)

       We can inspect the Aseba script loaded the remote node
       for mirroring

       >>> print(node.script)
       onevent call_f
       call f args[1:3]
       onevent e
       emit event_e [a]

       Assuming that the Aseba node has variables ``a`` and ``b``

       >>> class MyNode(Node):
       >>>     properties = ["a", "b"]

       exposes ``a`` and ``b`` as properties with cached values

       >>> node = MyNode()
       >>> node.connect(...)
       >>> node.a = [1, 2]
       >>> node.b
       [3, 4, 5]

       Callbacks for mirrored events are exposed as Python properties too.
       Following a previous example, instead of calling

       >>> node.set_callback("e", cb)

       we can set the attribute directly

       >>> node.on_e = cb

       The node class below expose Aseba functions as Python methods

       >>> class MyNode(Node):
       >>>     functions = ["f"]
       ...     ...
       >>> ...

       Instead of calling

       >>> node.call("f", 1, 2)

       we can call a specific method

       >>> node.call_f(1, 2)

       to perform the same work.

    """

    mirroring_config: MirroringConfig = MirroringConfig()
    """
    Mirroring config
    """
    default_target = ""
    """Default Dashel target"""
    properties: Collection[str] = ()
    """Which variables should be exposed as Python properties"""
    functions: Collection[str] = ()
    """Which functions should be exposed as ``call_<name>`` methods"""
    control_event = ""
    """Which local event to use to trigger the control step"""
    sync_include: Collection[str] = (r'.*', )
    """Regular expressions for variables to be included in :py:meth:`sync`"""
    sync_exclude: Collection[str] = ()
    """Regular expressions for variables to be excluded from :py:meth:`sync`"""
    cached: bool
    """The default value of ``cached``
       used by :py:meth:`set`, :py:meth:`get`, and :py:meth:`get_all`"""

    def __init__(self, cached: bool = False) -> None:
        """
        Constructs a new instance.

        :param cached:  The default value of ``cached``
           used by :py:meth:`set`, :py:meth:`get`, and :py:meth:`get_all`
        """
        # TODO: make copy of default
        self.mirroring_config = dc.replace(self.mirroring_config)
        self.mirroring_config.function_include.extend(list(self.functions))
        self.mirroring: Mirroring | None = None
        self._connection = 0
        self._description: Description | None = None
        self._target = ""
        self.cached = cached
        self._client: Client | None = None
        self._node_id = -1
        self._node_id_int16 = -1
        self._shared_client = False
        self._variable_values: dict[str, list[int]] = {}
        self._variable_sizes: dict[str, int] = {}
        self._event_callbacks: dict[str, EventCallback[Self]] = {}
        self._functions: dict[str, str] = {}
        self._next_variables_values: dict[str, list[int]] = {}
        self._prev_variables_values: dict[str, list[int]] = {}
        self._sync_variables: set[str] = set()
        self._last_payloads: dict[str, list[int]] = {}

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
    def mirrored_functions(self) -> list[str]:
        """
        Which Aseba functions are callable through
        :py:meth:`call`.
        """
        if self.mirroring:
            return list(self.mirroring._functions)
        return []

    @property
    def mirrored_events(self) -> list[str]:
        """
        Which local mirrored Aseba events are await-able through
        :py:meth:`wait`.
        """
        if self.mirroring:
            return list(self.mirroring._events.values())
        return []

    @property
    def events(self) -> list[str]:
        """
        All Aseba user events are await-able through
        :py:meth:`wait`.
        """
        if self._description:
            return list(self._description.user_events)
        return []

    @property
    def sync_variables(self) -> set[str]:
        """
        Which Aseba variables are included in :py:meth:`synch`.
        """
        return self._sync_variables

    def _init_variables(self) -> None:
        variables = self.description.variables if self.description else {}
        self._variable_values = {k: [] for k in variables}
        self._variable_sizes = {k: vs[1] for k, vs in variables.items()}
        self._sync_variables = {
            k
            for k in variables
            if matches(k, self.sync_include, self.sync_exclude)
        }

    def start_mirroring(self) -> None:
        """
        Starts a mirroring local events and native functions.
        through a custom Aseba script, as configured by :py:attr:`mirroring_config`.
        """
        assert (self._client)
        description = self._client.get_description(self._node_id,
                                                   include={self.connection})
        assert (description)
        mirroring = Mirroring(description, self.mirroring_config)
        if mirroring.code:
            if self.load_script(script=mirroring.code,
                                events=mirroring._code_events):
                self.mirroring = mirroring
                self._client.cmd_run(self._node_id)

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
                start_mirroring: bool = False,
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
        :param start_mirroring: Whether to start mirroring after connecting. If not set, call
                             :py:meth:`start_mirroring` later to start mirroring.
        :param kwargs:       Parameters that are appended to ``target`` as ``"<key>=<value>"``.
                             For example, if target is ``"tcp"``, passing ``port=33333``
                             will result in a target ``"tcp:port=33333"``.
        :returns:            Whether the connection was successful.
        """
        target = complete_target(target or self.default_target, **kwargs)
        if client:
            self._shared_client = True
        if not self._client:
            self._client = client or Client()
            self._client.add_event_callback(callback=self._event_cb)

        connections = set(conn for conn, t in self._client.connections.items()
                          if are_targets_compatible(target, t))
        if connections or self._client.connect(
                target=target, wait_ms=wait_ms, max_retries=max_retries):
            if node_id >= 0:
                self._client.cmd_reset(node_id)
            node_id, conn = self._client.wait_node(wait_ms=wait_ms,
                                                   node_id=node_id,
                                                   include=connections)
            if conn:
                self._connection = conn
                self._target = target
                self._node_id = node_id
                self._node_id_int16 = int16(node_id)
                self._description = self._client.get_description(
                    node_id, include={conn})
                if start_mirroring:
                    self.start_mirroring()
                self._init_variables()
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
        assert self._client
        return self._client.get_script(self._node_id,
                                       include={self.connection})

    def _extra_event_cb(self, event: Event) -> None:
        pass

    def _event_cb(self, event: Event) -> None:
        if event.source != self._node_id:
            return
        self._last_payloads[event.name] = event.data
        if self.mirroring:
            for name, value in self.mirroring.read_variables(event).items():
                if name in self._variable_values:
                    self._variable_values[name] = value
                    self._prev_variables_values[name] = value
        self._extra_event_cb(event)
        cb = self._event_callbacks.get(event.name)
        if cb:
            cb(self)  # type: ignore[arg-type]

    def get_last_payload(self, event_name: str) -> list[int]:
        """
        Gets the last event payload.

        :param      event_name:  The event name

        :returns:   The payload of the last received event, if any.
        """
        return self._last_payloads.get(event_name, [])

    def load_script(self,
                    script: str,
                    events: dict[str, int] = {},
                    constants: dict[str, int] = {}) -> bool:
        """
        Loads an Aseba script.

        :param      script:          The script
        :param      events:          User defined events as dictionary
                                     of payload sizes keyed by name.
        :param      constants:       The constants
        :type       constants:       User defined constants as dictionary
                                     of scalar sizes values keyed by name.

        :returns:   True if the script was successfully compiled and loaded.
        """
        assert (self._client)
        self.mirroring = None
        try:
            self._client.load_script(node_id=self._node_id,
                                     script=script,
                                     events=events,
                                     constants=constants)
        except Exception as e:
            m = re.search(r"Error at Line: (\d*)", str(e))
            if m:
                ln = int(m.group(1))
                line = script.splitlines()[ln - 1:ln + 1]
                logging.error(
                    f"Error while compiling script: {e}.\nLine: {line}")
            return False
        self._description = self._client.get_description(
            self._node_id, include={self.connection})
        self._init_variables()
        return True

    def run(self) -> None:
        """
        Sends a command to start running the remote node.
        It is only effective if a script has been loaded.
        """
        assert self._client
        self._client.cmd_run(self._node_id, include={self.connection})

    def pause(self) -> None:
        """
        Sends a command to pause running the remote node.
        """
        assert self._client
        self._client.cmd_pause(self._node_id, include={self.connection})

    def stop(self) -> None:
        """
        Sends a command to stop running the remote node.
        """
        assert (self._client)
        self.mirroring = None
        self._client.cmd_stop(self._node_id, include={self.connection})

    def reset(self) -> None:
        """
        Sends a command to pause reset the remote node.
        """
        assert self._client
        self.mirroring = None
        self._client.cmd_reset(self._node_id, include={self.connection})

    def close(self, reset: bool = False) -> None:
        """
        Closes the node and the client, if it was created by the node.

        :param      reset:  Whether to reset the remote node.
        """
        assert self._client
        if reset:
            self.reset()
        else:
            self.stop()
        time.sleep(0.1)
        if not self._shared_client:
            self._client.close()
            self._client = None
        self._connection = 0
        self._target = ''

    def _event_name(self, name: str) -> str:
        if self.mirroring:
            return self.mirroring.get_event_name(name) or name
        return name

    def wait(self, name: str, wait_ms: int = 1000) -> bool:
        """
        Waits for an event.

        If name is the name of a a mirrored local event, it waits for the
        corresponding user event.

        :param      name:     The name of the event.
        :param      wait_ms:  The maximal time to wait.

        :returns:   Whether an event has been received before the deadline.
        """
        assert (self._client)
        name = self._event_name(name)
        if name not in self.events:
            return False
        e = self._client.get_event(self._node_id,
                                   name,
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
        name = self._event_name(name)
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
        name = self._event_name(name)
        return self._event_callbacks.get(name)

    def emit(self, name: str, *args: int) -> None:
        """
        Emits an event

        :param name:   The name of the (user) event
        :param args:  The payload
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
        :param args: The arguments
        """
        assert (self._client)
        if self.mirroring:
            fname = self.mirroring.get_function_name(name)
            if fname:
                data = [self._node_id_int16] + list(args)
                self._client.emit_event(self._node_id,
                                        fname,
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
            setattr(cls, n, make_property_with_variable(v))
        for f in cls.functions:
            setattr(cls, f"call_{f.replace('.', '_')}",
                    make_method_with_function(f))
        for e in cls.mirroring_config.events:
            setattr(cls, f"on_{e.replace('.', '_')}",
                    make_property_with_event(e))

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
