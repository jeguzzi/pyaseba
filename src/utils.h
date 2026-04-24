#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include <cstring>
#include <string>
#include <vector>

#define VARIABLES_TOTAL_SIZE 1024
#define ID 0
#define SOURCE 1
#define BYTECODE_SIZE 1534
#define STACK_SIZE 32

#ifndef USE_MOBSYA_ASEBA
#define ASEBA_MESSAGE_DEVICE_INFO 0x900D
#define ASEBA_MAX_TARGET_PROTOCOL_VERSION 9 // ASEBA_PROTOCOL_VERSION

typedef enum {
  DEVICE_INFO_UUID = 1,
  DEVICE_INFO_NAME = 2,
  DEVICE_INFO_THYMIO2_RF_SETTINGS = 3,

  DEVICE_INFO_ENUM_COUNT = 3
} DeviceInfoType;

#endif

#ifdef ENABLE_LOGGING

#include <spdlog/spdlog.h>

#define LOG_DEBUG(...)                                                         \
  {                                                                            \
    spdlog::debug(__VA_ARGS__);                                                \
  }
#define LOG_INFO(...)                                                          \
  {                                                                            \
    spdlog::info(__VA_ARGS__);                                                 \
  }
#define LOG_WARN(...)                                                          \
  {                                                                            \
    spdlog::warn(__VA_ARGS__);                                                 \
  }
#define LOG_ERROR(...)                                                         \
  {                                                                            \
    spdlog::error(__VA_ARGS__);                                                \
  }
#else
#define LOG_DEBUG(...)
#define LOG_INFO(...)
#define LOG_WARN(...)
#define LOG_ERROR(...)
#endif

// UTF8 to wstring
inline std::wstring widen(const char *src) {
  const size_t destSize(mbstowcs(0, src, 0) + 1);
  std::vector<wchar_t> buffer(destSize, 0);
  mbstowcs(&buffer[0], src, destSize);
  return std::wstring(buffer.begin(), buffer.end() - 1);
}

inline std::wstring widen(const std::string &src) { return widen(src.c_str()); }

// wstring to UTF8
inline std::string narrow(const wchar_t *src) {
  const size_t destSize(wcstombs(0, src, 0) + 1);
  std::vector<char> buffer(destSize, 0);
  wcstombs(&buffer[0], src, destSize);
  return std::string(buffer.begin(), buffer.end() - 1);
}

inline std::string narrow(const std::wstring &src) {
  return narrow(src.c_str());
}

#endif // UTILS_H_INCLUDED
