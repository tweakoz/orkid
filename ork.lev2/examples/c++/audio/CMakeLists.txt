cmake_minimum_required (VERSION 3.13.4)
include(orkid)

project (ork.example.lev2.aud.devicetest CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

file(GLOB srcs auddev.cpp)
set(EXE_NAME ork.example.lev2.aud.devicetest.exe)
add_executable (${EXE_NAME} ${srcs} )

ork_std_target_opts_exe(${EXE_NAME})

target_link_libraries(${EXE_NAME} LINK_PRIVATE ork_core ork_lev2 )

set_target_properties(${EXE_NAME} PROPERTIES LINKER_LANGUAGE CXX)
#enable_memdebug_on_target(${EXE_NAME})

target_include_directories (${EXE_NAME} PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (${EXE_NAME} PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (${EXE_NAME} PRIVATE ${SRCD} )
