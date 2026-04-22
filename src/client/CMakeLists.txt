pybind11_add_module(_client_impl python.cpp)
target_link_libraries(_client_impl PRIVATE dashel asebacommon asebacompiler
                                           asebadashelplugins ${EXTRA_LIBS} spdlog::spdlog_header_only
                                           pybind11_log)
target_include_directories(_client_impl PRIVATE ${aseba_SOURCE_DIR}
                                                ${dashel_SOURCE_DIR}
                                                ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(_client_impl
                           PRIVATE PYBIND11_DETAILED_ERROR_MESSAGES=1)

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)
if(ipo_supported)
  set_property(TARGET _client_impl PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

# message("WARNINGS ${WARNINGS}")

target_compile_options(dashel PRIVATE ${DISABLE_WARNINGS})
target_compile_options(asebacommon PRIVATE ${DISABLE_WARNINGS})
target_compile_options(asebacompiler PRIVATE ${DISABLE_WARNINGS})
target_compile_options(asebadashelplugins PRIVATE ${DISABLE_WARNINGS})
target_compile_options(_client_impl PRIVATE ${WARNINGS})

install(
  TARGETS _client_impl
  DESTINATION pyaseba/client
  COMPONENT pyaseba)
