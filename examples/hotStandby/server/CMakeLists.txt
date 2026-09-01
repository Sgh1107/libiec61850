include_directories(
   .
)

set(hot_standby_server_SRCS
   server_hot_reload.c
)

IF(MSVC)
set_source_files_properties(${hot_standby_server_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF(MSVC)

add_executable(server_hot_reload
  ${hot_standby_server_SRCS}
)

target_link_libraries(server_hot_reload
    iec61850
)
