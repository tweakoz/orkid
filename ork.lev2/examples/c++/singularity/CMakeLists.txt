cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.example.lev2.aud.singularity CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)
include_directories(AFTER ${ORKROOT}/ork.core/inc )
include_directories(AFTER ${ORKROOT}/ork.lev2/inc )
include_directories(AFTER ${SRCD} )
include_directories(AFTER $ENV{QTDIR}/include)

function(setupEXE target sources)
  add_executable (${target} ${sources} )
  ork_std_target_opts_exe(${target})
  target_link_libraries(${target} LINK_PRIVATE ork_core ork_lev2 )
  set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
endfunction()

setupEXE (ork.example.lev2.aud.singularity.krzrom.exe main_krzrom.cpp )
setupEXE (ork.example.lev2.aud.singularity.cz1.exe main_cz1.cpp )
