# The admission example: one node admitting protobuf peer frames and JSON RPC requests. Needs
# protobuf (with_protobuf=ON).
add_executable(rpcspec_admission_example main.cpp gossip.proto)
target_link_libraries(rpcspec_admission_example PRIVATE rpcspec::rpcspec protobuf::libprotobuf)
target_include_directories(
    rpcspec_admission_example
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}
)
protobuf_generate(TARGET rpcspec_admission_example LANGUAGE cpp)
