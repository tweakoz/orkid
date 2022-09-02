cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
#enableQt5()
project (ork.eda.vcdviewer CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

set( EXE ork.eda.vcdviewer)
set( SRCD ${CMAKE_CURRENT_SOURCE_DIR} )

file(GLOB testsrcs ${SRCD}/*.cpp)
add_executable (${EXE} ${testsrcs} )

ork_std_target_opts_exe(${EXE})

target_link_libraries(${EXE} LINK_PRIVATE ork_core )
target_link_libraries(${EXE} LINK_PRIVATE ork_eda )
target_link_libraries(${EXE} LINK_PRIVATE ork_lev2 )

set_target_properties(${EXE} PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (${EXE} PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (${EXE} PRIVATE ${ORKROOT}/ork.eda/inc )
target_include_directories (${EXE} PRIVATE ${ORKROOT}/ork.lev2/inc )
target_include_directories (${EXE} PRIVATE ${SRCD} )
