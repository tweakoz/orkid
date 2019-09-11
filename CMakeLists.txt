cmake_minimum_required (VERSION 3.13.4)
include (GenerateExportHeader)
project (Orkid)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(CMAKE_INSTALL_RPATH "$ENV{OBT_STAGE}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

IF(${APPLE})
add_definitions(-DORK_OSX)
set(CMAKE_MACOSX_RPATH 1)
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "$ENV{OBT_STAGE}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
   SET(CMAKE_INSTALL_RPATH "$ENV{OBT_STAGE}/lib")
ENDIF("${isSystemDir}" STREQUAL "-1")
add_definitions(-DOSX)
ELSE()
add_definitions(-DIX -DLINUX -DGCC)
add_compile_options(-D_REENTRANT -DQT_NO_EXCEPTIONS -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DQT_GUI_LIB -DQT_CORE_LIB)
ENDIF()

add_compile_options(-Wno-deprecated -Wno-register -fexceptions)
add_compile_options(-Wno-unused-command-line-argument)
add_compile_options(-fPIE -fPIC -fno-common -fno-strict-aliasing -g -Wno-switch-enum)

##############################

set( destinc ${CMAKE_CURRENT_SOURCE_DIR}/ork/inc )
include_directories(BEFORE ${destinc})
link_directories(${destlib})

include_directories(AFTER $ENV{OBT_STAGE}/include)
link_directories($ENV{OBT_STAGE}/lib/)

##############################

add_subdirectory (ork.tuio)
add_subdirectory (ork.utpp)
add_subdirectory (ork.fcollada305)
add_subdirectory (ork.core)
add_subdirectory (ork.lev2)
add_subdirectory (ork.ent)
add_subdirectory (ork.tool)
