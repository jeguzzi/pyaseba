pybind11_add_module(_client_impl python.cpp)
target_link_libraries(_client_impl PRIVATE dashel asebacommon asebacompiler)

target_include_directories(_client_impl PRIVATE ${dashel_SOURCE_DIR}
                                                ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(_client_impl
                           PRIVATE PYBIND11_DETAILED_ERROR_MESSAGES=1)

if(LOGGING)
  target_link_libraries(_client_impl PRIVATE spdlog::spdlog_header_only fmt::fmt-header-only)
  target_compile_definitions(_client_impl PRIVATE ENABLE_LOGGING)
  target_include_directories(_client_impl PRIVATE ${pybind11_log_SOURCE_DIR}/include)
endif()

if(MOBSYA)
  target_compile_definitions(_client_impl PRIVATE USE_MOBSYA_ASEBA)
  target_include_directories(_client_impl PRIVATE ${mobsya_aseba_SOURCE_DIR})
else()
  target_include_directories(_client_impl PRIVATE ${aseba_SOURCE_DIR})
endif()

if(TARGET zeroconf)
  target_compile_definitions(_client_impl PRIVATE ZEROCONF)
  target_link_libraries(_client_impl PRIVATE asebazeroconf)
  target_include_directories(_client_impl
                             PRIVATE ${aseba_SOURCE_DIR}/aseba/common)
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
  set_property(TARGET _client_impl PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

target_compile_options(dashel PRIVATE ${DISABLE_WARNINGS})
target_compile_options(asebacommon PRIVATE ${DISABLE_WARNINGS})
target_compile_options(asebacompiler PRIVATE ${DISABLE_WARNINGS})
target_compile_options(_client_impl PRIVATE ${WARNINGS})

install(
  TARGETS _client_impl
  DESTINATION pyaseba/client
  COMPONENT pyaseba)
