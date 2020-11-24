cmake_minimum_required(VERSION 3.0.0)

set(TARGET_NAME driver_steamvr-test)
project(${TARGET_NAME} VERSION 0.1.0)


add_library(${TARGET_NAME} SHARED
    src/driver_sample.cpp
    src/driverlog.cpp
)
include_directories(${CMAKE_SOURCE_DIR}/include)

add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_NAME}> ${CMAKE_SOURCE_DIR}/bin/linux64/${TARGET_NAME}.so
)