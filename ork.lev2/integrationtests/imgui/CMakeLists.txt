cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.lev2.test.imgui CXX)

include_directories(AFTER ${SRCD} )

setupLev2EXE(ork.test.lev2.imgui.exe main.cpp imgui_demo.cpp )
