cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.lev2.test.ui.layout CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)
include_directories(AFTER ${ORKROOT}/ork.core/inc )
include_directories(AFTER ${ORKROOT}/ork.lev2/inc )
include_directories(AFTER ${SRCD} )

function(setupEXE target sources)
  add_executable (${target} harness.cpp ${sources} )
  ork_std_target_opts_exe(${target})
  target_link_libraries(${target} LINK_PRIVATE ork_core ork_lev2 )
  set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
endfunction()

setupEXE(ork.test.lev2.ui.layout1.exe layout.cpp )

setupEXE(ork.test.lev2.ui.surfaces.exe surfaces.cpp )
