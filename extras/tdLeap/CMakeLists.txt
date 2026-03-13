################################################################################
#
#  Software License Agreement (BSD License)
#  Copyright (c) 2003-2024, CHAI3D
#  (www.chai3d.org)
#
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions
#  are met:
#
#  * Redistributions of source code must retain the above copyright
#  notice, this list of conditions and the following disclaimer.
#
#  * Redistributions in binary form must reproduce the above
#  copyright notice, this list of conditions and the following
#  disclaimer in the documentation and/or other materials provided
#  with the distribution.
#
#  * Neither the name of CHAI3D nor the names of its contributors may
#  be used to endorse or promote products derived from this software
#  without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
#  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
#  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
#  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
#  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
#  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
#  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.
#
################################################################################

cmake_minimum_required(VERSION 3.10)

# Project information
project(tdLeap 
    VERSION 2.2.7
    DESCRIPTION "Wrapper to the Leap SDK"
    LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build configuration
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsigned-char")
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wshadow -Wpointer-arith -Wcast-qual -DDEBUG")
endif()

# Platform-specific settings
if(UNIX AND NOT APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DLINUX -Wno-deprecated -Wno-psabi")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m64 -march=x86-64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32 -march=${CMAKE_SYSTEM_PROCESSOR} -msse2 -mtune=generic")
    endif()
endif()

# Determine architecture suffix for library paths
if(UNIX AND NOT APPLE)
    set(OS_ARCH "lin-${CMAKE_SYSTEM_PROCESSOR}")
elseif(APPLE)
    set(OS_ARCH "mac-${CMAKE_SYSTEM_PROCESSOR}")
elseif(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OS_ARCH "win-x64")
    else()
        set(OS_ARCH "win-Win32")
    endif()
endif()

# Leap SDK paths
set(LEAP_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/externals/LeapSDK")
set(LEAP_INCLUDE_DIR "${LEAP_SDK_DIR}/include")
set(LEAP_LIBRARY_DIR "${LEAP_SDK_DIR}/lib/${OS_ARCH}")

# CHAI3D paths
set(CHAI3D_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(CHAI3D_SRC_DIR "${CHAI3D_DIR}/src")
set(CHAI3D_EIGEN_DIR "${CHAI3D_DIR}/externals/Eigen")
set(CHAI3D_GLEW_DIR "${CHAI3D_DIR}/externals/glew/include")

# Find Leap library
find_library(LEAP_LIBRARY
    NAMES Leap
    PATHS ${LEAP_LIBRARY_DIR}
    NO_DEFAULT_PATH)

if(NOT LEAP_LIBRARY)
    message(WARNING "Leap library not found in ${LEAP_LIBRARY_DIR}")
endif()

# Collect all source files
file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cc" "src/*.cpp" "src/*.cxx")
file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hh" "include/*.hpp" "include/*.hxx")

# Include directories
set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${LEAP_INCLUDE_DIR}
    ${CHAI3D_SRC_DIR}
    ${CHAI3D_EIGEN_DIR}
    ${CHAI3D_GLEW_DIR}
)

# Static library target
add_library(${PROJECT_NAME} STATIC ${SOURCES} ${HEADERS})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${LEAP_LIBRARY})

# Set library output directory
set(LIB_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/${CMAKE_BUILD_TYPE}/${OS_ARCH}-${CMAKE_CXX_COMPILER_ID}")
set(BIN_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set_target_properties(${PROJECT_NAME} PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
    POSITION_INDEPENDENT_CODE ON
)

# Shared library target (all platforms)
add_library(${PROJECT_NAME}_shared SHARED ${SOURCES} ${HEADERS})
target_include_directories(${PROJECT_NAME}_shared PUBLIC ${INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}_shared PUBLIC ${LEAP_LIBRARY})

set_target_properties(${PROJECT_NAME}_shared PROPERTIES
    OUTPUT_NAME ${PROJECT_NAME}
    ARCHIVE_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${LIB_OUTPUT_DIR}
    RUNTIME_OUTPUT_DIRECTORY ${BIN_OUTPUT_DIR}
    POSITION_INDEPENDENT_CODE ON
)

# Platform-specific shared library settings
if(UNIX AND NOT APPLE)
    set_target_properties(${PROJECT_NAME}_shared PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
        LINK_FLAGS "-Wl,--no-undefined"
    )
elseif(WIN32)
    # Windows DLL export settings
    target_compile_definitions(${PROJECT_NAME}_shared PRIVATE BUILDING_DLL)
    set_target_properties(${PROJECT_NAME}_shared PROPERTIES
        VERSION ${PROJECT_VERSION}
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

# Installation rules
install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/${PROJECT_NAME}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hh" PATTERN "*.hpp" PATTERN "*.hxx"
)

# Print configuration summary
message(STATUS "")
message(STATUS "========================================")
message(STATUS "tdLeap Configuration Summary")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "OS/Arch suffix: ${OS_ARCH}")
message(STATUS "Leap SDK: ${LEAP_SDK_DIR}")
message(STATUS "Leap library: ${LEAP_LIBRARY}")
message(STATUS "CHAI3D: ${CHAI3D_DIR}")
message(STATUS "Output directory: ${LIB_OUTPUT_DIR}")
message(STATUS "========================================")
message(STATUS "")
