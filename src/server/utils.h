#ifndef UTILS_H_INCLUDED
#define UTILS_H_INCLUDED

#include <string>
#include <cstring>
#include <vector>

#define VARIABLES_TOTAL_SIZE 1024
#define ID 0
#define SOURCE 1
#define BYTECODE_SIZE 1534
#define STACK_SIZE 32

#ifndef ASEBA_MESSAGE_DEVICE_INFO
#define ASEBA_MESSAGE_DEVICE_INFO 0x900D
#define DEVICE_INFO_UUID 1
#define DEVICE_INFO_NAME 2
#endif

#if 0
#define log_debug(...)                                                         \
  {                                                                            \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
  }
#define log_info(...)                                                          \
  {                                                                            \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
  }
#define log_warn(...)                                                          \
  {                                                                            \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
  }
#define log_error(...)                                                         \
  {                                                                            \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
  }
#else
#define log_debug(...)
#define log_info(...)
#define log_warn(...)
#define log_error(...)
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

inline std::string narrow(const std::wstring &src) { return narrow(src.c_str()); }



#endif // UTILS_H_INCLUDED
