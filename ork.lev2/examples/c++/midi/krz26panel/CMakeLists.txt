cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
project (ork.example.lev2.midi.krz26panel CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)
include_directories(AFTER ${ORKROOT}/ork.core/inc )
include_directories(AFTER ${ORKROOT}/ork.lev2/inc )
include_directories(AFTER ${SRCD} )

function(setupEXE target sources)
  add_executable (${target} ${sources} )
  ork_std_target_opts_exe(${target})
  target_link_libraries(${target} LINK_PRIVATE ork_core ork_lev2 )
  set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
endfunction()

setupEXE (ork.example.lev2.midi.krz26panel.exe main.cpp )
