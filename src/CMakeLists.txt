find_package(ZLIB QUIET)
find_package(lz4 QUIET)
find_package(fmt QUIET)

add_library(ftr INTERFACE)
target_include_directories(ftr INTERFACE .)

set(SOURCES lwtr/lwtr.cpp lwtr/lwtr_text.cpp)
if(TARGET lz4::lz4)
	list(APPEND SOURCES lwtr/util/lz4_streambuf.cpp)
	list(APPEND SOURCES lwtr/lwtr_ftr.cpp)
endif()

add_library (lwtr ${SOURCES})
target_include_directories(lwtr PUBLIC ${CMAKE_CURRENT_LIST_DIR})
if(TARGET ZLIB::ZLIB)
    target_compile_definitions(lwtr PRIVATE WITH_ZLIB)
	target_link_libraries (lwtr PUBLIC ZLIB::ZLIB)
endif()
if(TARGET lz4::lz4)
    target_compile_definitions(lwtr PRIVATE WITH_LZ4)
    target_link_libraries(lwtr PRIVATE lz4::lz4)
endif()
if(TARGET SystemC::systemc)
    if(USE_CWR_SYSTEMC)
        get_target_property(INCLS SystemC::systemc INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(DIRS SystemC::systemc INTERFACE_LINK_DIRECTORIES)
        get_target_property(LIBS SystemC::systemc INTERFACE_LINK_LIBRARIES)
        target_include_directories (lwtr PUBLIC ${INCLS})    
        target_link_directories(lwtr PUBLIC ${DIRS})
        target_link_libraries(lwtr PUBLIC ${LIBS})
    else()
        target_link_libraries(lwtr PUBLIC SystemC::systemc)
    endif()
else()
    message(STATUS "lwtr: link with ${SystemC_INCLUDE_DIRS}, ${SystemC_DEFINITIONS}, and ${SystemC_LIBRARIES}")
    target_compile_definitions(lwtr PUBLIC ${SystemC_DEFINITIONS})
    target_include_directories (lwtr PUBLIC ${SystemC_INCLUDE_DIRS})    
    target_link_libraries(lwtr PUBLIC ${SystemC_LIBRARIES} )
endif()
target_link_libraries (lwtr PRIVATE fmt::fmt)

set_target_properties(lwtr PROPERTIES
    VERSION ${PROJECT_VERSION}
    FRAMEWORK FALSE
    PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/lwtr/lwtr.h
)

install(TARGETS lwtr
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}${SCC_LIBRARY_DIR_MODIFIER}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}${SCC_LIBRARY_DIR_MODIFIER}${SCC_ARCHIVE_DIR_MODIFIER}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lwtr
        )
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/nonstd
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # target directory
        FILES_MATCHING # install only matched files
        PATTERN "*.h" # select header files
        )


