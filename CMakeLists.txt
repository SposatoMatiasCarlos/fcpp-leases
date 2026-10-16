cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

# Built on its own (tests and example), with the fcpp submodule: ./make.sh test all, ./make.sh gui run rovers.
# Used by another project, with the FCPP of that project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fcpp/src/CMakeLists.txt)
        message(FATAL_ERROR "FCPP is missing: git submodule update --init")
    endif()
    option(FCPP_INTERNAL_TESTS "Build internal tests for FCPP." OFF)
    # the freetype of the GUI without the optional libraries it finds in the system (built by other compilers, on Windows)
    foreach(package ZLIB BZip2 PNG HarfBuzz BrotliDec)
        set(CMAKE_DISABLE_FIND_PACKAGE_${package} TRUE)
    endforeach()
    add_subdirectory(./fcpp/src)
    fcpp_setup()
endif()

project(
    fcpp-leases
    VERSION 0.1.0
    DESCRIPTION "Field-based leases: fault-tolerant task ownership for FCPP."
)

# The library: header-only, on top of the target fcpp.
#   add_subdirectory(./fcpp-leases)   or   FetchContent_MakeAvailable(fcpp_leases)
#   target_link_libraries(my_program PRIVATE fcpp::leases)
add_library(fcpp-leases INTERFACE)
add_library(fcpp::leases ALIAS fcpp-leases)
target_include_directories(fcpp-leases INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(fcpp-leases INTERFACE fcpp)

if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    return()
endif()

fcpp_test(./test/ownership.cpp)
fcpp_test(./test/recovery.cpp)

fcpp_target(./examples/rovers.cpp ON)
target_link_libraries(rovers PRIVATE fcpp::leases)
