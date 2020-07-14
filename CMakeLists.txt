cmake_minimum_required (VERSION 3.13.4)
include (GenerateExportHeader)
project (Orkid)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(CMAKE_INSTALL_RPATH "$ENV{OBT_STAGE}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

list(PREPEND CMAKE_MODULE_PATH $ENV{OBT_STAGE}/lib/cmake)
list(PREPEND CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR} )

include(orkid)

add_compile_options(-Wno-deprecated -Wno-register -fexceptions)
add_compile_options(-Wno-unused-command-line-argument)
add_compile_options(-fPIE -fPIC -fno-common -fno-strict-aliasing -g -Wno-switch-enum)
add_compile_options(-fvisibility=default)

##############################

IF(PROFILER)
  add_compile_options(-DBUILD_WITH_EASY_PROFILER)
ENDIF()

##############################

set( destinc ${CMAKE_CURRENT_SOURCE_DIR}/ork/inc )
include_directories(BEFORE ${destinc})
link_directories(${destlib})

include_directories(AFTER $ENV{OBT_STAGE}/include)
include_directories(AFTER $ENV{OBT_STAGE}/include/tuio/oscpack)
link_directories($ENV{OBT_STAGE}/lib/)

link_directories($ENV{OBT_STAGE}/orkid/ork.tuio )
link_directories($ENV{OBT_STAGE}/orkid/ork.utpp )
link_directories($ENV{OBT_STAGE}/orkid/ork.core )
link_directories($ENV{OBT_STAGE}/orkid/ork.lev2 )
#link_directories($ENV{OBT_STAGE}/orkid/ork.ent )
#link_directories($ENV{OBT_STAGE}/orkid/ork.tool )


##############################

add_subdirectory (ork.tuio)
add_subdirectory (ork.utpp)
add_subdirectory (ork.core)
add_subdirectory (ork.lev2)
add_subdirectory (ork.eda)
#add_subdirectory (ork.ent)
#add_subdirectory (ork.tool)
#add_subdirectory (ork.toolmain)
