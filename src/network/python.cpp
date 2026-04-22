#include "network.h"
#include "node.h"
#include <algorithm>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#ifdef ENABLE_LOGGING
#include "pybind11_log.h"
#endif

namespace py = pybind11;

struct PyNode : public Node, public py::trampoline_self_life_support {

  using Node::Node;

  void init() override {
    Node::init();
    init_variables();
    init_events();
    init_functions();
    PYBIND11_OVERRIDE(void, Node, init);
  }

  void init_events() {
    auto obj = py::cast(this);
    if (!py::hasattr(obj, "events"))
      return;
    const auto events = obj.attr("events").cast<Description::EventsMap>();
    for (const auto &[name, desc] : events) {
      add_event(name, desc);
    }
  }

  void init_variables() {
    auto obj = py::cast(this);
    if (!py::hasattr(obj, "variables"))
      return;
    const auto variables =
        obj.attr("variables").cast<std::map<std::string, uint16_t>>();
    for (const auto &[name, size] : variables) {
      add_variable(name, size);
    }
  }

  void init_functions() {
    auto obj = py::cast(this);
    if (!py::hasattr(obj, "functions"))
      return;
    auto signature = py::module_::import("inspect").attr("signature");
    const auto functions =
        obj.attr("functions").cast<Description::FunctionsMap>();
    for (const auto &[name, fun] : functions) {
      const auto n = py::cast(name);
      if (py::hasattr(obj, n)) {
        const auto &f = obj.attr(n);
        const auto inputs = py::len(signature(f).attr("parameters"));
        add_function(name, fun, inputs);
      }
    }
  }

  void call_extra_function(AsebaVMState *vm, unsigned id) override {
    if (id >= functions.size())
      return;
    pybind11::gil_scoped_acquire acquire;
    const auto &[name, args, inputs] = functions[id];
    const auto &fun = py::cast(this).attr(py::cast(name));
    std::stack<uint16_t> addresses;
    size_t i = 0;
    py::tuple pyargs(inputs);
    for (const auto &size : args) {
      const uint16_t address = static_cast<uint16_t>(AsebaNativePopArg(vm));
      addresses.push(address);
      if (i < inputs) {
        pyargs[i] = std::vector<int16_t>(vm->variables + address,
                                         vm->variables + address + size);
      }
      i++;
    }
    const auto r = fun(*pyargs);
    std::vector<std::vector<int16_t>> rs;
    if (r.is_none()) {

    } else if (py::isinstance<py::tuple>(r)) {
      rs = r.cast<std::vector<std::vector<int16_t>>>();
    } else if (py::isinstance<py::list>(r)) {
      rs = {r.cast<std::vector<int16_t>>()};
    } else if (py::isinstance<py::int_>(r)) {
      rs = {{r.cast<int16_t>()}};
    }
    const auto num = std::min(args.size(), rs.size());
    for (i = 0; i < num; i++) {
      const size_t size = static_cast<size_t>(args[args.size() - i - 1]);
      uint16_t address = addresses.top();

      addresses.pop();
      const auto &values = rs[i];
      std::copy(std::begin(values), std::begin(values) + size,
                vm->variables + address);
    }
  }

  void tick(float dt) override {
    pybind11::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(void, Node, tick, dt);
  }

  void reset() override {
    Node::reset();
    PYBIND11_OVERRIDE(void, Node, reset);
  }
};

