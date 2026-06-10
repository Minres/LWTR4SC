###############################################################################
# 
###############################################################################
cmake_minimum_required(VERSION 3.20)
project(lwtr4sc VERSION 1.0.0)

if(CMAKE_PROJECT_NAME STREQUAL "lwtr4sc")
  enable_testing()
  include(FetchContent)
  # Make fmt's targets exportable so install(EXPORT) of lwtr/reporting succeeds,
  # and prefer a system-installed fmt (imported, already exportable) when present.
  set(FMT_INSTALL ON CACHE BOOL "" FORCE)
  FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 9.1.0
    FIND_PACKAGE_ARGS
  )
  FetchContent_MakeAvailable(fmt)
  # Build lz4 from source so the binary FTR backend (lwtr_ftr.cpp / tx_ftr_init)
  # is available on any machine without a preinstalled lz4. As a fetched
  # subproject lz4 builds a static library, so force PIC to link it into the
  # shared lwtr library, and skip its CLI. Alias to the lz4::lz4 name the
  # project's find_package()-based checks expect.
  set(LZ4_BUILD_CLI OFF CACHE BOOL "" FORCE)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  FetchContent_Declare(lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG v1.10.0
    SOURCE_SUBDIR build/cmake
  )
  FetchContent_MakeAvailable(lz4)
  if(NOT TARGET lz4::lz4)
    add_library(lz4::lz4 ALIAS lz4)
  endif()
  FetchContent_Declare(
      scp_git
      GIT_REPOSITORY  "https://github.com/accellera-official/systemc-common-practices.git"
      GIT_TAG         "origin/main"
      GIT_SHALLOW    ON
      UPDATE_DISCONNECTED NOT ${UPDATE_SCC_SRC} # When enabled, this option causes the update step to be skipped.
  )
  FetchContent_Populate(scp_git)
endif()


add_subdirectory(${scp_git_SOURCE_DIR}/report ${CMAKE_BINARY_DIR}/scp)
add_subdirectory(src)
add_subdirectory(example)
add_subdirectory(${CMAKE_SOURCE_DIR}/test EXCLUDE_FROM_ALL)
