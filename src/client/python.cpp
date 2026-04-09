#include "dashel_hub.h"
#include "node_manager.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

namespace pybind11 {
namespace detail {

template <> struct type_caster<Aseba::TargetDescription::NamedVariable> {

  PYBIND11_TYPE_CASTER(Aseba::TargetDescription::NamedVariable,
                       io_name("tuple[str, int]", "tuple[str, int]"));

  static handle cast(const Aseba::TargetDescription::NamedVariable &value,
                     return_value_policy /*policy*/, handle /*parent*/) {
    return py::make_tuple(value.name, value.size).release();
  }
};

template <> struct type_caster<Aseba::TargetDescription::LocalEvent> {

  PYBIND11_TYPE_CASTER(Aseba::TargetDescription::LocalEvent,
                       io_name("tuple[str, str]", "tuple[str, str]"));

  static handle cast(const Aseba::TargetDescription::LocalEvent &value,
                     return_value_policy /*policy*/, handle /*parent*/) {
    return py::make_tuple(value.name, value.description).release();
  }
};

template <>
struct type_caster<Aseba::TargetDescription::NativeFunctionParameter> {

  PYBIND11_TYPE_CASTER(Aseba::TargetDescription::NativeFunctionParameter,
                       io_name("tuple[str, int]", "tuple[str, int]"));

  static handle
  cast(const Aseba::TargetDescription::NativeFunctionParameter &value,
       return_value_policy /*policy*/, handle /*parent*/) {
    return py::make_tuple(value.name, value.size).release();
  }
};

template <> struct type_caster<Aseba::TargetDescription::NativeFunction> {

  PYBIND11_TYPE_CASTER(Aseba::TargetDescription::NativeFunction,
                       io_name("tuple[str, str, list[tuple[str, int]]]",
                               "tuple[str, str, list[tuple[str, int]]]"));

  static handle cast(const Aseba::TargetDescription::NativeFunction &value,
                     return_value_policy /*policy*/, handle /*parent*/) {
    return py::make_tuple(value.name, value.description, value.parameters)
        .release();
  }
};

template <> struct type_caster<Aseba::NamedValue> {

  PYBIND11_TYPE_CASTER(Aseba::NamedValue,
                       io_name("tuple[str, int]", "tuple[str, int]"));

  type_caster<Aseba::NamedValue>() : value(std::wstring(), 0){};

  static handle cast(const Aseba::NamedValue &value,
                     return_value_policy /*policy*/, handle /*parent*/) {
    return py::make_tuple(value.name, value.value).release();
  }

  bool load(handle src, bool /*convert*/) {
    // Check if handle is a Sequence
    if (!py::isinstance<py::sequence>(src)) {
      return false;
    }
    auto seq = py::reinterpret_borrow<py::sequence>(src);
    // Check if exactly two values are in the Sequence
    if (seq.size() != 2) {
      return false;
    }
    if (!py::isinstance<py::str>(seq[0])) {
      return false;
    }
    if (!py::isinstance<py::int_>(seq[1]))
      return false;
    value.name = seq[0].cast<std::wstring>();
    value.value = seq[1].cast<int>();
    return true;
  }
};
} // namespace detail
} // namespace pybind11

void DashelHub::incomingData(Dashel::Stream *stream) {
#ifdef ZEROCONF
  if (zeroconf.isStreamHandled(stream)) {
    // log_debug("Incoming data for zeroconf");
    try {
      zeroconf.dashelIncomingData(stream);
    } catch (const std::exception &e) {
      // log_error("incomingData zeroconf: %s", e.what());
    }
    return;
  }
#endif // ZEROCONF
  Aseba::Message *message = nullptr;
  // std::cout << "incomingData from " << stream->getTargetName() << std::endl;
  try {
    message = Aseba::Message::receive(stream);
  } catch (Dashel::DashelException e) {
    return;
  } catch (std::runtime_error e) {
    return;
  }
  if (message) {
    if (!stream_indices.count(stream)) {
      std::cerr << "Unindexed stream " << stream->getTargetName() << std::endl;
    }
    nm->received_msg(message, stream_indices.at(stream));
    delete message;
  }
}

void DashelHub::connectionClosed(Dashel::Stream *stream, bool abnormal) {
#ifdef ZEROCONF
  zeroconf.dashelConnectionClosed(stream);
#endif // ZEROCONF
  if (stream_indices.count(stream)) {
    const auto i = stream_indices.at(stream);
    nm->connectionClosed(i, stream->getTargetName());
  }
  remove_stream(stream);
}

