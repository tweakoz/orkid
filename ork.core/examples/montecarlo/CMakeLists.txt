cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.montecarlo CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

add_executable (ork.montecarlo_pi.exe ../shmobject_montecarlo_pi.cpp )

ork_std_target_opts(ork.montecarlo_pi.exe)

target_link_libraries(ork.montecarlo_pi.exe LINK_PRIVATE ork_core)

set_target_properties(ork.montecarlo_pi.exe PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories (ork.montecarlo_pi.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.montecarlo_pi.exe PRIVATE ${SRCD} )

install(TARGETS ork.montecarlo_pi.exe DESTINATION $ENV{OBT_STAGE}/bin )