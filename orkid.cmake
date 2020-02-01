cmake_minimum_required (VERSION 3.13.4)
include (GenerateExportHeader)
project (Orkid)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

set(CMAKE_INSTALL_RPATH "$ENV{OBT_STAGE}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

IF(${APPLE})
set(CMAKE_MACOSX_RPATH 1)
LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "$ENV{OBT_STAGE}/lib" isSystemDir)
IF("${isSystemDir}" STREQUAL "-1")
   SET(CMAKE_INSTALL_RPATH "$ENV{OBT_STAGE}/lib")
ENDIF("${isSystemDir}" STREQUAL "-1")
add_definitions(-DOSX -DORK_OSX)
ELSE()
add_definitions(-DIX -DLINUX -DGCC)
add_compile_options(-D_REENTRANT -DQT_NO_EXCEPTIONS -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DQT_GUI_LIB -DQT_CORE_LIB)
ENDIF()

add_compile_definitions(QTVER=$ENV{QTVER})

add_compile_options(-Wno-deprecated -Wno-register -fexceptions)
add_compile_options(-Wno-unused-command-line-argument)
add_compile_options(-fPIE -fPIC -fno-common -fno-strict-aliasing -g -Wno-switch-enum)

set( ORKROOT $ENV{ORKID_WORKSPACE_DIR} )

##############################

#set( destinc $ENV{ORKID_WORKSPACE_DIR}/ork/inc )
#include_directories(BEFORE ${destinc})
link_directories(${destlib})

include_directories(AFTER $ENV{OBT_STAGE}/include)
include_directories(AFTER $ENV{OBT_STAGE}/include/tuio/oscpack)
link_directories($ENV{OBT_STAGE}/lib/)

link_directories($ENV{OBT_STAGE}/orkid/ork.tuio )
link_directories($ENV{OBT_STAGE}/orkid/ork.utpp )
link_directories($ENV{OBT_STAGE}/orkid/ork.core )
link_directories($ENV{OBT_STAGE}/orkid/ork.lev2 )
link_directories($ENV{OBT_STAGE}/orkid/ork.ent )
link_directories($ENV{OBT_STAGE}/orkid/ork.tool )


###################################

IF(${APPLE})
set(QT5BASE /usr/local/opt/qt/lib/cmake)
set(CMAKE_MACOSX_RPATH 1)
ELSE()
set(QT5BASE $ENV{OBT_STAGE}/qt5/lib/cmake)
ENDIF()

###################################

set(Qt5Widgets_DIR ${QT5BASE}/Qt5Widgets)
set(Qt5Test_DIR ${QT5BASE}/Qt5Test)
set(Qt5Core_DIR ${QT5BASE}/Qt5Core)
set(Qt5_DIR ${QT5BASE})
set(Qt5Concurrent_DIR ${QT5BASE}/Qt5Concurrent)
set(Qt5Gui_DIR ${QT5BASE}/Qt5Gui)
set(Qt5Network_DIR ${QT5BASE}/Qt5Network)

find_package(Qt5 COMPONENTS Core Widgets Gui REQUIRED)

if(${APPLE})
else()
#find_package(Qt5X11Extras REQUIRED)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

##############################
