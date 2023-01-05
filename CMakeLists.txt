###############################################################################
# 
###############################################################################
cmake_minimum_required(VERSION 3.12)
project(lwtr4sc)

if(CMAKE_PROJECT_NAME STREQUAL "lwtr4sc")
include(FetchContent)
FetchContent_Declare(
    scp_git
    GIT_REPOSITORY  "https://github.com/accellera-official/systemc-common-practices.git"
    GIT_TAG         "origin/main"
    GIT_SHALLOW    ON
    UPDATE_DISCONNECTED NOT ${UPDATE_SCC_SRC} # When enabled, this option causes the update step to be skipped.
)
FetchContent_Populate(scp_git)
#FetchContent_MakeAvailable(scp_git)
FetchContent_Declare(fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG master
)
FetchContent_MakeAvailable(fmt)
find_package(Boost)
endif()


add_subdirectory(${scp_git_SOURCE_DIR}/report ${CMAKE_BINARY_DIR}/scp)
add_subdirectory(src)
add_subdirectory(example)
