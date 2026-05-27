cmake_minimum_required(VERSION 3.16)
project(TalonBot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(TalonBot ${SOURCES})
target_include_directories(TalonBot PRIVATE "${CMAKE_SOURCE_DIR}/rewards")
target_include_directories(TalonBot PRIVATE "${CMAKE_SOURCE_DIR}/src")

set_target_properties(TalonBot PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/build"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_SOURCE_DIR}/build"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/build"
)

if(NOT GIGALEARN_ROOT)
    if(DEFINED ENV{GIGALEARN_ROOT})
        set(GIGALEARN_ROOT $ENV{GIGALEARN_ROOT})
    elseif(EXISTS "/workspace/libs/GigaLearnCPP/CMakeLists.txt")
        set(GIGALEARN_ROOT "/workspace/libs/GigaLearnCPP")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/../GigaLearnCPP/CMakeLists.txt")
        set(GIGALEARN_ROOT "${CMAKE_SOURCE_DIR}/../GigaLearnCPP")
    else()
        message(FATAL_ERROR "GIGALEARN_ROOT not found. Set GIGALEARN_ROOT env var, use -DGIGALEARN_ROOT=..., or place GigaLearnCPP in parent directory.")
    endif()
endif()

get_filename_component(GIGALEARN_ABS "${GIGALEARN_ROOT}" ABSOLUTE)
message(STATUS "GigaLearnCPP root: ${GIGALEARN_ABS}")

add_subdirectory("${GIGALEARN_ABS}" GigaLearnCPP)
target_link_libraries(TalonBot GigaLearnCPP RocketSim)
