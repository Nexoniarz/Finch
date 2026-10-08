cmake_minimum_required(VERSION 3.20)
project(finch VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Using LLVM ${LLVM_PACKAGE_VERSION}")

add_executable(finch
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/codegen.cpp
    src/codegen_types.cpp
    src/codegen_builtins.cpp
    src/abi.cpp
    src/cimport.cpp
)

# The runtime (runtime/finch_rt.c) is embedded into the compiler as a string;
# finch compiles it once into ~/.cache/finch and links it into every program.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/runtime/finch_rt.c)
file(READ ${CMAKE_SOURCE_DIR}/runtime/finch_rt.c FINCH_RT_SOURCE)
file(WRITE ${CMAKE_BINARY_DIR}/rt_source.inc "static const char *kRuntimeSource = R\"FINCHRT(${FINCH_RT_SOURCE})FINCHRT\";\n")
target_include_directories(finch PRIVATE ${CMAKE_BINARY_DIR})

# libclang reads C headers for `import "stdio.h"`
find_path(LIBCLANG_INCLUDE clang-c/Index.h HINTS ${LLVM_INCLUDE_DIRS})
find_library(LIBCLANG NAMES clang libclang HINTS ${LLVM_LIBRARY_DIRS})
if(NOT LIBCLANG_INCLUDE OR NOT LIBCLANG)
    message(FATAL_ERROR "libclang not found (on Nix: run inside nix-shell)")
endif()

target_include_directories(finch SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS} ${LIBCLANG_INCLUDE})
target_compile_definitions(finch PRIVATE ${LLVM_DEFINITIONS} FINCH_VERSION="${PROJECT_VERSION}")
target_compile_options(finch PRIVATE -Wall -Wextra)

# Link the one shared libLLVM when available: libclang uses it too, and two copies
# of LLVM in one process crash on exit.
if(TARGET LLVM)
    set(LLVM_LIBS LLVM)
else()
    llvm_map_components_to_libnames(LLVM_LIBS core support passes native)
endif()
target_link_libraries(finch PRIVATE ${LLVM_LIBS} ${LIBCLANG})
