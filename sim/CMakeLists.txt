project(syn_sim)

find_package(ITC_Common REQUIRED QUIET COMPONENTS itc_logger)
find_package(NOSENGINE REQUIRED QUIET COMPONENTS common transport client uart)

include_directories(inc
                    ${sim_common_SOURCE_DIR}/inc
                    ${ITC_Common_INCLUDE_DIRS}
                    ${NOSENGINE_INCLUDE_DIRS})

set(syn_sim_src
    src/syn_hardware_model.cpp
    src/syn_42_data_provider.cpp
    src/syn_data_provider.cpp
    src/syn_data_point.cpp
)

# For Code::Blocks and other IDEs
file(GLOB syn_sim_inc inc/*.hpp)

set(syn_sim_libs
    sim_common
    ${ITC_Common_LIBRARIES}
    ${NOSENGINE_LIBRARIES}
)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH}:$ORIGIN/../lib") # Pick up .so in install directory

add_library(syn_sim SHARED ${syn_sim_src} ${syn_sim_inc})
target_link_libraries(syn_sim ${syn_sim_libs})
install(TARGETS syn_sim LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
