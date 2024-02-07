cmake_minimum_required(VERSION 2.6.4)
project(CFS_SYN_APP C)

#include(../ComponentSettings.cmake)
add_subdirectory(synopsis)

include_directories(fsw/public_inc)
include_directories(fsw/mission_inc)
include_directories(fsw/platform_inc)
include_directories(fsw/src)
include_directories(synopsis/include)

#include_directories(${hwlib_MISSION_DIR}/fsw/public_inc)

file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/${TGTSYS_${SYSVAR}}/data)
file(COPY fsw/owls DESTINATION ${CMAKE_INSTALL_PREFIX}/${TGTSYS_${SYSVAR}}/data/)
# Set RPATH that is getting stripped by CMAKE
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${TGTSYS_${SYSVAR}}/${INSTALL_SUBDIR}")

# Create the app module
aux_source_directory(fsw/src APP_SRC_FILES)
add_cfe_app(syn ${APP_SRC_FILES})

target_link_libraries(syn synopsis)
