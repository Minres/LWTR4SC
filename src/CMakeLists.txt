find_package(ZLIB QUIET)
find_package(lz4 QUIET)
find_package(fmt QUIET)

add_library(ftr INTERFACE)
target_include_directories(ftr INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # for headers when building
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> # for client in install mode
)

set(SOURCES lwtr/lwtr.cpp lwtr/lwtr_text.cpp)
if(TARGET lz4::lz4)
	list(APPEND SOURCES lwtr/util/lz4_streambuf.cpp)
	list(APPEND SOURCES lwtr/lwtr_ftr.cpp)
endif()

add_library (lwtr ${SOURCES})
target_include_directories(lwtr PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> # for headers when building
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}> # for client in install mode
)
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
target_link_libraries(lwtr PRIVATE fmt::fmt)

set_target_properties(lwtr PROPERTIES
    VERSION ${PROJECT_VERSION}
    FRAMEWORK FALSE
    PUBLIC_HEADER ${CMAKE_CURRENT_SOURCE_DIR}/lwtr/lwtr.h
)

install(TARGETS lwtr EXPORT lwtr-targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}${SCC_LIBRARY_DIR_MODIFIER}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}${SCC_LIBRARY_DIR_MODIFIER}${SCC_ARCHIVE_DIR_MODIFIER}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lwtr
        )

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/nonstd
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} # target directory
        FILES_MATCHING PATTERN "*.hpp" # select header files
        )

set(LWTR_CMAKE_CONFIG_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/lwtr)
include(CMakePackageConfigHelpers)

install(EXPORT lwtr-targets
    DESTINATION ${LWTR_CMAKE_CONFIG_DIR})

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/lwtr-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/lwtr-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/lwtr-config.cmake
    INSTALL_DESTINATION ${LWTR_CMAKE_CONFIG_DIR}
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/lwtr-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/lwtr-config-version.cmake
    DESTINATION ${LWTR_CMAKE_CONFIG_DIR})