PYBIND11_MODULE(_network_impl, m) {

  py::options options;
  options.enable_function_signatures();

  auto node = py::classh<Node, PyNode>(m, "Node", R"doc(
Node(node_id: int, name: str = "node", default_variables: bool = True, default_functions: bool = True, uuid: Sequence[int] = ..., advertised_name: str = "")

Args:
  node_id: The id of the node. It should be unique in the same network.
  name: The name of the node.
  default_variables: Whether to add Aseba default native variables.
  default_functions: Whether to add Aseba default native functions.
  uuid: An optional unique identifier for auto-discovery
  advertised_name: An optional alternative name to use for auto-discovery 


Examples:
    
    The base class is typically sub-classed to implement
    specific Aseba nodes. 

    >>> class MyNode(Node):

    We can add Aseba local events by specifying names,

    >>>     events = ["event"]

    Aseba variables by specifying tuples of ``(name, size)``,

    >>>     variables = [("value", 1)]

    and Aseba local functions by specifying tuples 
    of ``(name, parameters)``, where each parameters is specified
    by a size and a name. The name must corresponds to a compatible 
    Python method

    >>>     functions = [("add", [(1, "first"), (1, "second")])]
    >>>
    >>>     def add(self, first: list[int], second: list[int]) -> None:
    >>>         if (len(first) == len(second) == 1):
    >>>             result = [x + y for x, y in zip(first, second)]
    >>>             self.set("value", result)


    We can also override virtual functions :py:meth:`init`, :py:meth:`tick`, 
    and :py:meth:`reset` to specialize a node.
   
    >>>     def init(self) -> None:
    >>>         self.set("value", 0)
    >>> 
    >>>     def tick(self, time_step: float) -> None:
    >>>         self.emit("event")
    >>> 
    >>>     def reset(self) -> None:
    >>>         self.set("value", 0)

    Nodes should be added to a network, else
    they will not perform any work.

    >>> network = Network() 
    >>> network.add_node(MyNode(id=0))

    During spinning, the network will dispatch Aseba messages
    to the node and call py:meth:`tick` every ``time_step`` seconds. 

    >>> network.spin(time_step=0.1)
    
)doc");
  auto network = py::classh<Network>(m, "Network", R"doc(
Network(address: str = "0.0.0.0", port: int = 33333, advertised_name: str = "pyaseba")

Aseba network hosting :py:class:`pyaseba.network.Node` nodes. 
Clients can connect to the dashel target ``"tcp:address=<address>;port=<port>"``.

Args:
  address: The IP address to connect to this network.
  port: The port to connect to this network.
  advertised_name: The name used for auto-discovery.

Examples:
    
    The typical life-cycle of an Aseba network 
    starts by creating it,

    >>> network = Network()

    adding one or more nodes

    >>> network.add_node(...)

    and then letting it spin

    >>> network.spin(time_step=0.1, duration=10)

    In case we need to spin it in different thread, 
    we can start

    >>> network.start(time_step=0.1, duration=10)

    and possibly stop it later

    >>> network.stop()
)doc");

  auto desc = py::classh<Description>(m, "Description", R"doc(
The description of an Aseba node.
)doc");

  network
      .def("__repr__",
           [](const Network &n) {
             return py::str("Network(address='") + py::str(n.get_address()) +
                    py::str("', port='") + py::str(py::cast(n.get_port())) +
                    py::str(")");
           })
      .def_property("port", &Network::get_port, nullptr, R"doc(
The port (readonly).
)doc")
      .def_property("address", &Network::get_address, nullptr, R"doc(
The IP address (readonly).
)doc");

  node.def("__repr__",
           [](const Node &n) {
             return py::str("Node(node_id='") +
                    py::str(py::cast(n.get_node_id())) + py::str("', name='") +
                    py::str(n.name) + py::str(")");
           })
      .def_property("node_id", &Node::get_node_id, nullptr, R"doc(
The node id (readonly).
)doc")
      .def_readonly("description", &Node::description, R"doc(
The node description (readonly).
)doc")
      .def_readonly("name", &Node::name, R"doc(
The node name (readonly)..
)doc");

  desc.def_property("name", &Description::get_name, nullptr, R"doc(
The name of the Aseba node (readonly).
)doc")
      .def_property("protocol_version", &Description::get_protocol_version,
                    nullptr,
                    R"doc(
The version of Aseba used by the node (readonly).
)doc")
      .def_property("variables", &Description::get_variables_map, nullptr,
                    R"doc(
The variables defined by the Aseba node as a dictionary
of ``(index, size)`` tuples keyed by name (readonly).
)doc")
      .def_property("local_events", &Description::get_events_map, nullptr,
                    R"doc(
The local events defined by the Aseba node as a dictionary
of descriptions keyed by name (readonly).

Local events are locally emitted by the Aseba node and 
can only be accessed through an Aseba script running on the node.
)doc")
      .def_property("functions", &Description::get_functions_map, nullptr,
                    R"doc(
The local functions defined by the Aseba node 
as a dictionary of ``(description, arguments)`` tuples keyed by name,
where each argument is a tuple ``(name, size)`` (readonly).

Local functions can be called through an Aseba script running on the node.
)doc")
      .def("__repr__", [](const Description &d) {
        return py::str("Description(name='") + py::cast(d.get_name()) +
               py::str("', protocol_version=") +
               py::str(py::cast(d.get_protocol_version())) +
               py::str(", variables=") +
               py::str(py::cast(d.get_variables_map())) +
               py::str(", local_events=") +
               py::str(py::cast(d.get_events_map())) + py::str(", functions=") +
               py::str(py::cast(d.get_functions_map())) + py::str(")");
      });

  options.disable_function_signatures();

  network
      .def(py::init<const std::string &, int, const std::string &>(),
           py::arg("address") = "0.0.0.0", py::arg("port") = ASEBA_DEFAULT_PORT,
           py::arg("advertised_name") = "pyaseba", R"doc(
__init__(self, address: str = "0.0.0.0", port: int = 33333, advertised_name: str = "pyaseba") -> None

Constructs an instance.
)doc")
      .def("spin", &Network::spin, py::arg("time_step"),
           py::arg("duration") = -1, R"doc(
spin(self, time_step: float, duration: float = -1) -> None

Spins the network for some time.
During spinning, it dispatches messages to nodes, 
it runs the nodes Aseba virtual machine, and
it regularly call :py:meth:`pyaseba.network.Node.tick`.

Arguments:
  time_step: the period in seconds between calling :py:meth:`pyaseba.network.Node.tick`
  duration: the duration in seconds. If negative, it will keep spinning indefinitely.
)doc")
      .def(
          "spin_async",
          [](py::object network, double time_step, double duration) {
            const auto to_thread =
                py::module_::import("asyncio").attr("to_thread");
            return to_thread(network.attr("_spin_no_gil"), py::cast(time_step), py::cast(duration));
          },
          py::arg("time_step"), py::arg("duration") = -1, R"doc(
spin_async(self, time_step: float, duration: float = -1) -> Awaitable[None]

Spins the network for some time in a background thread.
During spinning, it dispatches messages to nodes, 
it runs the nodes Aseba virtual machine, and
it regularly call :py:meth:`pyaseba.network.Node.tick`.

Arguments:
  time_step: the period in seconds between calling :py:meth:`pyaseba.network.Node.tick`
  duration: the duration in seconds. If negative, it will keep spinning indefinitely.
)doc")
      .def(
          "_spin_no_gil", &Network::spin,
          py::arg("time_step"), py::arg("duration") = -1, py::call_guard<py::gil_scoped_release>())
      .def("start", &Network::start, py::arg("time_step"),
           py::arg("duration") = -1, R"doc(
start(self, time_step: float, duration: float = -1) -> None

Starts spinning inside a background thread.

Arguments:
  time_step: the period in seconds between calling :py:meth:`pyaseba.network.Node.tick`
  duration: the duration in seconds. If negative, it will keep spinning indefinitely.
)doc")
      .def("stop", &Network::stop, R"doc(
stop(self) -> None

Stops spinning (only relevant if already spinning inside a background thread).

)doc")
      .def("add_node", &Network::add_node, py::arg("node"), R"doc(
add_node(self, node: Node) -> None

Adds a node to the network.

Arguments:
  node: the node.
)doc");

  node.def(py::init<unsigned, const std::string &, bool, bool,
                    const std::array<uint8_t, 16>, const std::string &>(),
           py::arg("node_id"), py::arg("name") = "node",
           py::arg("default_variables") = true,
           py::arg("default_functions") = true,
           py::arg("uuid") = std::array<uint8_t, 16>(),
           py::arg("advertised_name") = "", R"doc(
__init__(self, node_id: int, name: str = "node", default_variables: bool = True, default_functions: bool = True, uuid: Sequence[int] = ..., advertised_name: str = "") -> None

Constructs an instance.
)doc")
      .def("emit", &Node::emit_name, py::arg("name"), R"doc(
emit(self, name: str) -> None

Emits a local event. The list of events should be known a-priori
by specifying the class py:attr:`Node.events`.

Arguments:
  name: the name of the event.
)doc")
      .def("get_all", &Node::get_variables, R"doc(
get_all(self) -> dict[str, list[int]]

Gets the current value of all Aseba variables

Returns:
  A dictionary of variable values keyed by variable names.
)doc")
      .def("set", &Node::set_variable, py::arg("name"), py::arg("value"), R"doc(
set(self, name: str, value: Sequence[int]) -> None

Sets the value of an Aseba variable. The variable should be defined a-priori
by specifying the specific class py:attr:`Node.variables` or as part of
the default Aseba variables.

Arguments:
  name: the variable name.
  value: the variable value.
)doc")
      .def("get", &Node::get_variable, py::arg("name"), R"doc(
get(self, name: str) -> list[int]

Gets the value of an Aseba variable. The variable should be defined a-priori
by specifying the specific class py:attr:`Node.variables` or as part of
the default Aseba variables.

Arguments:
  name: the variable name.

Returns:
  the variable value.
)doc");
}