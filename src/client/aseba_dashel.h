#ifndef ASEBA_DASHEL_H_GUARD
#define ASEBA_DASHEL_H_GUARD

#include "aseba/common/msg/endian.h"
#include "aseba/common/msg/msg.h"
#include "dashel/dashel.h"
#include "utils.h"
#include <vector>

Aseba::Message *receive(Dashel::Stream *stream) {
  uint16_t len, source, type;
  stream->read(&len, 2);
  Aseba::swapEndian(len);
  stream->read(&source, 2);
  Aseba::swapEndian(source);
  stream->read(&type, 2);
  Aseba::swapEndian(type);
  LOG_INFO("Receive message of length {0} and type 0x{1:X} from {2}", len, type,
           source);
  // read content
  if (len > ASEBA_MAX_EVENT_ARG_SIZE) {
    LOG_ERROR("Message size {0} too large: ignore", len);
    return nullptr;
  }

  // Try to detect garbage:
  // if ((type > 100 && !Aseba::Message::has_type(type) && len > 0) || len >
  // 200) {
  //   LOG_ERROR("Suspect message: ignore", len);
  //   return nullptr;
  // }

  Aseba::Message::SerializationBuffer buffer;
  buffer.rawData.resize(len);

  if (len) {
    stream->read(&buffer.rawData[0], len);
  }
  // deserialize message
  try {
    return Aseba::Message::create(source, type, buffer);
  } catch (const std::exception &e) {
    LOG_ERROR("Message unpacking error: {0}", e.what());
  }
  return nullptr;
}

bool serialize(const Aseba::Message &message,
               const Aseba::Message::SerializationBuffer &buffer,
               Dashel::Stream *stream) {
  const auto len = static_cast<uint16_t>(buffer.rawData.size());
  if (len > ASEBA_MAX_EVENT_ARG_SIZE) {
    LOG_ERROR("Message size {0} too large", len);
    return false;
  }
  uint16_t t;
  Aseba::swapEndian(len);
  stream->write(&len, 2);
  t = Aseba::swapEndianCopy(message.source);
  stream->write(&t, 2);
  t = Aseba::swapEndianCopy(message.type);
  stream->write(&t, 2);
  if (buffer.rawData.size()) {
    stream->write(&buffer.rawData[0], buffer.rawData.size());
  }
  return true;
}

#endif // ASEBA_DASHEL_H_GUARD
