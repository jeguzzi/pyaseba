#include "client.h"
#include <pybind11/native_enum.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#ifdef ENABLE_LOGGING
#include "pybind11_log.h"
#endif

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

#ifdef USE_MOBSYA_ASEBA
template <> struct type_caster<Aseba::ChangedVariables::area> {

  PYBIND11_TYPE_CASTER(Aseba::NamedValue, io_name("tuple[int, list[int]]",
                                                  "tuple[int, list[int]]"));

  static handle cast(const Aseba::ChangedVariables::area &value,
                     return_value_policy /*policy*/, handle /*parent*/) {
    return py::make_tuple(value.start, value.variables).release();
  }
};
#endif

} // namespace detail
} // namespace pybind11

PYBIND11_MODULE(_client_impl, m) {

  py::options options;
  options.enable_function_signatures();

  py::classh<Event>(m, "Event", R"doc(
A named Aseba event
)doc")
      .def_readonly("source", &Event::source, R"doc(
Readonly

The id of the node that emitted the event.
)doc")
      .def_readonly("name", &Event::name, R"doc(
Readonly

The name of the event.
)doc")
      .def_readonly("data", &Event::data, R"doc(
Readonly

The payload of the event.
)doc")
      .def("__repr__", [](const Event &e) {
        return py::str("Event(source=") + py::str(py::cast(e.source)) +
               py::str(", name='") + py::cast(e.name) + py::str("', data=") +
               py::str(py::cast(e.data)) + py::str(")");
      });

  auto msgs = m.def_submodule("msgs", "Aseba messages");

  py::native_enum<DeviceInfoType>(msgs, "DeviceInfoType", "enum.IntEnum")
      .value("UUID", DeviceInfoType::DEVICE_INFO_UUID)
      .value("NAME", DeviceInfoType::DEVICE_INFO_NAME)
      .finalize();

  py::classh<Aseba::Message>(m, "Message", R"doc(
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
      .def_readonly("variables", &Aseba::Description::namedVariables)
      .def_readonly("events", &Aseba::Description::localEvents)
      .def_readonly("functions", &Aseba::Description::nativeFunctions)
      .def("__repr__", [](const Aseba::Description &msg) {
        // return py::str("Description(source=") + py::str(py::cast(msg.source))
        // +
        //        py::str(", name='") + py::cast(msg.name) +
        //        py::str("', protocol_version=") +
        //        py::str(py::cast(msg.protocolVersion)) + py::str(")");

        return py::str("Description(source=") + py::str(py::cast(msg.source)) +
               py::str(", name='") + py::cast(msg.name) +
               py::str("', protocol_version=") +
               py::str(py::cast(msg.protocolVersion)) +
               py::str(", variables=") + py::str(py::cast(msg.namedVariables)) +
               py::str(", events=") + py::str(py::cast(msg.localEvents)) +
               py::str(", functions=") +
               py::str(py::cast(msg.nativeFunctions)) + py::str(")");
      });

  py::classh<Aseba::NamedVariableDescription, Aseba::Message
             // , Aseba::TargetDescription::NamedVariable
             >(msgs, "NamedVariableDescription", R"doc(
)doc")
      .def(py::init<>())
      .def_readwrite("name", &Aseba::NamedVariableDescription::name)
      .def_readwrite("size", &Aseba::NamedVariableDescription::size)
      .def("__repr__", [](const Aseba::NamedVariableDescription &msg) {
        return py::str("NamedVariableDescription(source=") +
               py::str(py::cast(msg.source)) + py::str(", name='") +
               py::cast(msg.name) + py::str("', size=") +
               py::str(py::cast(msg.size)) + py::str(")");
      });

  py::classh<Aseba::LocalEventDescription, Aseba::Message
             // , Aseba::TargetDescription::LocalEvent
             >(msgs, "LocalEventDescription",
               R"doc(
)doc")
      .def(py::init<>())
      .def_readwrite("name", &Aseba::LocalEventDescription::name)
      .def_readwrite("description", &Aseba::LocalEventDescription::description)
      .def("__repr__", [](const Aseba::LocalEventDescription &msg) {
        return py::str("LocalEventDescription(source=") +
               py::str(py::cast(msg.source)) + py::str(", name='") +
               py::cast(msg.name) + py::str("', description='") +
               py::cast(msg.description) + py::str("')");
      });

  py::classh<Aseba::NativeFunctionDescription, Aseba::Message
             // , Aseba::TargetDescription::NativeFunction
             >(msgs, "NativeFunctionDescription", R"doc(
)doc")
      .def(py::init<>())
      .def_readwrite("name", &Aseba::NativeFunctionDescription::name)
      .def_readwrite("description",
                     &Aseba::NativeFunctionDescription::description)
      .def_readonly("parameters", &Aseba::NativeFunctionDescription::parameters)
      .def("__repr__", [](const Aseba::NativeFunctionDescription &msg) {
        return py::str("NativeFunctionDescription(source=") +
               py::str(py::cast(msg.source)) + py::str(", name='") +
               py::cast(msg.name) + py::str("', description='") +
               py::cast(msg.description) + py::str("', parameters=") +
               py::str(py::cast(msg.parameters)) + py::str(")");
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

#ifdef USE_MOBSYA_ASEBA
  py::classh<Aseba::GetNodeDescriptionFragment, Aseba::CmdMessage,
             Aseba::Message>(msgs, "GetNodeDescriptionFragment",
                             R"doc(
)doc")
      .def(py::init<int16_t, uint16_t>(), py::arg("fragment") = -1,
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::GetNodeDescriptionFragment &msg) {
        return py::str("GetNodeDescriptionFragment(source=") +
               py::str(py::cast(msg.source)) + py::str(", dest=") +
               py::str(py::cast(msg.dest)) + py::str(", fragment=") +
               py::str(py::cast(msg.m_fragment)) + py::str(")");
      });

  py::classh<Aseba::GetChangedVariables, Aseba::CmdMessage, Aseba::Message>(
      msgs, "GetChangedVariables",
      R"doc(
)doc")
      .def(py::init<uint16_t>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID))
      .def("__repr__", [](const Aseba::GetNodeDescriptionFragment &msg) {
        return py::str("GetChangedVariables(source=") +
               py::str(py::cast(msg.source)) + py::str(", dest=") +
               py::str(py::cast(msg.dest)) + py::str(")");
      });

  py::classh<Aseba::GetDeviceInfo, Aseba::CmdMessage, Aseba::Message>(
      msgs, "GetDeviceInfo",
      R"doc(
)doc")
      .def(py::init<uint16_t, DeviceInfoType>(),
           py::arg("dest") = static_cast<uint16_t>(ASEBA_DEST_INVALID),
           py::arg("info") = DEVICE_INFO_UUID)
      .def("__repr__", [](const Aseba::GetDeviceInfo &msg) {
        return py::str("GetDeviceInfo(source=") +
               py::str(py::cast(msg.source)) + py::str(", dest=") +
               py::str(py::cast(msg.dest)) + py::str(", info=") +
               py::str(py::cast(msg.info)) + py::str(")");
      });

  py::classh<Aseba::DeviceInfo, Aseba::Message>(msgs, "DeviceInfo",
                                                R"doc(
)doc")
      .def(py::init<DeviceInfoType, std::vector<uint8_t>>(), py::arg("info"),
           py::arg("data"))
      .def("__repr__", [](const Aseba::DeviceInfo &msg) {
        return py::str("GetDeviceInfo(source=") +
               py::str(py::cast(msg.source)) + py::str(", info=") +
               py::str(py::cast(msg.info)) + py::str(", data=") +
               py::str(py::cast(msg.data)) + py::str(")");
      });

  py::classh<Aseba::ChangedVariables, Aseba::Message>(msgs, "ChangedVariables",
                                                      R"doc(
)doc")
      .def(py::init<>())
      .def("__repr__", [](const Aseba::ChangedVariables &msg) {
        return py::str("ChangedVariables(source=") +
               py::str(py::cast(msg.source)) + py::str(", variables=") +
               py::str(py::cast(msg.variables)) + py::str(")");
      });
#endif

  py::classh<ClientNode>(m, "Description", R"doc(
The description of an Aseba node.
)doc")
      .def_readonly("name", &Aseba::TargetDescription::name, R"doc(
The name of the Aseba node (readonly).
)doc")
      .def_readonly("protocol_version",
                    &Aseba::TargetDescription::protocolVersion, R"doc(
The version of Aseba used by the node (readonly).
)doc")
      .def_property("variables", &ClientNode::get_variables, nullptr, R"doc(
The variables defined by the Aseba node as a dictionary
of ``(index, size)`` tuples keyed by name (readonly).
)doc")
      .def_property("local_events", &ClientNode::get_local_events, nullptr,
                    R"doc(
The local events defined by the Aseba node as a dictionary
of descriptions keyed by name (readonly).

Local events are locally emitted by the Aseba node and 
can only be accessed through an Aseba script running on the node.
)doc")
      .def_property("user_events", &ClientNode::get_user_events, nullptr, R"doc(
The user events defined in the script loaded on the Aseba node
as a dictionary of payload sizes keyed by name (readonly).

User events can be emitted and received by any Aseba node (including the client).
)doc")
      .def_property("functions", &ClientNode::get_functions, nullptr, R"doc(
The local functions defined by the Aseba node 
as a dictionary of ``(description, arguments)`` tuples keyed by name,
where each argument is a tuple ``(name, size)`` (readonly).

Local functions can be called through an Aseba script running on the node.
)doc")
      // .def_property("event_names", &ClientNode::get_event_names, nullptr)
      // .def_property("variable_names", &ClientNode::get_variable_names,
      // nullptr) .def_property("function_names",
      // &ClientNode::get_function_names, nullptr)
      .def("__repr__", [](const ClientNode &d) {
        return py::str("Description(name='") + py::cast(d.name) +
               py::str("', protocol_version=") +
               py::str(py::cast(d.protocolVersion)) + py::str(", variables=") +
               py::str(py::cast(d.get_variables())) +
               py::str(", local_events=") +
               py::str(py::cast(d.get_local_events())) +
               py::str(", user_events=") +
               py::str(py::cast(d.get_user_events())) +
               py::str(", functions=") + py::str(py::cast(d.get_functions())) +
               py::str(")");
      });
#if 0
      .def_property(
          "_variables_map",
          [](const ClientNode &desc) {
            unsigned i;
            return desc.getVariablesMap(i);
          },
          nullptr);
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
#endif
  options.disable_function_signatures();
  auto client = py::classh<Client>(m, "Client", R"doc(
Client(port: int = -1, address: str = "0.0.0.0", ping_period_ms: int = 1000, automatic_query: bool = True, node_disconnection_timeout_ms: int = 3000, min_protocol_version: int = ..., max_protocol_version: int = ...)


If port if positive and address is a valid IP4 address, it will listen for incoming connections. Other clients
will be able to connect the target at "tcp:host=<address>;port=<port>".
 
Args:
  port: The port. Pass a negative number to disable listening for incoming connections.
  address: If a valid IP address. Pass an empty to disable listening for incoming connections.
  ping_period_ms: The period in milliseconds to broadcast node discovery messages (:py:class:`pyaseba.client.msgs.ListNodes`).
        Set it to zero to disable node discovery.
  automatic_query: Whether to automatically query discovered nodes for their description.
        If selected, it will effectively call :py:meth:`query_description` 
        when a presence message from a new node is received. 
  node_disconnection_timeout_ms: The maximal interval to consider a node as disconnected. Only relevant when pinging the network.
  min_protocol_version: minimal Aseba protocol version that nodes must satisfy to interact with them.
  max_protocol_version: maximal Aseba protocol version that nodes must satisfy to interact with them.

Examples:
    The typical life-cycle of a client starts by creating it

    >>> client = Client()

    and then connecting to one or more networks.

    >>> connection = client.connect(...)
    1

    After interacting with the networks, 
    we should close the connection

    >>> client.close_connection(connection)
    True

    and/or close the client, which would also close all connections

    >>> client.close()

    Alternatively, we can use the client in a context that closes 
    it automatically when it exits.

    >>> with Client() as client:
            connection = client.connect(...)
            ...

)doc");
  client
      .def(py::init<int, const std::string &, unsigned, bool, unsigned,
                    unsigned, unsigned>(),
           py::kw_only(), py::arg("port") = -1, py::arg("address") = "0.0.0.0",
           py::arg("ping_period_ms") = 1000, py::arg("automatic_query") = true,
           py::arg("node_disconnection_timeout_ms") = 3000,
           py::arg("min_protocol_version") = ASEBA_MIN_TARGET_PROTOCOL_VERSION,
           py::arg("max_protocol_version") = ASEBA_MAX_TARGET_PROTOCOL_VERSION,
           R"doc(
__init__(self, port: int = -1, address: str = "0.0.0.0", ping_period_ms: int = 1000, automatic_query: bool = True, node_disconnection_timeout_ms: int = 3000, min_protocol_version: int = ..., max_protocol_version: int = ...) -> None

Constructs an instance.
)doc")
      .def("connect", &Client::connect_and_start_kwargs, py::arg("target"),
           py::arg("wait_ms") = 1000, py::arg("max_retries") = 3,
           R"DOC(
connect(self, target: str, wait_ms: int = 1000, max_retries: int = 3, **kwargs: Any) -> int

Connects to an Aseba network.

Args:
  target: a valid `Dashel target <https://aseba-community.github.io/dashel/>`_
  wait_ms: time to wait before retrying to connect in case of failure.
  max_retries: maximal number of time to try to connect before returning a failure.
  **kwargs: parameters that are appended to ``target`` as ``"<key>=<value>"``. 
    For example, if target is ``"tcp"``, passing ``port=33333`` 
    will result in a target ``"tcp:port=33333"``.
Returns:
  The positive index of the connected network in case of success, or ``0`` in case of failure.
)DOC")
      .def("close_connection", &Client::close, py::arg("connection"),
           py::arg("wait_ms") = 1000, R"DOC(
close_connection(self, connection: int, wait_ms: int = 1000) -> bool

Closes a connection.

Args:
  connection: a connection
  wait_ms: time to wait for the connection to close.
Returns:
  Whether the connection has been closed.
)DOC")
      .def("close", &Client::stop_and_close, R"DOC(
close(self) -> None

Closes all connections.
)DOC")

      .def("_connect", &Client::try_to_connect_, py::arg("target"),
           py::pos_only(), R"DOC(
_connect(self, target: str) -> int

Tries to connect to a target once, without starting the client.

Args:
  target: a valid `Dashel target <https://aseba-community.github.io/dashel/>`_
Returns:
  The positive index of the connected network in case of success, or ``0`` in case of failure.
)DOC")
      .def("_start", &Client::start, R"DOC(
_start(self) -> None

Starts the client.
)DOC")
      .def("_stop", &Client::stop, R"DOC(
_stop(self) -> None

Stops the client.
)DOC")
      // .def_readwrite("message_callbacks", &Client::message_callbacks,
      // py::return_value_policy::reference)
      .def("__enter__", [](Client &client) { return &client; })
      .def("__exit__",
           [](Client &client, py::args args) { client.stop_and_close(); })
      .def("wait_connection", &Client::wait_target_connection,
           py::arg("connection") = 0, py::arg("wait_ms") = 1000,
           py::arg("callback") = nullptr, R"DOC(
wait_connection(self, connection: int = 0, wait_ms: int = 1000, callback: Callable[[int], None] = None) -> tuple[int, str]

Waits for a new connection.

Args:
  connection: the connection to wait. Set it to ``-1`` to wait for the any connection.
  wait_ms: the maximal time in ms to wait for a connection.
  callback: An optional callback called when a connection is established.
            It receives the connection as argument.
Returns:
  The positive index of the new connection or ``0`` in case of timeouts.
)DOC")
      .def("wait_disconnection", &Client::wait_target_disconnection,
           py::arg("connection") = 0, py::arg("wait_ms") = 1000,
           py::arg("callback") = nullptr, R"DOC(
wait_disconnection(self, connection: int = 0, wait_ms: int = 1000, callback: Callable[[int], None] = None) -> tuple[int, str]

Waits for a disconnection.

Args:
  connection: the connection to wait to disconnect. Set it to ``-1`` to wait for the any disconnection.
  wait_ms: the maximal time in ms to wait for a disconnection.
  callback: An optional callback called when a connection is closed. 
            It receives the connection as argument.
Returns:
  The positive index of the connection closed or ``0`` in case of timeouts.
)DOC")
      .def("add_connection_callback", &Client::add_target_connection_callback,
           py::arg("callback"),
           R"DOC(
add_connection_callback(self, callback: Callable[[int, str], None]) -> None

Adds a callback called when a connection is opened.

Args:
  callback: The callback that receives the connection as argument.
)DOC")
      .def("add_disconnection_callback",
           &Client::add_target_disconnection_callback, py::arg("callback"),
           R"DOC(
add_disconnection_callback(self, callback: Callable[[int, str], None]) -> None

Adds a callback called when a connection is closed.

Args:
  callback: The callback that receives the connection as argument.
)DOC")
      .def("wait_nodes", &Client::wait_nodes,
           py::arg("node_ids") = std::set<uint16_t>(), py::arg("number") = -1,
           py::arg("wait_ms") = 1000, py::arg("callback") = nullptr,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
wait_nodes(self, node_ids: set[int] = set(), number: int = -1, wait_ms: int = 1000, callback: Callable[[int, int, bool], None] | None = None, include: set[int] = set(), exclude: set[int] = set()) -> dict[int, set[int]]

Waits until nodes are discovered. 

Nodes are considered as discovered when their complete description is received.

Args:
  node_ids: A set of ids. If not empty, only nodes with ids in this set will be considered as discovered.
  wait_ms: The maximal time in milliseconds to wait.
  number: The number of nodes to discover.
  callback: An optional callback, called each time a node is discovered. 
           It receives a three arguments ``(node_id, connection, complete)`` where
           complete is `True` only if ``number`` nodes have been discovered.
  include: If not empty, restricts the search to nodes on the networks specified in this set.
  exclude: Ignore nodes on networks specified in this set.

Returns:
  A dictionary with the discovered node ids indexed by connection.
)DOC")
      .def("wait_node", &Client::wait_node_connection, py::arg("node_id") = -1,
           py::arg("wait_ms") = 1000, py::arg("callback") = nullptr,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
wait_node(self, node_id: int = -1, wait_ms: int = 1000, callback: Callable[[int, int], None] | None = None, include: set[int] = set(), exclude: set[int] = set()) -> tuple[int, int]

Waits until one node is discovered. 

Nodes are considered as discovered when their complete description is received.

Args:
  node_id: The id of the node to wait. If negative, it will match any node id.
  wait_ms: The maximal time in milliseconds to wait.
  callback: An optional callback, called each time a node is discovered. 
           It receives a two arguments ``(node_id, connection)``.
  include: If not empty, restricts the search to nodes on the networks specified in this set.
  exclude: Ignore nodes on networks specified in this set.

Returns:
  A tuple ``(node_id, connection)``. If it fails discovering a node, it returns ``(0, 0)``.
  Else ``connection`` will be strictly positive.
)DOC")
      .def("wait_node_disconnection", &Client::wait_node_disconnection,
           py::arg("node_id") = -1, py::arg("wait_ms") = 1000,
           py::arg("callback") = nullptr,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
wait_node_disconnection(self, node_id: int = -1, wait_ms: int = 1000, callback: Callable[[int, int], None] | None = None, include: set[int] = set(), exclude: set[int] = set()) -> tuple[int, int]

Waits until one node is discovered. 

Nodes are considered as discovered when their complete description is received.

Args:
  node_id: The id of the node to wait. If negative, it will match any node id.
  wait_ms: The maximal time in milliseconds to wait.
  callback: An optional callback, called each time a node is discovered. 
           It receives a two arguments ``(node_id, connection)``.
  include: If not empty, restricts the search to nodes on the networks specified in this set.
  exclude: Ignore nodes on networks specified in this set.

Returns:
  A tuple ``(node_id, connection)``. If the node has not disconnected, it returns ``(0, 0)``.
  Else ``connection`` will be strictly positive.
)DOC")
      .def("get_node_ids", &Client::get_node_ids,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{},
           py::arg("connected") = true, R"DOC(
get_node_ids(self, include: set[int] = set(), exclude: set[int] = set(), connected: bool = True) -> dict[int, set[int]]

Gets the discovered node ids.

Args:
  include: If not empty, restricts the search to nodes on the networks specified in this set.
  exclude: Ignore nodes on networks specified in this set.
  connected: An optional criterion for the current connection state of the node.

Returns:
  A dictionary with discovered node ids indexed by connection.
)DOC")
      .def("has_node", &Client::has_node, py::arg("node_id"),
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{},
           py::arg("connected") = true, R"DOC(
has_node(self, node_id: int, include: set[int] = set(), exclude: set[int] = set(), connected: bool = True) -> bool

Checks if a node is known.

Args:
  node_id: The id of the node.  
  include: If not empty, restricts the search to nodes on the networks specified in this set.
  exclude: Ignore nodes on networks specified in this set.
  connected: An optional criterion for the current connection state of the node.
Returns:
  True if a corresponding node is known.
)DOC")
      .def("get_descriptions", &Client::get_nodes,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{},
           py::arg("connected") = true, R"DOC(
get_descriptions(self, include: set[int] = set(), exclude: set[int] = set(), connected: bool = True) -> dict[int, dict[int, Description]]

Gets the discovered node descriptions.

Args:
  include: If not empty, restricts the search to nodes on the networks specified in this set.
  exclude: Ignore nodes on networks specified in this set.
  connected: An optional criterion for the current connection state of the node.

Returns:
  A dictionary with discovered node descriptions indexed by connection.
)DOC")
      .def("get_description", &Client::get_node, py::arg("node_id"),
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{},
           py::arg("connected") = true, py::return_value_policy::copy,
           R"DOC(
get_description(self, node_id: int, include: set[int] = set(), exclude: set[int] = set(), connected: bool = True) -> Description | None

Gets a node description.

Args:
  node_id: The id of the node to wait. If negative, it will match any node id.
  include: If not empty, restricts the search to nodes on the networks specified in this set.
  exclude: Ignore nodes on networks specified in this set.
  connected: An optional criterion for the current connection state of the node.

Returns:
  The node description or ``None`` is no suitable node was found. 
)DOC")
      .def("scan", &Client::scan, py::arg("number") = -1,
           py::arg("wait_ms") = 1000, py::arg("callback") = nullptr, R"DOC(
scan(self, number: int = -1, wait_ms: int = 1000, callback: Callable[[int, int, bool], None]| None = None) -> dict[int, set[int]]

Scans for nodes on all connected networks.

Args:
  number: The minimal number of nodes to find before returning.
  wait_ms: the maximal time in ms to wait.
  callback: An optional callback called each time a node is found.
            It receives a three arguments ``(node_id, connection, complete)`` where
            complete is `True` only if ``number`` nodes have been found.
Returns:
  A dictionary of node ids indexed by connection.
)DOC")
      .def("ping", &Client::send_message_of_type<Aseba::ListNodes>,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
ping(self) -> None

Broadcast a :py:class:`pyaseba.client.msgs.ListNodes` message on all connected networks.

Args:
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("query_description", &Client::query_description, py::arg("node_id"),
           py::arg("wait_ms") = 1000, py::arg("callback") = nullptr,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{},
           py::return_value_policy::copy, R"DOC(
query_description(self, node_id: int, wait_ms: int = 1000, callback: Callable[[Description], None]| None = None, include: set[int] = set(), exclude: set[int] = set()) -> Description | None

Queries the description of a node.

Args:
  node_id: The id of the node.
  wait_ms: The maximal time in milliseconds to wait.
  callback: An optional callback called after the description is received.  
  include: If not empty, restricts the search to nodes on the networks specified in this set.
  exclude: Ignore nodes on networks specified in this set.
Returns:
  A description or ``None`` if not received in time.
)DOC")
      .def("add_node_callback", &Client::add_node_connection_callback,
           py::arg("callback"), R"DOC(
add_node_callback(self, callback: Callable[[int, int], None]) -> None

Adds a callback called called when a node is discovered. 

Args:
  callback: A callback that receives a two arguments ``(node_id, connection)``.

)DOC")
      .def("add_node_disconnection_callback",
           &Client::add_node_disconnection_callback, py::arg("callback"),
           R"DOC(
add_node_disconnection_callback(self, callback: Callable[[int, int], None]) -> None

Adds a callback called when a node is disconnected. 

Args:
  callback: A callback that receives a two arguments ``(node_id, connection)``.

)DOC")
      .def("add_message_callback", &Client::add_message_callback,
           py::arg("callback"), R"DOC(
add_message_callback(self, callback: Callable[[Message, int], None]) -> None

Adds a callback called when a message is received. 

Args:
  callback: A callback that receives a two arguments ``(message, connection)``.

)DOC")
      .def("remove_message_callback", &Client::remove_message_callback,
           py::arg("index"), R"DOC(
remove_message_callback(self, index: int) -> None

Removes a message callback. 

Args:
  index: The index of the callback.

)DOC")
      .def("clear_message_callbacks", &Client::clear_message_callbacks, R"DOC(
clear_message_callbacks(self) -> None

Clears all message callbacks. 

)DOC")
      .def("clear_incoming_messages", &Client::clear_in_msgs, R"DOC(
clear_incoming_messages(self) -> None

Deletes all incoming messages. 

)DOC")
      .def("get_message", &Client::get_message, py::arg("node_id") = -1,
           py::arg("type") = -1, py::arg("wait_ms") = 1000,
           py::arg("callback") = nullptr,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, py::arg("pause") = false,
           R"DOC(
get_message(self, node_id: int = -1, type: int = -1, wait_ms: int = 1000, callback: Callable[[Message, int], None]| None = None, include: set[int] = set(), exclude: set[int] = set(), pause: bool = false) -> tuple[Message | None, int]

Wait until a message is received.

Args:
  node_id: The id of the node.
  wait_ms: The maximal time in milliseconds to wait.
  callback: An optional callback called after the message is received.  
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
  pause: Whether to stop processing once a message is received.
Returns:
  A message or ``None`` if not received in time.
)DOC")
      .def("send_user_message", &Client::send_user_message, py::arg("type"),
           py::arg("payload") = Aseba::VariablesDataVector(),
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
send_user_message(self, type: int = -1, payload: Sequence[int] = (), include: set[int] = set(), exclude: set[int] = set()) -> Description | None

Send a message of arbitrary type.

Args:
  type: The type of the Aseba message
  payload: The payload of the message
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("send_message", &Client::send_message, py::arg("message"),
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
send_message(self, message: Message, include: set[int] = set(), exclude: set[int] = set()) -> Description | None

Send a message.

Args:
  message: The message.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("add_event_callback", &Client::add_event_callback,
           py::arg("callback"), R"DOC(
add_event_callback(self, callback: Callable[[Event], None]) -> None

Adds a callback called when an event is received. 

Args:
  callback: A callback.

)DOC")
      .def("get_event", &Client::get_event, py::arg("node_id") = -1,
           py::arg("name") = "", py::arg("wait_ms") = 1000,
           py::arg("callback") = nullptr,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
get_event(self, node_id: int, name: str, wait_ms: int = 1000, callback: Callable[[Event], None]| None = None, include: set[int] = set(), exclude: set[int] = set()) -> Description | None

Wait until an event is received.

Args:
  node_id: The id of the node.
  name: The name of the event.
  wait_ms: The maximal time in milliseconds to wait.
  callback: An optional callback called after the event is received.  
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
Returns:
  A message or ``None`` if not received in time.
)DOC")
      .def("emit_event", &Client::emit_event, py::arg("node_id"),
           py::arg("name"), py::arg("payload") = Aseba::VariablesDataVector(),
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
emit_event(self, node_id: int, name: str, *, payload: Sequence[int] = (), include: set[int] = set(), exclude: set[int] = set()) -> Description | None

Send a message.

Args:
  node_id: The id of the node defining the event.
  name: The name of the event.
  payload: The payload of the event
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("send_event", &Client::send_event, py::arg("type"),
           py::arg("payload") = Aseba::VariablesDataVector(),
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
send_event(self, type: int, *, payload: Sequence[int] = (), include: set[int] = set(), exclude: set[int] = set()) -> Description | None

Send an event of arbitrary type. Same as :py:meth:`send_user_message`.

Args:
  type: The type of the event.
  payload: The payload of the event
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("cmd_run", &Client::send_message_of_type<Aseba::Run, uint16_t>,
           py::arg("node_id"), py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
cmd_run(self, node_id: int, include: set[int] = set(), exclude: set[int] = set()) -> None

Sends a command to a node to start running

Args:
  node_id: The id of the node.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("cmd_stop", &Client::send_message_of_type<Aseba::Stop, uint16_t>,
           py::arg("node_id"), py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
cmd_stop(self, node_id: int, include: set[int] = set(), exclude: set[int] = set()) -> None

Sends a command to a node to stop running

Args:
  node_id: The id of the node.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("cmd_pause", &Client::send_message_of_type<Aseba::Pause, uint16_t>,
           py::arg("node_id"), py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
cmd_pause(self, node_id: int, include: set[int] = set(), exclude: set[int] = set()) -> None

Sends a command to a node to pause

Args:
  node_id: The id of the node.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("cmd_reset", &Client::send_message_of_type<Aseba::Reset, uint16_t>,
           py::arg("node_id"), py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
cmd_reset(self, node_id: int, include: set[int] = set(), exclude: set[int] = set()) -> None

Sends a command to a node to reset

Args:
  node_id: The id of the node.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("cmd_sleep", &Client::send_message_of_type<Aseba::Sleep, uint16_t>,
           py::arg("node_id"), py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
cmd_sleep(self, node_id: int, include: set[int] = set(), exclude: set[int] = set()) -> None

Sends a command to a node to sleep

Args:
  node_id: The id of the node.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("get_variable", &Client::get_variable, py::arg("node_id"),
           py::arg("name"), py::arg("wait_ms") = 1000,
           py::arg("callback") = nullptr,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
get_variable(self, node_id: int, name: str, wait_ms: int = 1000, callback: Callable[[list[int]], None] | None = None, include: set[int] = set(), exclude: set[int] = set()) -> list[int]

Query a node for the value of a variable by name.

Args:
  node_id: The id of the node.
  name: The name of the variable.
  callback: An optional callback called when the value is received.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("get_variable_by_index", &Client::get_variable_at_index,
           py::arg("node_id"), py::arg("index"), py::arg("length"),
           py::arg("wait_ms") = 1000, py::arg("callback") = nullptr,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
get_variable_by_index(self, node_id: int, index: int, length: int, wait_ms: int = 1000, callback: Callable[[list[int]], None] | None = None, include: set[int] = set(), exclude: set[int] = set()) -> list[int]

Query a node for the value of a variable by index and length

Args:
  node_id: The id of the node.
  index: The index of the variable.
  length: The size of the variable.
  callback: An optional callback called when the value is received.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("get_all_variables", &Client::get_variables, py::arg("node_id"),
           py::arg("wait_ms") = 1000, py::arg("callback") = nullptr,
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
get_all_variables(self, node_id: int, wait_ms: int = 1000, callback: Callable[[list[int]], None] | None = None, include: set[int] = set(), exclude: set[int] = set()) -> dict[str, list[int]]

Query a node for the value of all variables

Args:
  node_id: The id of the node.
  callback: An optional callback called when the value is received.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("set_variable", &Client::set_variable, py::arg("node_id"),
           py::arg("name"), py::arg("value"),
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
set_variable(self, node_id: int, name: str, value: Sequence[int], wait_ms: int = 1000, include: set[int] = set(), exclude: set[int] = set()) -> None

Set the value of a variable by name

Args:
  node_id: The id of the node.
  name: The name of the variable.
  value: The value.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("set_variable_by_index", &Client::set_variable_at_index,
           py::arg("node_id"), py::arg("index"), py::arg("value"),
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
set_variable_by_index(self, node_id: int, index: int, value: Sequence[int], wait_ms: int = 1000, include: set[int] = set(), exclude: set[int] = set()) -> None

Set the value of a variable by index.

Args:
  node_id: The id of the node.
  index: The index of the variable.
  value: The value.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.
)DOC")
      .def("load_script", &Client::load_script, py::arg("node_id"),
           py::arg("script"), py::arg("events") = std::map<std::wstring, int>(),
           py::arg("constants") = std::map<std::wstring, int>(),
           py::arg("include") = std::set<unsigned>{},
           py::arg("exclude") = std::set<unsigned>{}, R"DOC(
load_script(self, node_id: int, script: str, events: dict[str, int] = {}, constants: dict[str, int] = {}, include: set[int] = set(), exclude: set[int] = set()) -> None

Set the value of a variable by index.

Args:
  node_id: The id of the node.
  script: The Aseba code.
  events: A dictionary of {name -> payload size} defining each event in the script.
  constants: A dictionary of {name -> value} defining each constant in the script.
  include: If not empty, restricts to networks specified in this set.
  exclude: Ignores networks specified in this set.

Raises:
  RuntimeError: when it fails to compile the script.
)DOC")
      .def("advertise", &Client::advertise, py::arg("name") = "",
           py::arg("nodes") = std::vector<AdvertisedNode>(),
           py::arg("protocol_version") = ASEBA_PROTOCOL_VERSION, R"DOC(
advertise(self, name: str = "", nodes: Sequence[tuple[int, int, str]] = (), protocol_version: int = ...) -> None

Advertise with zeroconf.

Args:
  name: The name of the zeroconf record. If empty, it defaults to `"pyaseba <port>"`
  nodes: A list of `(id, pid, name)` node tuples.
  protocol_version: The Aseba protocol version.
)DOC")
      .def("advertise_nodes", &Client::advertise_nodes, py::arg("name") = "",
           py::arg("specs") =
               std::map<std::string, std::tuple<std::string, unsigned>>(
                   {{"thymio-II", {"Thymio II", 8}}}),
           py::arg("query_product_id") = false,
           py::arg("protocol_version") = ASEBA_PROTOCOL_VERSION, R"DOC(
advertise(self, name: str = "", specs: dict[str, tuple[str, int]] = {"thymio-II: ("Thymio II", 8)}, query_product_id: bool = false) -> None

Advertise all nodes with zeroconf.

Args:
  name: The name of the zeroconf record. If empty, it defaults to `"pyaseba <port>"`
  specs: An optional mapping between node description names and a tuple with name and product id.
  query_product_id: Whether to query the product id. If not set and no key is provided in ``specs``, 
                    the product id will default to ``0``.
  protocol_version: The Aseba protocol version.
)DOC")
      .def("deadvertise", &Client::deadvertise, py::arg("name") = "",
           R"DOC(
deadvertise(self, name: str = "") -> None

De-advertise with zeroconf.

Args:
  name: The name of the zeroconf record. If empty, it defaults to `"pyaseba <port>"`
)DOC");
  m.def("scan_serial_ports", &Dashel::SerialPortEnumerator::getPorts, R"DOC(
scan_serial_ports() -> dict[int, tuple[str, str]]

Lists the serial ports on this device.

Args:
  A list of ``(device, description)`` tuples.
)DOC");

  m.def("complete_target", &Client::complete_target, R"DOC(
complete_target(target: str, **kwargs: Any) -> str

Add parameters to a Dashel target.

Args:
  **kwargs: parameters that are appended to ``target`` as ``"<key>=<value>"``. 
            For example, if target is ``"tcp"``, passing ``port=33333`` 
            will result in a target ``"tcp:port=33333"``.

Returns:
  The dashel target

)DOC");

  options.enable_function_signatures();
  client
      .def_readwrite("automatic_query", &Client::query, R"DOC(
Readonly

Whether to automatically query discovered nodes for their description.
If set, it will effectively call :py:meth:`query_description` 
when a presence message from a new node is received. 
)DOC")
      .def_property("node_disconnection_timeout_ms",
                    &Client::get_disconnection_timeout_ms,
                    &Client::set_disconnection_timeout_ms, R"DOC(
The maximal interval to consider a node as disconnected. Only relevant when pinging the network.
)DOC")
      .def_property(
          "is_connected", [](Client &c) { return c.streams.is_connected(); },
          nullptr, R"DOC(
Readonly

Whether at least the client has at least one open connection.
)DOC")
      .def_readonly("port", &Client::port, R"DOC(
Readonly

Whether at least the client has at least one open connection.
)DOC")
      .def_property(
          "connections",
          [](Client &client) { return client.streams.get_target_names(); },
          nullptr,
          R"DOC(
Readonly

A dictionary of connected dashel target indexed by connection.
)DOC")
      .def_property("ping_period_ms", &Client::get_ping_period_ms,
                    &Client::set_ping_period_ms, R"DOC(
The period in milliseconds to broadcast node discovery messages (:py:class:`pyaseba.client.msgs.ListNodes`).
Set it to zero to disable node discovery. 
)DOC")
      .def_property(
          "node_ids", [](Client &client) { return client.get_node_ids(); },
          nullptr, R"DOC(
Readonly

A dictionary with all discovered node ids indexed by connection.
)DOC")
      .def_property(
          "descriptions", [](Client &client) { return client.get_nodes(); },
          nullptr,
          R"DOC(
Readonly

A dictionary with all discovered node descriptions indexed by connection.
)DOC")
      .def_property("pause_processing", &Client::get_processing_paused,
                    &Client::set_processing_paused, R"DOC(
Whether incoming messages are processed or kept in the queue.
)DOC")
      //       .def_property("pause_sending", &Client::get_sending_paused,
      //                     &Client::set_sending_paused, R"DOC(
      // Whether outgoing messages are processed or kept in the queue.
      // )DOC")
      .def_property(
          "_is_running", [](const Client &m) { return !m.stopped; }, nullptr);

  m.def("_init_logger", []() {
#ifdef ENABLE_LOGGING
    spdlog::set_level(spdlog::level::debug);
    pybind11_log::init_mt("pyaseba");
#else
        std::cerr << "pyaseba was built without logging support" << std::endl;
#endif
  });

  m.def(
      "_set_logger_level",
      [](const std::string &level) {
#ifdef ENABLE_LOGGING
        spdlog::set_level(spdlog::level::from_str(level));
#else
        std::cerr << "pyaseba was built without logging support" << std::endl;
#endif
      },
      py::arg("level"));

  m.def(
      "supports_logging",
      []() {
#ifdef ENABLE_LOGGING
        return true;
#else
        return false;
#endif
      },
      R"doc(
supports_logging() -> bool

Returns whether pyaseba was built with logging support.


Returns:
  True if pyaseba supports logging.
)doc");

  m.def(
      "uses_mobsya_aseba",
      []() {
#ifdef USE_MOBSYA_ASEBA
        return true;
#else
        return false;
#endif
      },
      R"doc(
uses_mobsya_aseba() -> bool

Returns whether pyaseba was built against the Mobsya version
of Aseba.


Returns:
  True if pyaseba uses Mobsya's Aseba.
)doc");
}