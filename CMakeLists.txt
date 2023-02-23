### Configuration
cmake_minimum_required (VERSION 3.7)

project (CppReact)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wpedantic -Wno-unused-variable -Wno-keyword-macro")

include_directories ("${PROJECT_SOURCE_DIR}/include")

add_subdirectory(deps)

### CppReact
add_library(CppReact INTERFACE)

target_compile_features(CppReact INTERFACE cxx_std_17)

target_link_libraries(CppReact INTERFACE tbb_static)

### examples/ 
option(build_examples "Build examples?" ON)
if(build_examples)
	add_subdirectory(examples)
endif()

### benchmarks/ 
option(build_benchmarks "Build benchmarks?" OFF)
if(build_benchmarks)
	add_subdirectory(benchmarks)
endif()

### tests/ 
option(build_tests "Build unit tests?" OFF)
if(build_tests)
	add_subdirectory(tests)
endif()
