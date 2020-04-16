project (orklev2 CXX)
include (GenerateExportHeader)

enableQt5()
find_package (Eigen3 3.3 REQUIRED NO_MODULE)
#option(LIBIGL_USE_STATIC_LIBRARY "Use libigl as static library" ON)
#option(LIBIGL_WITH_ANTTWEAKBAR      "Use AntTweakBar"    OFF)
#option(LIBIGL_WITH_CGAL             "Use CGAL"           OFF)
#option(LIBIGL_WITH_COMISO           "Use CoMiso"         OFF)
#option(LIBIGL_WITH_CORK             "Use Cork"           OFF)
#option(LIBIGL_WITH_EMBREE           "Use Embree"         OFF)
#option(LIBIGL_WITH_LIM              "Use LIM"            OFF)
#option(LIBIGL_WITH_MATLAB           "Use Matlab"         OFF)
#option(LIBIGL_WITH_MOSEK            "Use MOSEK"          OFF)
#option(LIBIGL_WITH_OPENGL           "Use OpenGL"         ON)
#option(LIBIGL_WITH_OPENGL_GLFW      "Use GLFW"           ON)
#option(LIBIGL_WITH_PNG              "Use PNG"            OFF)
#option(LIBIGL_WITH_PYTHON           "Use Python"         OFF)
#option(LIBIGL_WITH_TETGEN           "Use Tetgen"         OFF)
#option(LIBIGL_WITH_TRIANGLE         "Use Triangle"       OFF)
#option(LIBIGL_WITH_VIEWER           "Use OpenGL viewer"  ON)
#option(LIBIGL_WITH_XML              "Use XML"            OFF)
#find_package(LIBIGL REQUIRED)
#list(APPEND CMAKE_MODULE_PATH "${LIBIGL_INCLUDE_DIR}/../cmake")
#include($ENV{OBT_BUILDS}/igl/cmake/libigl.cmake )

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )
set( COREINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.core/inc )
set( TUIOINCD ${CMAKE_CURRENT_SOURCE_DIR}/../ork.tuio/inc )
###################################

file(GLOB_RECURSE src_aud ${SRCD}/aud/*.cpp)
file(GLOB_RECURSE src_gfx ${SRCD}/gfx/*.cpp)
file(GLOB_RECURSE src_input ${SRCD}/input/*.cpp)
file(GLOB_RECURSE src_qtui ${SRCD}/qtui/*.cpp)
file(GLOB_RECURSE src_ui ${SRCD}/ui/*.cpp)
file(GLOB_RECURSE src_ovr ${SRCD}/vr/*.cpp)
file(GLOB src_lev2 ${SRCD}/*.cpp)

IF(${APPLE})
  file(GLOB src_apple ${SRCD}/gfx/gl/*.mm)
ELSE()
  set( src_apple )
ENDIF()

file(GLOB_RECURSE src_glad ${SRCD}/gfx/gl/glad/*.c)

set( orksrcs ${src_aud} ${src_gfx} ${src_input} )
list( APPEND orksrcs ${src_qtui} ${src_ui} ${src_lev2} )
list( APPEND orksrcs ${src_apple} ${src_ovr} ${src_glad} )
list( APPEND orksrcs ${INCD}/ork/lev2/ezapp.h )

###################################

add_library (ork_lev2 SHARED ${orksrcs} )

###################################

set( libs ork_core ork_tuio )
list(APPEND libs OpenImageIO OpenImageIO_Util portaudio ispc_texcomp )
#list(APPEND libs igl::core igl::opengl_glfw )

IF(${APPLE})
    target_include_directories (ork_lev2 PRIVATE /usr/local/include)
    list(APPEND libs m pthread boost_filesystem )
    list(APPEND libs MoltenVK )
    list(APPEND libs "-framework OpenGL" "-framework Metal" "-framework IOSurface"  )
    list(APPEND libs "-framework CoreMIDI" "-framework CoreAudio" "-framework QuartzCore" )
    list(APPEND libs "-framework AudioUnit" "-framework AudioToolbox" )
    list(APPEND libs "-framework Carbon" "-framework Foundation" )
    list(APPEND libs "-framework ApplicationServices" "-framework AppKit" )
    list(APPEND libs glfw.3 objc)
ELSEIF(${UNIX})
    set(VULKAN_SDK=$ENV{OBT_STAGE}/builds/vulkan/1.2.131.2/x86_64)
    set(ENV{VULKAN_SDK}=${VULKAN_SDK})
    set(ENV{PATH}=$ENV{PATH}:${VULKAN_SDK}/bin)
    set(ENV{LD_LIBRARY_PATH}=$ENV{LD_LIBRARY_PATH}:${VULKAN_SDK}/lib)
    set(ENV{VK_LAYER_PATH}=${VULKAN_SDK}/etc/vulkan/explicit_layer.d)
    list(APPEND libs rt dl pthread GL Qt5::X11Extras openvr_api Xrandr vulkan )
ENDIF()

list(APPEND libs Qt5::Widgets Qt5::Core Qt5::Gui)
IF(PROFILER)
  list(APPEND libs easy_profiler)
ENDIF()

list(APPEND libs Eigen3::Eigen embree3)

target_link_libraries(ork_lev2 ${libs})

target_include_directories (ork_lev2 PRIVATE $ENV{OBT_STAGE}/include )
target_include_directories (ork_lev2 PRIVATE ${INCD} )
target_include_directories (ork_lev2 PRIVATE ${COREINCD} )
target_include_directories (ork_lev2 PRIVATE ${TUIOINCD} )
target_include_directories (ork_lev2 PRIVATE $ENV{QTDIR}/include/QtGui/$ENV{QTVER})
#target_include_directories (ork_lev2 PRIVATE $ENV{OBT_BUILDS}/igl/external/glad/include )
target_include_directories (ork_lev2 PRIVATE $ENV{OBT_BUILDS}/igl/include )

###################################

install(TARGETS ork_lev2 LIBRARY DESTINATION $ENV{OBT_STAGE}/lib )

###################################

add_subdirectory (tests)
add_subdirectory (examples)
add_subdirectory (pyext)
