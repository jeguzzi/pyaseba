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

  inline static Variables default_variables = {
      {1, "id"},
      {1, "source"},
      {32, "args"},
      {1, ASEBA_PID_VAR_NAME},
  };

  Description()
      : _variables(default_variables), _events({{nullptr, nullptr}}),
        _functions({ASEBA_NATIVES_STD_DESCRIPTIONS, nullptr}),
        _allocated_functions(), _desc(nullptr), _names() {}

  const AsebaVMDescription *get_description() {
    std::cout << "AsebaVMDescription::get_description\n";
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

  void set_name(const std::string &name) { _name = name; }

  void add_variable(const std::string &name, uint16_t size) {
    _variables.emplace_back(size, name);
  }

  void add_event(const std::string &name, const std::string &description) {
    _events.insert(std::end(_events) - 1, {c_str(name), c_str(description)});
  }

  void add_function(const std::string &name, const std::string &description,
                    const std::vector<std::tuple<int16_t, std::string>> &args) {
    AsebaNativeFunctionDescription *desc =
        (AsebaNativeFunctionDescription *)malloc(
            sizeof(AsebaNativeFunctionDescription) +
            (1 + args.size()) * sizeof(AsebaNativeFunctionArgumentDescription));
    desc->name = c_str(name);
    desc->doc = c_str(description);
    auto arg = desc->arguments;
    for (const auto &[size, n] : args) {
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
  std::vector<AsebaLocalEventDescription> _events;
  std::vector<const AsebaNativeFunctionDescription *> _functions;
  std::vector<const AsebaNativeFunctionDescription *> _allocated_functions;
  AsebaVMDescription *_desc;
  std::string _name;
  std::list<std::string> _names;
};

#endif // DESCRIPTION_H
