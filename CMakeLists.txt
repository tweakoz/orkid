cmake_minimum_required (VERSION 3.13.4)
include (GenerateExportHeader)
project (Orkid)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(CMAKE_INSTALL_RPATH "$ENV{OBT_SUBSPACE_LIB_DIR}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

list(PREPEND CMAKE_MODULE_PATH $ENV{OBT_STAGE}/lib/cmake)
list(PREPEND CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/find_modules/" )
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/configs/" )

#message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")
#ERROR()

##############################

include(orkid)

##############################

add_subdirectory (ork.utpp)
add_subdirectory (ork.core)
add_subdirectory (ork.lev2)
add_subdirectory (ork.eda)
add_subdirectory (ork.ecs)
