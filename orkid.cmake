cmake_minimum_required (VERSION 3.13.4)
include (GenerateExportHeader)
project (Orkid)

################################################################################

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)

################################################################################

set(CMAKE_INSTALL_RPATH "$ENV{OBT_STAGE}/lib")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

################################################################################

set(CMAKE_FIND_DEBUG_MODE OFF)
set(PYTHON_EXECUTABLE $ENV{OBT_STAGE}/bin/python3)
set(PYTHON_LIBRARY $ENV{OBT_STAGE}/lib/libpython3.8d.so)

################################################################################

IF(${APPLE})
    set(XCODE_SDKBASE /Library/Developer/CommandLineTools/SDKs)
    set(CMAKE_OSX_SYSROOT ${XCODE_SDKBASE}/MacOSX10.15.sdk)
    set(CMAKE_MACOSX_RPATH 1)
    LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "$ENV{OBT_STAGE}/lib" isSystemDir)
    IF("${isSystemDir}" STREQUAL "-1")
       SET(CMAKE_INSTALL_RPATH "$ENV{OBT_STAGE}/lib")
    ENDIF("${isSystemDir}" STREQUAL "-1")
    add_definitions(-DOSX -DORK_OSX)

    macro(ADD_OSX_FRAMEWORK fwname target)
        find_library(FRAMEWORK_${fwname}
        NAMES ${fwname}
        PATHS ${CMAKE_OSX_SYSROOT}/System/Library
        PATH_SUFFIXES Frameworks
        NO_DEFAULT_PATH)
        if( ${FRAMEWORK_${fwname}} STREQUAL FRAMEWORK_${fwname}-NOTFOUND)
            MESSAGE(ERROR ": Framework ${fwname} not found")
        else()
            TARGET_LINK_LIBRARIES(${target} PUBLIC "${FRAMEWORK_${fwname}}/${fwname}")
            MESSAGE(STATUS "Framework ${fwname} found at ${FRAMEWORK_${fwname}}")
        endif()
    endmacro(ADD_OSX_FRAMEWORK)

    set(CMAKE_MACOSX_RPATH 1)
    LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "$ENV{OBT_STAGE}/lib" isSystemDir)
    IF("${isSystemDir}" STREQUAL "-1")
       SET(CMAKE_INSTALL_RPATH "$ENV{OBT_STAGE}/lib")
    ENDIF("${isSystemDir}" STREQUAL "-1")
    add_definitions(-DOSX -DORK_OSX)
    link_directories(/usr/local/lib) # homebrew

ELSE()
    add_definitions(-DORK_CONFIG_IX -DLINUX -DGCC)
    add_compile_options(-D_REENTRANT -DQT_NO_EXCEPTIONS -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DQT_GUI_LIB -DQT_CORE_LIB)
ENDIF()

add_compile_definitions(QTVER=$ENV{QTVER})

add_compile_options(-Wno-deprecated -Wno-register -fexceptions)
add_compile_options(-Wno-unused-command-line-argument)
add_compile_options(-mavx -fPIE -fPIC -fno-common -fno-strict-aliasing -g -Wno-switch-enum)
add_compile_options(-fextended-identifiers)

set( ORKROOT $ENV{ORKID_WORKSPACE_DIR} )

##############################

#set( destinc $ENV{ORKID_WORKSPACE_DIR}/ork/inc )
#include_directories(BEFORE ${destinc})
link_directories(${destlib})

include_directories(AFTER $ENV{OBT_STAGE}/include)
include_directories(AFTER $ENV{OBT_STAGE}/include/tuio/oscpack)
link_directories($ENV{OBT_STAGE}/lib/)
link_directories($ENV{OBT_STAGE}/qt5/lib )

link_directories($ENV{OBT_STAGE}/orkid/ork.tuio )
link_directories($ENV{OBT_STAGE}/orkid/ork.utpp )
link_directories($ENV{OBT_STAGE}/orkid/ork.core )
link_directories($ENV{OBT_STAGE}/orkid/ork.lev2 )
link_directories($ENV{OBT_STAGE}/orkid/ork.ent )
link_directories($ENV{OBT_STAGE}/orkid/ork.tool )


set( ORK_CORE_INCD ${ORKROOT}/ork.core/inc )
set( ORK_LEV2_INCD ${ORKROOT}/ork.lev2/inc )
set( ORK_ECS_INCD ${ORKROOT}/ork.ecs/inc )

################################################################################
# QT5
################################################################################

# use a macro, not a function because of variable scoping issues
#  there does not seem to be an easy way to propagate variables
#  that were set by find_package up to the PARENT_SCOPE

macro(enableQt5)

  list(PREPEND CMAKE_MODULE_PATH $ENV{OBT_STAGE}/qt5/lib/cmake)

  IF(${APPLE})
  set(QT5BASE /usr/local/opt/qt/lib/cmake)
  ELSE()
  set(QT5BASE $ENV{OBT_STAGE}/qt5/lib/cmake)
  ENDIF()

  ###################################

  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTOUIC ON)
  set(CMAKE_AUTORCC ON)

  add_compile_options(-D_REENTRANT -DQT_NO_EXCEPTIONS -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -DQT_GUI_LIB -DQT_CORE_LIB)
  add_compile_definitions(QTVER=$ENV{QTVER})

  set(Qt5_DIR ${QT5BASE})
  set(Qt5Widgets_DIR ${QT5BASE}/Qt5Widgets)
  set(Qt5Test_DIR ${QT5BASE}/Qt5Test)
  set(Qt5Core_DIR ${QT5BASE}/Qt5Core)
  set(Qt5Concurrent_DIR ${QT5BASE}/Qt5Concurrent)
  set(Qt5Gui_DIR ${QT5BASE}/Qt5Gui)
  set(Qt5Network_DIR ${QT5BASE}/Qt5Network)
  set(Qt5X11Extras_DIR ${QT5BASE}/Qt5X11Extras)

  set(QT5_COMPONENTS Core Widgets Gui )

  # find_component(Qt5 ...) not working with X11Extras!
  #  so we will explicitly add our qt5 modules
  include(${Qt5Core_DIR}/Qt5CoreConfig.cmake )
  include(${Qt5Gui_DIR}/Qt5GuiConfig.cmake )
  include(${Qt5Widgets_DIR}/Qt5WidgetsConfig.cmake )

  if(${APPLE})
  else()
    include(${Qt5X11Extras_DIR}/Qt5X11ExtrasConfig.cmake )
    list(APPEND QT5_COMPONENTS X11Extras )
  endif()

  find_package(Qt5 COMPONENTS ${QT5_COMPONENTS} REQUIRED )

  include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})


endmacro()

##############################
