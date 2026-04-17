#include "network.h"

namespace Aseba {

// -------------- Implementation of aseba glue code

extern "C" void AsebaPutVmToSleep(AsebaVMState *) {
  log_info("Received request to go into sleep");
}

extern "C" void AsebaSendBuffer(AsebaVMState *vm, const uint8_t *data,
                                uint16_t length) {
  Dashel::Stream *stream = Network::network_for_vm(vm)->stream;
  log_debug("AsebaSendBuffer: %u", length);
  if (stream) {
    try {
      uint16_t temp;
      temp = bswap16(length - 2);
      stream->write(&temp, 2);
      temp = bswap16(vm->nodeId);
      stream->write(&temp, 2);
      stream->write(data, length);
      stream->flush();
    } catch (Dashel::DashelException e) {
      log_warn("Cannot write to socket: %s", stream->getFailReason().c_str());
    }
  }
}

extern "C" uint16_t AsebaGetBuffer(AsebaVMState *vm, uint8_t *data, uint16_t,
                                   uint16_t *source) {
  Node *node = Network::node_for_vm(vm);
  if (node->lastMessageData.size()) {
    *source = node->lastMessageSource;
    memcpy(data, &node->lastMessageData[0], node->lastMessageData.size());
  }
  return static_cast<uint16_t>(node->lastMessageData.size());
}

extern "C" const AsebaVMDescription *AsebaGetVMDescription(AsebaVMState *vm) {
  // printf("Got Node description name: %s\n",
  // node_with_vm[vm]->node_description->name);
  // return Network::node_for_vm(vm)->variables_description;
  return Network::node_for_vm(vm)->description.get_description();
}

extern "C" const AsebaNativeFunctionDescription *const *
AsebaGetNativeFunctionsDescriptions(AsebaVMState *vm) {
  return Network::node_for_vm(vm)->description.get_functions();
  // return Network::node_for_vm(vm)->functions_description;
}

extern "C" const AsebaLocalEventDescription *
AsebaGetLocalEventsDescriptions(AsebaVMState *vm) {
  return Network::node_for_vm(vm)->description.get_events();
  // return Network::node_for_vm(vm)->events_description;
}

extern "C" void AsebaNativeFunction(AsebaVMState *vm, uint16_t id) {
  // printf("AsebaNativeFunction %d\n", id);
  Node *node = Network::node_for_vm(vm);
  if (!node)
    return;
  node->call_function(vm, id);
}

extern "C" void AsebaWriteBytecode(AsebaVMState *) {
  log_info("Received request to write bytecode into flash");
}

extern "C" void AsebaResetIntoBootloader(AsebaVMState *) {
  log_info("Received request to reset into bootloader");
}

extern "C" void AsebaAssert(AsebaVMState *vm, AsebaAssertReason reason) {
  const char *e;
  switch (reason) {
  case ASEBA_ASSERT_UNKNOWN:
    e = "undefined";
    break;
  case ASEBA_ASSERT_UNKNOWN_BINARY_OPERATOR:
    e = "unknown binary operator";
    break;
  case ASEBA_ASSERT_UNKNOWN_BYTECODE:
    e = "unknown bytecode";
    break;
  case ASEBA_ASSERT_STACK_OVERFLOW:
    e = "stack overflow";
    break;
  case ASEBA_ASSERT_STACK_UNDERFLOW:
    e = "stack underflow";
    break;
  case ASEBA_ASSERT_OUT_OF_VARIABLES_BOUNDS:
    e = "out of variables bounds";
    break;
  case ASEBA_ASSERT_OUT_OF_BYTECODE_BOUNDS:
    e = "out of bytecode bounds";
    break;
  case ASEBA_ASSERT_STEP_OUT_OF_RUN:
    e = "step out of run";
    break;
  case ASEBA_ASSERT_BREAKPOINT_OUT_OF_BYTECODE_BOUNDS:
    e = "breakpoint out of bytecode bounds";
    break;
  default:
    e = "unknown exception";
    break;
  }
  log_error("Fatal error; exception: %s; pc = %d, sp = %d", e, vm->pc, vm->sp);
  abort();
  log_info("Resetting VM");
  AsebaVMInit(vm);
}

extern "C" void AsebaVMResetCB(AsebaVMState *vm) {
  log_info("AsebaVMResetCB");
  Node *node = Network::node_for_vm(vm);
  if (node) {
    node->reset();
  }
}

extern "C" void AsebaVMRunCB(AsebaVMState *) {
  log_info("AsebaVMRunCB");
}
extern "C" void AsebaVMErrorCB(AsebaVMState *, const char *message) {
  log_info("AsebaVMErrorCB");
  log_error("%s", message);
}

} // namespace Aseba