cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
project (ork.core.test CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/../src )
message(${proj_unittestpp_SOURCE_DIR}/src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )
file(GLOB testsrcs ${TESTSRCD}/*.cpp)
add_executable (ork.core.test.exe ${testsrcs} )

target_link_libraries(ork.core.test.exe LINK_PRIVATE ork_utpp )
target_link_libraries(ork.core.test.exe LINK_PRIVATE ork_core )
target_link_libraries(ork.core.test.exe LINK_PRIVATE boost_filesystem )

set_target_properties(ork.core.test.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.core.test.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.core.test.exe PRIVATE ${SRCD} )

install(TARGETS ork.core.test.exe DESTINATION $ENV{OBT_STAGE}/bin )
