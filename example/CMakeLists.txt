cmake_minimum_required(VERSION 3.20)

add_executable(lwtr_example lwtr_example.cpp)
target_link_libraries (lwtr_example lwtr reporting)

add_test(NAME lwtr_example_test COMMAND lwtr_example)