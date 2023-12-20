cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.lev2.test CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/../src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )
file(GLOB testsrcs ${TESTSRCD}/*.cpp)
add_executable (ork.test.lev2.exe ${testsrcs} )

ork_std_target_opts(ork.test.lev2.exe)

target_link_libraries(ork.test.lev2.exe LINK_PRIVATE ork_utpp )
target_link_libraries(ork.test.lev2.exe LINK_PRIVATE ork_core )
target_link_libraries(ork.test.lev2.exe LINK_PRIVATE ork_lev2 )

set_target_properties(ork.test.lev2.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.test.lev2.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.test.lev2.exe PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (ork.test.lev2.exe PRIVATE ${SRCD} )

install(TARGETS ork.test.lev2.exe DESTINATION $ENV{OBT_SUBSPACE_BIN_DIR} )
