cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.lev2.test CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/../src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )
file(GLOB testsrcs ${TESTSRCD}/*.cpp)
add_executable (ork.test.lev2.exe ${testsrcs} )

ork_std_target_opts(ork.test.lev2.exe)

target_include_directories (ork.test.lev2.exe PRIVATE ${SRCD} )

ork_std_target_opts_exe(ork.test.lev2.exe)
ork_std_target_opts_core(ork.test.lev2.exe)
ork_std_target_opts_lev2(ork.test.lev2.exe)
ork_std_target_opts_utpp(ork.test.lev2.exe)

install(TARGETS ork.test.lev2.exe DESTINATION $ENV{OBT_SUBSPACE_BIN_DIR} )
