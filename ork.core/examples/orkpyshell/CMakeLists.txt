cmake_minimum_required (VERSION 3.13.4)
set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )
include(orkid)
project (ork.python.repl CXX)

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)

file(GLOB srcs ./*.cpp)
add_executable (ork.python.repl.exe ${srcs} )

target_link_libraries(ork.python.repl.exe LINK_PRIVATE ork_core )

set_target_properties(ork.python.repl.exe PROPERTIES LINKER_LANGUAGE CXX)
target_include_directories (ork.python.repl.exe PRIVATE ${ORKROOT}/ork.core/inc )
target_include_directories (ork.python.repl.exe PRIVATE ${SRCD} )
target_include_directories (ork.python.repl.exe PRIVATE $ENV{OBT_STAGE}/include/python3.8d )

install(TARGETS ork.python.repl.exe DESTINATION $ENV{OBT_STAGE}/bin )
