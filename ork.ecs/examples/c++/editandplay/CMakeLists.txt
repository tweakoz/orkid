cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
project (ork.example.ecs.editor CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

file(GLOB srcs ./*.cpp)

function(setupEXE target sources)
  add_executable (${target} ${sources} )
  ork_std_target_opts_exe(${target})
  target_include_directories (${target} PRIVATE ${ORKROOT}/ork.core/inc )
  target_include_directories (${target} PRIVATE ${ORKROOT}/ork.lev2/inc )
  target_include_directories (${target} PRIVATE ${ORKROOT}/ork.ecs/inc )
  target_include_directories (${target} PRIVATE ${SRCD} )
  target_link_libraries(${target} LINK_PRIVATE ork_core ork_lev2 ork_ecs swscale avutil avformat avcodec  )
  set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
endfunction()


setupEXE (ork.example.ecs.editandplay.editor.exe main_editor.cpp )
setupEXE (ork.example.ecs.editandplay.player.exe main_player.cpp )

#set( EXE_NAME ork.example.ecs.editor.exe )

#add_executable (${EXE_NAME} ${srcs} )

#ork_std_target_opts_exe(${EXE_NAME})
#target_link_libraries(${EXE_NAME} LINK_PRIVATE ork_core ork_lev2 ork_ecs swscale avutil avformat avcodec )

#set_target_properties(${EXE_NAME} PROPERTIES LINKER_LANGUAGE CXX)
#target_include_directories (${EXE_NAME} PRIVATE ${ORKROOT}/ork.core/inc )
#target_include_directories (${EXE_NAME} PRIVATE ${ORKROOT}/ork.lev2/inc )
#target_include_directories (${EXE_NAME} PRIVATE ${ORKROOT}/ork.ecs/inc )
#target_include_directories (${EXE_NAME} PRIVATE ${SRCD} )
#target_include_directories (${EXE_NAME} PRIVATE $ENV{OBT_STAGE}/include/luajit-2.1 )
