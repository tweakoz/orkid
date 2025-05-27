cmake_minimum_required (VERSION 3.13.4)
include(orkid)

project (ork.example.lev2.gfx.bufperftest CXX)
file(GLOB srcs ./*.cpp)
setupLev2EXE(ork.example.lev2.gfx.bufperftest.exe ${srcs} )
