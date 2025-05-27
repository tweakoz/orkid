cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.example.lev2.aud.singularity CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)
include_directories(AFTER ${SRCD} )

setupLev2EXE (ork.example.lev2.aud.singularity.krzrom.exe main_krzrom.cpp )
setupLev2EXE (ork.example.lev2.aud.singularity.cz1.exe main_cz1.cpp )
