cmake_minimum_required(VERSION 3.0)

project(writer)

add_executable(writer main.cpp)
target_link_libraries(writer crypto ssl)

install(TARGETS writer RUNTIME DESTINATION bin)
