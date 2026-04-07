#include "network.h"
#include "node.h"
#include <algorithm>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

using FunctionArgSpec = std::tuple<int16_t, std::string>;
using FunctionSpec = std::tuple<std::string, std::vector<FunctionArgSpec>>;

struct PyNode : public Node, public py::trampoline_self_life_support {

  using Node::Node;

  void init() override {
    init_variables();
    init_events();
    init_functions();
    PYBIND11_OVERRIDE(void, Node, init);
  }

  void init_events() {
    const auto events =
        py::cast(this).attr("events").cast<std::vector<std::string>>();
    for (const auto &name : events) {
      add_event(name, "");
    }
  }

  void init_variables() {
    const auto variables =
        py::cast(this)
            .attr("variables")
            .cast<std::vector<std::tuple<std::string, unsigned>>>();
    for (const auto &[name, size] : variables) {
      add_variable(name, size);
    }
  }

  void init_functions() {
    auto signature = py::module_::import("inspect").attr("signature");
    const auto functions =
        py::cast(this).attr("functions").cast<std::vector<FunctionSpec>>();
    for (const auto &[name, args] : functions) {
      const auto p = py::cast(this);
      const auto n = py::cast(name);
      if (py::hasattr(p, n)) {
        const auto &fun = py::cast(this).attr(n);
        const auto inputs = py::len(signature(fun).attr("parameters"));
        add_function(name, "", args, inputs);
      }
    }
  }

  void call_extra_function(AsebaVMState *vm, unsigned id) override {
    if (id > functions.size())
      return;
    const auto &[name, args, inputs] = functions[id];
    const auto &fun = py::cast(this).attr(py::cast(name));
    // std::vector<std::vector<int16_t>> arg_values(args.size());
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
    // py::print("set result to ", num, py::cast(rs));
    for (i = 0; i < num; i++) {
      const size_t size = static_cast<size_t>(args[args.size() - i - 1]);
      uint16_t address = addresses.top();

      addresses.pop();
      const auto &values = rs[i];
      std::copy(std::begin(values), std::begin(values) + size,
                vm->variables + address);
    }
  }

  void tick(float dt) override { PYBIND11_OVERRIDE(void, Node, tick, dt); }

  void reset() override {
    Node::reset();
    PYBIND11_OVERRIDE(void, Node, reset);
  }
};

PYBIND11_MODULE(_server_impl, m) {

  py::classh<Network>(m, "Server", R"doc(
)doc")
      .def(py::init<const std::string &, int, int>(),
           py::arg("address") = "0.0.0.0", py::arg("port") = ASEBA_DEFAULT_PORT,
           py::arg("timeout") = 0)
      .def_property("port", &Network::get_port, nullptr)
      .def_property("address", &Network::get_address, nullptr)
      .def("spin", &Network::spin, py::arg("time_step"))
      .def("add_node", &Network::add_node, py::arg("node"));

  py::classh<Node, PyNode>(m, "Node", R"doc(
)doc")
      .def(py::init_alias<unsigned, const std::string &>(), py::arg("node_id"),
           py::arg("name"))
      .def("emit", &Node::emit_name, py::arg("name"))
      .def("set", &Node::set_variable, py::arg("name"), py::arg("value"))
      .def("get", &Node::get_variable, py::arg("name"));
}