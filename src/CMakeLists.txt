add_library (lwtr lwtr/lwtr.cpp lwtr/lwtr_text.cpp)
target_include_directories(lwtr PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries (lwtr PUBLIC SystemC::systemc fmt::fmt Boost::boost)

