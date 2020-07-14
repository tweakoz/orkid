cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
project (ork.eda.test CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/../src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )
file(GLOB testsrcs ${TESTSRCD}/*.cpp)
add_executable (ork.test.eda.exe ${testsrcs} )

target_link_libraries(ork.test.eda.exe LINK_PRIVATE ork_utpp )
target_link_libraries(ork.test.eda.exe LINK_PRIVATE ork_eda )
target_link_libraries(ork.test.eda.exe LINK_PRIVATE boost_filesystem boost_system )

set_target_properties(ork.test.eda.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.test.eda.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.test.eda.exe PRIVATE ${ORKROOT}/ork.eda/inc )
target_include_directories (ork.test.eda.exe PRIVATE ${SRCD} )

install(TARGETS ork.test.eda.exe DESTINATION $ENV{OBT_STAGE}/bin )
