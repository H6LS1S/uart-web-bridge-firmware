# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.22)

find_program(NPM_EXECUTABLE npm REQUIRED)

execute_process(
        COMMAND ${NPM_EXECUTABLE} ci
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/client
        RESULT_VARIABLE NPM_CI_RESULT
)
if(NOT NPM_CI_RESULT EQUAL 0)
    message(FATAL_ERROR "npm ci failed")
endif()

execute_process(
        COMMAND ${NPM_EXECUTABLE} run build
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/client
        RESULT_VARIABLE NPM_BUILD_RESULT
)
if(NOT NPM_BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "npm run build failed")
endif()

set(GZ_INDEX_HTML_PATH "${CMAKE_CURRENT_SOURCE_DIR}/client/dist/index.html.gz")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(MINIMAL_BUILD ON)
project(uart-web-bridge)