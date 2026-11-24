include_directories(
   .
)

set(server_example_setpoint_SRCS
   server_example_setpoint.c
   static_model.c
)

IF(MSVC)
set_source_files_properties(${server_example_setpoint_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF(MSVC)

add_executable(server_example_setpoint
  ${server_example_setpoint_SRCS}
)

target_link_libraries(server_example_setpoint
    iec61850
)
