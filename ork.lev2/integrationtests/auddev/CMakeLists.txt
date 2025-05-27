cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.lev2.test.auddev CXX)

include_directories(AFTER ${SRCD} )

setupLev2EXE(ork.test.lev2.auddev.exe auddev.cpp )
