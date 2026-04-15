#ifndef DESCRIPTION_H
#define DESCRIPTION_H

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <list>
#include <tuple>
#include <vector>

#include "aseba/common/consts.h"
#include "aseba/common/msg/TargetDescription.h"
#include "aseba/common/productids.h"
#include "aseba/vm/natives.h"

#include "utils.h"

struct Description {

  using Variables = std::vector<std::tuple<uint16_t, std::string>>;
  using VariablesMap = std::map<std::string, std::tuple<int, int>>;
  using EventsMap = std::map<std::string, std::string>;
  using FunctionArgument = std::tuple<std::string, int16_t>;
  using Function = std::tuple<std::string, std::vector<FunctionArgument>>;
  using FunctionsMap = std::map<std::string, Function>;

  inline static Variables default_variables = {
      {1, "id"},
      {1, "source"},
      {32, "args"},
      {1, ASEBA_PID_VAR_NAME},
  };

  explicit Description(const std::string &name = "node",
                       bool add_default_variables = true,
                       bool add_default_functions = true)
      : _variables(), _variables_map(), _variables_size(0),
        _events({{nullptr, nullptr}}), _events_map(), _functions({nullptr}),
        _allocated_functions(), _functions_map(), _desc(nullptr), _name(name),
        _names() {

    if (add_default_variables) {
      _variables = default_variables;
      for (const auto &[s, n] : default_variables) {
        _variables_map[n] = {_variables_size, s};
        _variables_size += s;
      }
    }
    if (add_default_functions) {
      _functions = {ASEBA_NATIVES_STD_DESCRIPTIONS, nullptr};
      for (const auto &f : _functions) {
        if (f) {
          std::vector<FunctionArgument> args;
          for (auto arg = f->arguments; arg->size && arg->name; arg++) {
            args.emplace_back(std::string(arg->name), arg->size);
          }
          _functions_map[std::string(f->name)] = {std::string(f->doc), args};
        }
      }
    }
  }

  const Variables &get_variables() const { return _variables; }
  const VariablesMap &get_variables_map() const { return _variables_map; }

  const AsebaVMDescription *get_description() {
    // std::cout << "AsebaVMDescription::get_description\n";
    if (!_desc) {
      _desc = (AsebaVMDescription *)malloc(
          sizeof(AsebaVMDescription) +
          (1 + _variables.size()) * sizeof(AsebaVariableDescription));
      assert(_desc);
      _desc->name = _name.c_str();
      auto var = _desc->variables;
      for (const auto &[size, name] : _variables) {
        var->size = size;
        var->name = name.c_str();
        var++;
      }
      var->size = 0;
      var->name = nullptr;
    }
    return _desc;
  }

  const AsebaLocalEventDescription *get_events() const {
    return _events.data();
  }

  const AsebaNativeFunctionDescription *const *get_functions() const {
    return _functions.data();
  }

  const Aseba::TargetDescription get_target_description() const {
    Aseba::TargetDescription d;
    d.name = widen(_name);
    d.bytecodeSize = BYTECODE_SIZE;
    d.variablesSize = VARIABLES_TOTAL_SIZE;
    d.stackSize = STACK_SIZE;
    for (const auto &[size, name] : _variables) {
      if (size) {
        d.namedVariables.emplace_back(widen(name), size);
      }
    }
    for (const auto &[name, desc] : _events) {
      if (name) {
        d.localEvents.emplace_back(
            Aseba::TargetDescription::LocalEvent{widen(name), widen(desc)});
      }
    }
    auto f = get_functions();
    while (*f) {
      std::vector<Aseba::TargetDescription::NativeFunctionParameter> parameters;
      auto a = (*f)->arguments;
      while (a->name) {
        parameters.emplace_back(widen(a->name), a->size);
        a++;
      }
      d.nativeFunctions.emplace_back(Aseba::TargetDescription::NativeFunction{
          widen((*f)->name), widen((*f)->doc), parameters});
      f++;
    }
    return d;
  }

  const std::string &get_name() const { return _name; }
  unsigned get_protocol_version() const { return ASEBA_PROTOCOL_VERSION; }

  const EventsMap &get_events_map() const { return _events_map; }

  const FunctionsMap &get_functions_map() const { return _functions_map; }

  void set_name(const std::string &name) { _name = name; }

  void add_variable(const std::string &name, uint16_t size) {
    _variables_size += size;
    _variables_map[name] = {_variables_size, size};
    _variables.emplace_back(size, name);
  }

  void add_event(const std::string &name, const std::string &description) {
    _events.insert(std::end(_events) - 1, {c_str(name), c_str(description)});
    _events_map[name] = description;
  }

  void add_function(const std::string &name, const Function &fun) {
    _functions_map[name] = fun;
    const auto &[doc, args] = fun;
    AsebaNativeFunctionDescription *desc =
        (AsebaNativeFunctionDescription *)malloc(
            sizeof(AsebaNativeFunctionDescription) +
            (1 + args.size()) * sizeof(AsebaNativeFunctionArgumentDescription));
    desc->name = c_str(name);
    desc->doc = c_str(doc);
    auto arg = desc->arguments;
    for (const auto &[n, size] : args) {
      arg->size = size;
      arg->name = c_str(n);
      arg++;
    }
    arg->size = 0;
    arg->name = nullptr;
    _functions.insert(_functions.end() - 1, desc);
    _allocated_functions.push_back(desc);
  }

  ~Description() {
    for (auto p : _allocated_functions) {
      free((void *)p);
    }
    if (_desc) {
      free((void *)_desc);
    }
  }

private:
  const char *c_str(const std::string &value) {
    _names.push_back(value);
    return _names.back().c_str();
  }

  Variables _variables;
  VariablesMap _variables_map;
  unsigned _variables_size;
  std::vector<AsebaLocalEventDescription> _events;
  EventsMap _events_map;
  std::vector<const AsebaNativeFunctionDescription *> _functions;
  std::vector<const AsebaNativeFunctionDescription *> _allocated_functions;
  FunctionsMap _functions_map;
  AsebaVMDescription *_desc;
  std::string _name;
  std::list<std::string> _names;
};

#endif // DESCRIPTION_H
