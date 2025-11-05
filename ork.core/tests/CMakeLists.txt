cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.core.test CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/../src )
set( TESTSRCD ${CMAKE_CURRENT_SOURCE_DIR} )
file(GLOB testsrcs ${TESTSRCD}/*.cpp)
add_executable (ork.test.core.exe ${testsrcs} )

target_include_directories (ork.test.core.exe PRIVATE ${SRCD} )

ork_std_target_opts_exe(ork.test.core.exe)
ork_std_target_opts_core(ork.test.core.exe)
ork_std_target_opts_utpp(ork.test.core.exe)

install(TARGETS ork.test.core.exe DESTINATION $ENV{OBT_SUBSPACE_BIN_DIR} )

###################################
# Swift Tests (macOS only)
###################################

IF(APPLE AND NOT BUILD_IOS_MINIMAL)
  include(swift/macos/CMakeLists.txt)
ENDIF()