void DashelHub::connectionCreated(Dashel::Stream *stream) {
  add_stream(stream);
  if (stream_indices.count(stream)) {
    const auto i = stream_indices.at(stream);
    nm->connectionCreated(i, stream->getTargetName());
  }
  // nm->connectionCreated(stream);
}

// PYBIND11_MAKE_OPAQUE(std::vector<PyNodesManager::MessageCallback>)

PYBIND11_MODULE(_client_impl, m) {

  // py::bind_vector<std::vector<PyNodesManager::MessageCallback>>(m,
  // "CallbackList");

  py::classh<Event>(m, "Event", R"doc(
)doc")
      .def_readonly("source", &Event::source)
      .def_readonly("name", &Event::name)
      .def_readonly("data", &Event::data)
      .def("__repr__", [](const Event &e) {
        return py::str("Event(source=") + py::str(py::cast(e.source)) +
               py::str(", name='") + py::cast(e.name) + py::str("', data=") +
               py::str(py::cast(e.data)) + py::str(")");
      });

  auto msgs = m.def_submodule("msgs", "TODO");
  py::classh<Aseba::Message>(msgs, "Message", R"doc(
)doc")
      .def_readwrite("source", &Aseba::Message::source)
      .def_readwrite("type", &Aseba::Message::type);

  py::classh<Aseba::UserMessage, Aseba::Message>(msgs, "UserMessage", R"doc(
)doc")
      .def(py::init<uint16_t, Aseba::VariablesDataVector>(), py::arg("type"),
           py::arg("data") = Aseba::VariablesDataVector())
      .def("__repr__",
           [](const Aseba::UserMessage &msg) {
             return py::str("UserMessage(source=") +
                    py::str(py::cast(msg.source)) + py::str(", type=") +
                    py::str(py::cast(msg.type)) + py::str(", data=") +
                    py::str(py::cast(msg.data)) + py::str(")");
           })
      .def_readwrite("data", &Aseba::UserMessage::data);

  py::classh<Aseba::CmdMessage, Aseba::Message>(msgs, "CmdMessage", R"doc(
)doc")
      .def_readwrite("dest", &Aseba::CmdMessage::dest);

  py::classh<Aseba::ListNodes, Aseba::Message>(msgs, "ListNodes", R"doc(
)doc")
      .def(py::init<>())
      .def("__repr__",
           [](const Aseba::ListNodes &msg) {
             return py::str("ListNodes(source=") +
                    py::str(py::cast(msg.source)) + py::str(", version=") +
                    py::str(py::cast(msg.version)) + py::str(")");
           })
      .def_readwrite("version", &Aseba::ListNodes::version);

  py::classh<Aseba::NodePresent, Aseba::Message>(msgs, "NodePresent", R"doc(
)doc")
      .def(py::init<>())
      .def("__repr__",
           [](const Aseba::NodePresent &msg) {
             return "NodePresent(source=" + std::to_string(msg.source) +
                    ", version=" + std::to_string(msg.version) + ")";
           })
      .def_readwrite("version", &Aseba::NodePresent::version);

  py::classh<Aseba::GetDescription, Aseba::Message>(msgs, "GetDescription",
                                                    R"doc(
)doc")
      .def(py::init<>())
      .def("__repr__",
           [](const Aseba::GetDescription &msg) {
             return py::str("GetDescription(source=") +
                    py::str(py::cast(msg.source)) + py::str(", version=") +
                    py::str(py::cast(msg.version)) + py::str(")");
           })
      .def_readwrite("version", &Aseba::GetDescription::version);

  py::classh<Aseba::GetNodeDescription, Aseba::CmdMessage, Aseba::Message>(
      msgs, "GetNodeDescription", R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__",
           [](const Aseba::GetNodeDescription &msg) {
             return py::str("GetNodeDescription(source=") +
                    py::str(py::cast(msg.source)) + py::str(", dest=") +
                    py::str(py::cast(msg.dest)) + py::str(", version=") +
                    py::str(py::cast(msg.version)) + py::str(")");
           })
      .def_readwrite("version", &Aseba::GetNodeDescription::version);

  py::classh<Aseba::Description, Aseba::Message
             // , Aseba::TargetDescription
             >(msgs, "Description", R"doc(
)doc")
      .def(py::init<>())
      .def_readwrite("name", &Aseba::Description::name)
      .def_readwrite("protocol_version", &Aseba::Description::protocolVersion)
      .def("__repr__", [](const Aseba::Description &msg) {
        return py::str("Description(source=") + py::str(py::cast(msg.source)) +
               py::str(", name='") + py::cast(msg.name) +
               py::str("', protocol_version=") +
               py::str(py::cast(msg.protocolVersion)) + py::str(")");
      });

  py::classh<Aseba::NamedVariableDescription, Aseba::Message
             // , Aseba::TargetDescription::NamedVariable
             >(msgs, "NamedVariableDescription", R"doc(
)doc")
      .def(py::init<>())
      .def("__repr__", [](const Aseba::NamedVariableDescription &msg) {
        return py::str("NamedVariableDescription(source=") +
               py::str(py::cast(msg.source)) + py::str(", name='") +
               py::cast(msg.name) + py::str("')");
      });

  py::classh<Aseba::LocalEventDescription, Aseba::Message
             // , Aseba::TargetDescription::LocalEvent
             >(msgs, "LocalEventDescription",
               R"doc(
)doc")
      .def(py::init<>())
      .def("__repr__", [](const Aseba::LocalEventDescription &msg) {
        return py::str("LocalEventDescription(source=") +
               py::str(py::cast(msg.source)) + py::str(", name='") +
               py::cast(msg.name) + py::str("')");
      });

  py::classh<Aseba::NativeFunctionDescription, Aseba::Message
             // , Aseba::TargetDescription::NativeFunction
             >(msgs, "NativeFunctionDescription", R"doc(
)doc")
      .def(py::init<>())
      .def("__repr__", [](const Aseba::NativeFunctionDescription &msg) {
        return py::str("NativeFunctionDescription(source=") +
               py::str(py::cast(msg.source)) + py::str(", name='") +
               py::cast(msg.name) + py::str("')");
      });

  py::classh<Aseba::Disconnected, Aseba::Message>(msgs, "Disconnected", R"doc(
)doc")
      .def(py::init<>())
      .def("__repr__", [](const Aseba::Disconnected &msg) {
        return "Disconnected(source=" + std::to_string(msg.source) + ")";
      });

  py::classh<Aseba::Variables, Aseba::Message>(msgs, "Variables", R"doc(
)doc")
      .def(py::init<>())
      .def_readwrite("start", &Aseba::Variables::start)
      .def_readwrite("variables", &Aseba::Variables::variables)
      .def("__repr__", [](const Aseba::Variables &msg) {
        return py::str("Variables(source=") + py::str(py::cast(msg.source)) +
               py::str(", start=") + py::str(py::cast(msg.start)) +
               py::str(", variables=") + py::str(py::cast(msg.variables)) +
               py::str(")");
      });

  py::classh<Aseba::ArrayAccessOutOfBounds, Aseba::Message>(
      msgs, "ArrayAccessOutOfBounds", R"doc(
)doc")
      .def(py::init<>())
      .def_readwrite("pc", &Aseba::ArrayAccessOutOfBounds::pc)
      .def_readwrite("size", &Aseba::ArrayAccessOutOfBounds::size)
      .def_readwrite("index", &Aseba::ArrayAccessOutOfBounds::index)
      .def("__repr__", [](const Aseba::ArrayAccessOutOfBounds &msg) {
        return py::str("ArrayAccessOutOfBounds(source=") +
               py::str(py::cast(msg.source)) + py::str(", pc=") +
               py::str(py::cast(msg.pc)) + py::str(", size=") +
               py::str(py::cast(msg.size)) + py::str(", index=") +
               py::str(py::cast(msg.index)) + py::str(")");
      });

  py::classh<Aseba::DivisionByZero, Aseba::Message>(msgs, "DivisionByZero",
                                                    R"doc(
)doc")
      .def(py::init<>())
      .def_readwrite("pc", &Aseba::DivisionByZero::pc)
      .def("__repr__", [](const Aseba::DivisionByZero &msg) {
        return py::str("DivisionByZero(source=") +
               py::str(py::cast(msg.source)) + py::str(", pc=") +
               py::str(py::cast(msg.pc)) + py::str(")");
      });
  py::classh<Aseba::EventExecutionKilled, Aseba::Message>(
      msgs, "EventExecutionKilled", R"doc(
)doc")
      .def(py::init<>())
      .def_readwrite("pc", &Aseba::EventExecutionKilled::pc)
      .def("__repr__", [](const Aseba::EventExecutionKilled &msg) {
        return py::str("EventExecutionKilled(source=") +
               py::str(py::cast(msg.source)) + py::str(", pc=") +
               py::str(py::cast(msg.pc)) + py::str(")");
      });

  py::classh<Aseba::NodeSpecificError, Aseba::Message>(msgs,
                                                       "NodeSpecificError",
                                                       R"doc(
)doc")
      .def(py::init<>())
      .def_readwrite("pc", &Aseba::NodeSpecificError::pc)
      .def_readwrite("message", &Aseba::NodeSpecificError::message)
      .def("__repr__", [](const Aseba::NodeSpecificError &msg) {
        return py::str("EventExecutionKilled(source=") +
               py::str(py::cast(msg.source)) + py::str(", pc=") +
               py::str(py::cast(msg.pc)) + py::str(", message=") +
               py::cast(msg.message) + py::str(")");
      });

  py::classh<Aseba::ExecutionStateChanged, Aseba::Message>(
      msgs, "ExecutionStateChanged",
      R"doc(
)doc")
      .def(py::init<>())
      .def_readwrite("pc", &Aseba::ExecutionStateChanged::pc)
      .def_readwrite("flags", &Aseba::ExecutionStateChanged::flags)
      .def("__repr__", [](const Aseba::ExecutionStateChanged &msg) {
        return py::str("ExecutionStateChanged(source=") +
               py::str(py::cast(msg.source)) + py::str(", pc=") +
               py::str(py::cast(msg.pc)) + py::str(", flags=") +
               py::str(py::cast(msg.flags)) + py::str(")");
      });

  py::classh<Aseba::BreakpointSetResult, Aseba::Message>(msgs,
                                                         "BreakpointSetResult",
                                                         R"doc(
)doc")
      .def(py::init<>())
      .def_readwrite("pc", &Aseba::BreakpointSetResult::pc)
      .def_readwrite("success", &Aseba::BreakpointSetResult::success)
      .def("__repr__", [](const Aseba::BreakpointSetResult &msg) {
        return py::str("BreakpointSetResult(source=") +
               py::str(py::cast(msg.source)) + py::str(", pc=") +
               py::str(py::cast(msg.pc)) + py::str(", success=") +
               py::cast(msg.success) + py::str(")");
      });

  py::classh<Aseba::SetBytecode, Aseba::CmdMessage, Aseba::Message>(
      msgs, "SetBytecode",
      R"doc(
)doc")
      .def(py::init<uint16_t, uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID),
           py::arg("start") = 0)
      .def_readwrite("start", &Aseba::SetBytecode::start)
      .def_readwrite("bytecode", &Aseba::SetBytecode::bytecode)
      .def("__repr__", [](const Aseba::SetBytecode &msg) {
        return py::str("SetBytecode(source=") + py::str(py::cast(msg.source)) +
               py::str(", dest=") + py::str(py::cast(msg.dest)) +
               py::str(", start=") + py::str(py::cast(msg.start)) +
               py::str(")");
      });

  py::classh<Aseba::Reset, Aseba::CmdMessage, Aseba::Message>(msgs, "Reset",
                                                              R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::Reset &msg) {
        return py::str("Reset(source=") + py::str(py::cast(msg.source)) +
               py::str(", dest=") + py::str(py::cast(msg.dest)) + py::str(")");
      });

  py::classh<Aseba::Run, Aseba::CmdMessage, Aseba::Message>(msgs, "Run",
                                                            R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::Run &msg) {
        return py::str("Run(source=") + py::str(py::cast(msg.source)) +
               py::str(", dest=") + py::str(py::cast(msg.dest)) + py::str(")");
      });
  py::classh<Aseba::Pause, Aseba::CmdMessage, Aseba::Message>(msgs, "Pause",
                                                              R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::Pause &msg) {
        return py::str("Pause(source=") + py::str(py::cast(msg.source)) +
               py::str(", dest=") + py::str(py::cast(msg.dest)) + py::str(")");
      });
  py::classh<Aseba::Step, Aseba::CmdMessage, Aseba::Message>(msgs, "Step",
                                                             R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::Step &msg) {
        return py::str("Step(source=") + py::str(py::cast(msg.source)) +
               py::str(", dest=") + py::str(py::cast(msg.dest)) + py::str(")");
      });
  py::classh<Aseba::Stop, Aseba::CmdMessage, Aseba::Message>(msgs, "Stop",
                                                             R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::Stop &msg) {
        return py::str("Stop(source=") + py::str(py::cast(msg.source)) +
               py::str(", dest=") + py::str(py::cast(msg.dest)) + py::str(")");
      });

  py::classh<Aseba::GetExecutionState, Aseba::CmdMessage, Aseba::Message>(
      msgs, "GetExecutionState",
      R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::GetExecutionState &msg) {
        return py::str("GetExecutionState(source=") +
               py::str(py::cast(msg.source)) + py::str(", dest=") +
               py::str(py::cast(msg.dest)) + py::str(")");
      });

  py::classh<Aseba::BreakpointSet, Aseba::CmdMessage, Aseba::Message>(
      msgs, "BreakpointSet",
      R"doc(
)doc")
      .def(py::init<uint16_t, uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID),
           py::arg("pc") = 0)
      .def_readwrite("pc", &Aseba::BreakpointSet::pc)
      .def("__repr__", [](const Aseba::BreakpointSet &msg) {
        return py::str("BreakpointSet(source=") +
               py::str(py::cast(msg.source)) + py::str(", dest=") +
               py::str(py::cast(msg.dest)) + py::str(", pc=") +
               py::str(py::cast(msg.pc)) + py::str(")");
      });

  py::classh<Aseba::BreakpointClear, Aseba::CmdMessage, Aseba::Message>(
      msgs, "BreakpointClear",
      R"doc(
)doc")
      .def(py::init<uint16_t, uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID),
           py::arg("pc") = 0)
      .def_readwrite("pc", &Aseba::BreakpointClear::pc)
      .def("__repr__", [](const Aseba::BreakpointClear &msg) {
        return py::str("BreakpointClear(source=") +
               py::str(py::cast(msg.source)) + py::str(", dest=") +
               py::str(py::cast(msg.dest)) + py::str(", pc=") +
               py::str(py::cast(msg.pc)) + py::str(")");
      });

  py::classh<Aseba::BreakpointClearAll, Aseba::CmdMessage, Aseba::Message>(
      msgs, "BreakpointClearAll",
      R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::BreakpointClearAll &msg) {
        return py::str("BreakpointClearAll(source=") +
               py::str(py::cast(msg.source)) + py::str(", dest=") +
               py::str(py::cast(msg.dest)) + py::str(")");
      });

  py::classh<Aseba::GetVariables, Aseba::CmdMessage, Aseba::Message>(
      msgs, "GetVariables",
      R"doc(
)doc")
      .def(py::init<uint16_t, uint16_t, uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID),
           py::arg("start") = 0, py::arg("length") = 0)
      .def_readwrite("start", &Aseba::GetVariables::start)
      .def_readwrite("length", &Aseba::GetVariables::length)
      .def("__repr__", [](const Aseba::GetVariables &msg) {
        return py::str("GetVariables(source=") + py::str(py::cast(msg.source)) +
               py::str(", dest=") + py::str(py::cast(msg.dest)) +
               py::str(", start=") + py::str(py::cast(msg.start)) +
               py::str(", length=") + py::str(py::cast(msg.length)) +
               py::str(")");
      });

  py::classh<Aseba::SetVariables, Aseba::CmdMessage, Aseba::Message>(
      msgs, "SetVariables",
      R"doc(
)doc")
      .def(py::init<uint16_t, uint16_t, Aseba::VariablesDataVector>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID),
           py::arg("start") = 0,
           py::arg("variables") = Aseba::VariablesDataVector())
      .def_readwrite("start", &Aseba::SetVariables::start)
      .def_readwrite("variables", &Aseba::SetVariables::variables)
      .def("__repr__", [](const Aseba::SetVariables &msg) {
        return py::str("SetVariables(source=") + py::str(py::cast(msg.source)) +
               py::str(", dest=") + py::str(py::cast(msg.dest)) +
               py::str(", start=") + py::str(py::cast(msg.start)) +
               py::str(", variables=") + py::str(py::cast(msg.variables)) +
               py::str(")");
      });

  py::classh<Aseba::WriteBytecode, Aseba::CmdMessage, Aseba::Message>(
      msgs, "WriteBytecode",
      R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::WriteBytecode &msg) {
        return py::str("WriteBytecode(source=") +
               py::str(py::cast(msg.source)) + py::str(", dest=") +
               py::str(py::cast(msg.dest)) + py::str(")");
      });

  py::classh<Aseba::Reboot, Aseba::CmdMessage, Aseba::Message>(msgs, "Reboot",
                                                               R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::Reboot &msg) {
        return py::str("Reboot(source=") + py::str(py::cast(msg.source)) +
               py::str(", dest=") + py::str(py::cast(msg.dest)) + py::str(")");
      });

  py::classh<Aseba::Sleep, Aseba::CmdMessage, Aseba::Message>(msgs, "Sleep",
                                                              R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::Sleep &msg) {
        return py::str("Sleep(source=") + py::str(py::cast(msg.source)) +
               py::str(", dest=") + py::str(py::cast(msg.dest)) + py::str(")");
      });

  py::classh<Aseba::TargetDescription>(m, "Description", R"doc(
)doc")
      .def_readonly("name", &Aseba::TargetDescription::name)
      .def_readonly("protocol_version",
                    &Aseba::TargetDescription::protocolVersion)
      .def_readonly("variables", &Aseba::TargetDescription::namedVariables)
      .def_readonly("events", &Aseba::TargetDescription::localEvents)
      .def_readonly("functions", &Aseba::TargetDescription::nativeFunctions)
      .def("__repr__",
           [](const Aseba::TargetDescription &d) {
             return py::str("Description(name='") + py::cast(d.name) +
                    py::str("', protocol_version=") +
                    py::str(py::cast(d.protocolVersion)) +
                    py::str(", variables=") +
                    py::str(py::cast(d.namedVariables)) + py::str(", events=") +
                    py::str(py::cast(d.localEvents)) + py::str(", functions=") +
                    py::str(py::cast(d.nativeFunctions)) + py::str(")");
           })
      .def_property(
          "_variables_map",
          [](const Aseba::TargetDescription &desc) {
            unsigned i;
            return desc.getVariablesMap(i);
          },
          nullptr);

  py::classh<PyNodesManager>(m, "Client", R"doc(
)doc")
      .def(py::init<int, bool>(), py::arg("port") = -1, py::arg("query") = true)
      // .def_readwrite("message_callbacks", &PyNodesManager::message_callbacks,
      // py::return_value_policy::reference)
      .def("clear_incoming_messages", &PyNodesManager::clear_in_msgs)
      .def_readwrite("query", &PyNodesManager::query)
      .def("_query", &PyNodesManager::query_description, py::arg("node"), py::arg("wait_ms") = 1000, py::arg("callback") = nullptr, py::return_value_policy::copy)
      .def_property("nodes", &PyNodesManager::get_nodes, nullptr)
      .def_property("_nodes", &PyNodesManager::get_nodes_, nullptr)
      .def("connect", &PyNodesManager::connect_and_start, py::arg("target"),
           py::arg("wait_ms") = 1000, py::arg("max_retries") = 3,
           py::arg("ping") = true)
      .def_property("is_connected", &PyNodesManager::is_connected, nullptr)
      .def_property("connected_targets", &PyNodesManager::get_connected_targets,
                    nullptr)
      .def_property(
          "is_running", [](const PyNodesManager &m) { return !m.stopped; },
          nullptr)
      .def("_connect", &PyNodesManager::connect, py::arg("target"))
      .def("_close", &PyNodesManager::close)
      .def("_start", &PyNodesManager::start, py::arg("ping") = true)
      .def("_stop", &PyNodesManager::stop)
      .def("has_node", &PyNodesManager::has_node, py::arg("node"))
      .def("get_message", &PyNodesManager::get_message, py::arg("node") = -1,
           py::arg("type") = -1, py::arg("wait_ms") = 1000,
           py::arg("callback") = nullptr)
      .def("get_event", &PyNodesManager::get_event, py::arg("node") = -1,
           py::arg("name") = "", py::arg("wait_ms") = 1000,
           py::arg("callback") = nullptr)
      .def("wait_target_connection", &PyNodesManager::wait_target_connection,
           py::arg("index") = -1, py::arg("wait_ms") = 1000,
           py::arg("callback") = nullptr)
      .def("wait_target_disconnection",
           &PyNodesManager::wait_target_disconnection, py::arg("index") = -1,
           py::arg("wait_ms") = 1000, py::arg("callback") = nullptr)
      .def("wait_nodes", &PyNodesManager::wait_nodes,
           py::arg("nodes") = std::set<uint16_t>(), py::arg("number") = -1,
           py::arg("wait_ms") = 1000, py::arg("callback") = nullptr)
      .def("wait_node_connection", &PyNodesManager::wait_node_connection,
           py::arg("node") = -1, py::arg("wait_ms") = 1000,
           py::arg("callback") = nullptr)
      .def("wait_node_disconnection", &PyNodesManager::wait_node_disconnection,
           py::arg("node") = -1, py::arg("wait_ms") = 1000,
           py::arg("callback") = nullptr)
      // TODO: better name
      .def("ping", &PyNodesManager::ping)
      .def("close", &PyNodesManager::stop_and_close)
      .def("run", &PyNodesManager::run, py::arg("node"))
      .def("stop", &PyNodesManager::stop_node, py::arg("node"))
      .def("pause", &PyNodesManager::pause, py::arg("node"))
      .def("reset", &PyNodesManager::reset, py::arg("node"))
      .def("sleep", &PyNodesManager::sleep, py::arg("node"))
      .def("scan", &PyNodesManager::scan, py::arg("number") = -1,
           py::arg("wait_ms") = 1000, py::arg("callback") = nullptr)
      .def("send_user_message", &PyNodesManager::sendUserMessage,
           py::arg("type"), py::arg("payload") = Aseba::VariablesDataVector())
      .def("send_message", &PyNodesManager::send_message, py::arg("message"),
           py::arg("target_index") = -1,
           py::arg("exclude_target_indices") = std::set<unsigned>())
      .def("emit_event", &PyNodesManager::emit_event, py::arg("node"),
           py::arg("name"), py::arg("payload") = Aseba::VariablesDataVector())
      .def("send_event", &PyNodesManager::send_event, py::arg("node"),
           py::arg("type"), py::arg("payload") = Aseba::VariablesDataVector())
      .def("get_description", &PyNodesManager::getDescription, py::arg("node"),
           py::return_value_policy::copy)
      .def("get_variable", &PyNodesManager::get_variable, py::arg("node"),
           py::arg("name"), py::arg("wait_ms"), py::arg("callback") = nullptr)
      .def("_get_variables", &PyNodesManager::get_variable_at_index,
           py::arg("node"), py::arg("index"), py::arg("length"),
           py::arg("wait_ms"), py::arg("callback") = nullptr)
      .def("set_variable", &PyNodesManager::set_variable, py::arg("node"),
           py::arg("name"), py::arg("value"))
      .def("set_variables", &PyNodesManager::set_variable_at_index,
           py::arg("node"), py::arg("index"), py::arg("values"))
      .def("add_event_callback", &PyNodesManager::add_event_callback,
           py::arg("callback"))
      .def("add_message_callback", &PyNodesManager::add_message_callback,
           py::arg("callback"))
      .def("remove_message_callback", &PyNodesManager::remove_message_callback,
           py::arg("index"))
      .def("clear_message_callbacks", &PyNodesManager::clear_message_callbacks)
      .def("add_target_connection_callback",
           &PyNodesManager::add_target_connection_callback, py::arg("callback"))
      .def("add_target_disconnection_callback",
           &PyNodesManager::add_target_disconnection_callback,
           py::arg("callback"))
      .def("add_node_connection_callback",
           &PyNodesManager::add_node_connection_callback, py::arg("callback"))
      .def("add_node_disconnection_callback",
           &PyNodesManager::add_node_disconnection_callback,
           py::arg("callback"))
      .def("get_user_events", &PyNodesManager::getEvents, py::arg("node"))
      .def("get_variables", &PyNodesManager::getVariables, py::arg("node"))
      .def("get_all_variables", &PyNodesManager::get_variables, py::arg("node"),
           py::arg("wait_ms") = 1000, py::arg("callback") = nullptr)
      .def("get_variables_size", &PyNodesManager::get_variables_size,
           py::arg("node"))
      .def("advertise", &PyNodesManager::advertise)
      .def("deadvertise", &PyNodesManager::deadvertise)
      .def("load_script", &PyNodesManager::load_script, py::arg("node"),
           py::arg("script"),
           py::arg("events") = std::vector<Aseba::NamedValue>{},
           py::arg("constants") = std::vector<Aseba::NamedValue>{});

  m.def("scan_serial_ports", &Dashel::SerialPortEnumerator::getPorts);
}