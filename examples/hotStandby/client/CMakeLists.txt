include_directories(
   .
)

set(hot_standby_client_SRCS
   client_hot_reload.c
)

IF(MSVC)
set_source_files_properties(${hot_standby_client_SRCS}
                                       PROPERTIES LANGUAGE CXX)
ENDIF(MSVC)

add_executable(client_hot_reload
  ${hot_standby_client_SRCS}
)

target_link_libraries(client_hot_reload
    iec61850
)
