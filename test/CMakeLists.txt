cmake_minimum_required(VERSION 3.20)
add_executable(test_writer test_writer.cpp)
target_link_libraries(test_writer PRIVATE lwtr fmt)
add_test(NAME test_writer COMMAND test_writer)
