#ifndef NODE_H
#define NODE_H

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>
#include <set>
#include <tuple>
#include <valarray>
#include <vector>

#include "aseba/common/consts.h"
#include "aseba/common/msg/TargetDescription.h"
#include "aseba/common/productids.h"
#include "aseba/common/utils/FormatableString.h"
#include "aseba/common/utils/utils.h"
#include "aseba/transport/buffer/vm-buffer.h"
#include "aseba/vm/natives.h"
#include "aseba/vm/vm.h"

#include "description.h"
#include "utils.h"

class Node {

  std::valarray<unsigned short> bytecode;
  std::valarray<signed short> stack;
  static inline std::vector<AsebaNativeFunctionPointer> native_functions = {
      ASEBA_NATIVES_STD_FUNCTIONS};

protected:
  int16_t variables[VARIABLES_TOTAL_SIZE];
  std::array<uint8_t, 16> uuid;
  std::string friendly_name;
  std::set<void *> sent_device_info;
  bool initialized;

public:
  std::string name;
  AsebaVMState vm;
  Description description;

  uint16_t lastMessageSource;
  std::valarray<uint8_t> lastMessageData;

  // Aseba::UnifiedTime lastTime;
  // name -> (pointer, size)
  std::map<std::string, std::pair<int16_t *, unsigned int>> named_variable;
  int16_t *next_variable;
  // name -> id
  std::map<std::string, uint16_t> named_event;
  // [<name, argument sizes>]
  std::vector<std::tuple<std::string, std::vector<int>, size_t>> functions;
  const bool has_native_functions;

public:
  explicit Node(unsigned node_id, const std::string &_name,
                bool default_variables = true, bool default_functions = true,
                const std::array<uint8_t, 16> &uuid_ = {},
                const std::string &friendly_name_ = "")
      : uuid(uuid_), friendly_name(friendly_name_), sent_device_info(),
        initialized(false), name(_name),
        description(name, default_variables, default_functions),
        lastMessageSource(0), lastMessageData(),
        has_native_functions(default_functions) {
    // setup variables
    vm.nodeId = static_cast<uint16_t>(node_id);
    bytecode.resize(BYTECODE_SIZE);
    vm.bytecode = &bytecode[0];
    vm.bytecodeSize = static_cast<uint16_t>(bytecode.size());
    stack.resize(STACK_SIZE);
    vm.stack = &stack[0];
    vm.stackSize = static_cast<uint16_t>(stack.size());
    vm.variables = reinterpret_cast<int16_t *>(&variables);
    vm.variablesSize = sizeof(variables) / sizeof(int16_t);
    AsebaVMInit(&vm);
    vm.flags = ASEBA_VM_STEP_BY_STEP_MASK;
    variables[ID] = static_cast<int16_t>(vm.nodeId);
    next_variable = variables;
    description.set_name(name);
  }

  unsigned get_node_id() const { return vm.nodeId; }

  virtual ~Node() = default;

  virtual void init() {
    if (initialized)
      return;
    initialized = true;
    for (const auto &[size, n] : description.get_variables()) {
      named_variable.emplace(n, std::make_pair(next_variable, size));
      next_variable += size;
      log_debug("Added variable %s", n.c_str());
    }
  }

  virtual void tick(float) {}

  void step(float dt) {
    AsebaVMRun(&vm, 1000);
    tick(dt);
  }

  void add_variable(const std::string &name, unsigned int size) {
    log_debug("Try to add variable %s of size %d", name.c_str(), size);
    if (named_variable.count(name)) {
      log_warn("Variable %s cannot be added: already defined", name.c_str());
      return;
    }
    const auto number = named_variable.size();
    if (next_variable + size > variables + VARIABLES_TOTAL_SIZE) {
      log_warn("Variable %s cannot be added: not enough free space",
               name.c_str());
      return;
    }
    named_variable.emplace(name, std::make_pair(next_variable, size));
    next_variable += size;
    description.add_variable(name, size);
    log_debug("Added variable");
  }

  std::map<std::string, std::vector<int>> get_variables() const {
    std::map<std::string, std::vector<int>> vs;
    for (const auto &[name, v] : named_variable) {
      const auto &[address, size] = v;
      vs[name].assign(address, address + size);
    }
    return vs;
  }

  std::vector<int> get_variable(const std::string &name) {
    if (!named_variable.count(name)) {
      std::cerr << "Unknown variable " << name << std::endl;
      return {};
    }
    auto &[address, size] = named_variable[name];
    std::vector<int> value;
    value.assign(address, address + size);
    return value;
  }

