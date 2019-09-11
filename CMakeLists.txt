cmake_minimum_required (VERSION 3.13.4)
include (GenerateExportHeader)
project (Orkid)


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC -shared")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

IF(${APPLE})
add_definitions(-DORK_OSX)
set(CMAKE_MACOSX_RPATH 1)
set(CMAKE_INSTALL_PREFIX $ENV{OBT_STAGE})
# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
# the RPATH to be used when installing, but only if it's not a system directory
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
   SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
ENDIF("${isSystemDir}" STREQUAL "-1")
add_definitions(-DOSX)
ELSE()
add_definitions(-DIX -DLINUX -DGCC)
add_compile_options(-Wno-deprecated -Wno-register -fexceptions)
add_compile_options(-Wno-unused-command-line-argument)
add_compile_options(-fPIE -fPIC -fno-common -fno-strict-aliasing -g -Wno-switch-enum)
add_compile_options(-D_REENTRANT -DQT_NO_EXCEPTIONS -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DQT_GUI_LIB -DQT_CORE_LIB)
ENDIF()

##############################

set( destinc ${CMAKE_CURRENT_SOURCE_DIR}/ork/inc )
include_directories(BEFORE ${destinc})
link_directories(${destlib})

include_directories(AFTER ${CMAKE_INSTALL_PREFIX}/include)
link_directories(${CMAKE_INSTALL_PREFIX}/lib/)

##############################

add_subdirectory (ork.tuio)
add_subdirectory (ork.utpp)
add_subdirectory (ork.fcollada305)
add_subdirectory (ork.core)
add_subdirectory (ork.lev2)
add_subdirectory (ork.ent)
add_subdirectory (ork.tool)
