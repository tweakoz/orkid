cmake_minimum_required (VERSION 3.13.4)
include(orkid)

project (ork.eda.vcdviewer CXX)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR} )

file(GLOB testsrcs ${SRCD}/*.cpp)

setupLev2EXE(ork.eda.vcdviewer ${testsrcs} )
target_include_directories (ork.eda.vcdviewer PRIVATE ${ORKROOT}/ork.eda/inc )
target_include_directories (ork.eda.vcdviewer PRIVATE ${SRCD} )
target_link_libraries(ork.eda.vcdviewer LINK_PRIVATE ork_eda )
target_link_libraries(ork.eda.vcdviewer LINK_PRIVATE openvdb openvdb_ax tbb)