  void set_variable(const std::string &name, const std::vector<int> &value) {
    if (!named_variable.count(name)) {
      std::cerr << "Unknown variable " << name << std::endl;
      return;
    }
    auto &[address, vsize] = named_variable[name];
    const size_t size = std::min(value.size(), static_cast<size_t>(vsize));
    std::copy(value.begin(), value.begin() + size, address);
  }

  void add_event(const std::string &name, const std::string &desc = "") {
    if (named_event.count(name)) {
      log_warn("Event %s cannot be added: already defined", name.c_str());
      return;
    }
    const auto &[it, _] =
        named_event.emplace(name, static_cast<uint16_t>(named_event.size()));
    description.add_event(name, desc);
  }

  // ! Execute a local event, killing the execution of the current one if not
  // in step-by-step mode
  void emit_name(const std::string &name) {
    if (!named_event.count(name))
      return;
    const auto number = named_event[name];
    emit(number);
  }

  void emit(uint16_t number) {
    // in step-by-step, only setup an event if none is being executed
    // currently
    if (AsebaMaskIsSet(vm.flags, ASEBA_VM_STEP_BY_STEP_MASK) &&
        AsebaMaskIsSet(vm.flags, ASEBA_VM_EVENT_ACTIVE_MASK))
      return;

    variables[SOURCE] = static_cast<int16_t>(vm.nodeId);
    AsebaVMSetupEvent(&vm, ASEBA_EVENT_LOCAL_EVENTS_START - number);
    AsebaVMRun(&vm, 1000);
  }

  void add_function(const std::string &name, const Description::Function &fun,
                    size_t input_size) {
    log_debug("Try to add function %s", name.c_str());
    for (const auto &[fname, a, i] : functions) {
      if (fname == name) {
        log_warn("Function %s cannot be added: already defined", name.c_str());
        return;
      }
    }
    std::vector<int> sizes;
    const auto &[d, arguments] = fun;
    for (auto &[n, size] : arguments) {
      sizes.push_back(size);
    }
    description.add_function(name, fun);
    functions.emplace_back(name, sizes, input_size);
    log_debug("Added function");
  }

  void set_uuid(const std::array<uint8_t, 16> &uuid_) {
    uuid = uuid_;
    send_uuid(uuid_);
  }

  void set_friendly_name(const std::string &name_) {
    friendly_name = name_;
    send_friendly_name(name_);
  }

  void send_device_info(void *stream) {
    if (sent_device_info.count(stream))
      return;
    send_uuid(uuid);
    if (!friendly_name.empty()) {
      send_friendly_name(friendly_name);
    }
    sent_device_info.insert(stream);
  }

  const std::string &get_advertized_name() const {
    if (friendly_name.empty()) {
      return name;
    }
    return friendly_name;
  }

  void call_function(AsebaVMState *vm, unsigned id) {
    if (has_native_functions) {
      const auto num = static_cast<unsigned>(native_functions.size());
      if (id < num) {
        native_functions[id](vm);
        return;
      }
      id -= num;
    }
    call_extra_function(vm, id);
  };

  virtual void call_extra_function(AsebaVMState *vm, unsigned id) {

  };

  virtual void reset() {
    memset(vm.variables, 0, vm.variablesSize * sizeof(int16_t));
    variables[ID] = static_cast<int16_t>(vm.nodeId);
  }

protected:
  void send_uuid(const std::array<uint8_t, 16> &uuid) {
    log_info("Send device uuid");
    uint8_t size = static_cast<uint8_t>(uuid.size());
    std::vector<uint8_t> payload = {DEVICE_INFO_UUID, size};
    std::copy(uuid.begin(), uuid.end(), std::back_inserter(payload));
    AsebaSendMessage(&vm, ASEBA_MESSAGE_DEVICE_INFO, payload.data(),
                     static_cast<uint16_t>(payload.size()));
  }

  void send_friendly_name(const std::string &name) {
    log_info("Send device name %s", name.c_str());
    uint8_t size = static_cast<uint8_t>(name.length()) + 1;
    std::vector<uint8_t> payload = {DEVICE_INFO_NAME, size};
    std::copy(name.c_str(), name.c_str() + name.length() + 1,
              std::back_inserter(payload));
    AsebaSendMessage(&vm, ASEBA_MESSAGE_DEVICE_INFO, payload.data(),
                     static_cast<uint16_t>(payload.size()));
  }
};

#endif // NODE_H
