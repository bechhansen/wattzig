# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS    
    ${CMAKE_CURRENT_SOURCE_DIR}/common/switch_driver
    )
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Get version from git tags, branch, or commit
execute_process(
    COMMAND git describe --tags --always --abbrev=0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    OUTPUT_VARIABLE GIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Set version: use tag if available (matches release pattern), otherwise branch-commit
if(GIT_TAG MATCHES "^v[0-9]+\\.")
    set(PROJECT_VER "${GIT_TAG}")
elseif(GIT_BRANCH AND GIT_COMMIT)
    set(PROJECT_VER "${GIT_COMMIT}")
else()
    set(PROJECT_VER "unknown")
endif()

message(STATUS "Build version: ${PROJECT_VER} (tag: ${GIT_TAG}, branch: ${GIT_BRANCH}, commit: ${GIT_COMMIT})")

project(PowerMeter)
