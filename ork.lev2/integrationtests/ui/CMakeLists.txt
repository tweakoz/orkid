cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
enableQt5()
project (ork.lev2.test.ui.layout CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)
include_directories(AFTER ${ORKROOT}/ork.core/inc )
include_directories(AFTER ${ORKROOT}/ork.lev2/inc )
include_directories(AFTER ${SRCD} )
include_directories(AFTER $ENV{QTDIR}/include)

function(setupEXE target sources)
  add_executable (${target} harness.cpp ${sources} )
  ork_std_target_opts(${target})
  target_link_libraries(${target} LINK_PRIVATE ork_core ork_lev2 boost_program_options )
  set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
  install(TARGETS ${target} DESTINATION $ENV{OBT_STAGE}/bin )
endfunction()

setupEXE(ork.test.lev2.ui.layout1.exe layout.cpp )
