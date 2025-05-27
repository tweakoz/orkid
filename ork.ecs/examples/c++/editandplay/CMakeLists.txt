cmake_minimum_required (VERSION 3.13.4)
include(orkid)
project (ork.example.ecs.editor CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

file(GLOB srcs ./*.cpp)

function(setupEXE target sources)
  setupEcsEXE(${target} ${sources} )
  #target_include_directories (${target} PRIVATE ${SRCD} )
  target_link_libraries (${target} PRIVATE swscale avutil avformat avcodec )
endfunction()

setupEXE (ork.example.ecs.editandplay.editor.exe main_editor.cpp )
setupEXE (ork.example.ecs.editandplay.player.exe main_player.cpp )
