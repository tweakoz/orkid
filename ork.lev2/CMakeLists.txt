project (orklev2 CXX)
include (GenerateExportHeader)

enableQt5()

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
# ISPC files
###################################

gen_ispc_object_list( ${SRCD}/aud/*.ispc
                      ork_lev2.dir/src/aud
                      ispc_audio_object_list )

gen_ispc_object_list( ${SRCD}/gfx/*.ispc
                      ork_lev2.dir/src/gfx
                      ispc_gfx_object_list )

###################################

add_library (ork_lev2 SHARED ${orksrcs}
                             ${ispc_audio_object_list}
                             ${ispc_gfx_object_list} )

###################################

set( libs ork_core ork_tuio )
list(APPEND libs OpenImageIO OpenImageIO_Util portaudio ispc_texcomp openvr_api )
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
    list(APPEND libs rt dl pthread GL Qt5::X11Extras Xrandr vulkan )
ENDIF()

list(APPEND libs Qt5::Widgets Qt5::Core Qt5::Gui)
IF(PROFILER)
  list(APPEND libs easy_profiler)
ENDIF()

list(APPEND libs embree3 mpfr gmp )
IF(${UNIX})
  list(APPEND igl )
ENDIF()

target_link_libraries(ork_lev2 PUBLIC ${libs} triangle tetgen )
target_link_libraries(ork_lev2 PUBLIC boost_filesystem  )
target_link_libraries(ork_lev2 PRIVATE assimp audiofile )

target_include_directories (ork_lev2 PRIVATE $ENV{OBT_STAGE}/include )
target_include_directories (ork_lev2 PRIVATE ${INCD} )
target_include_directories (ork_lev2 PRIVATE ${COREINCD} )
target_include_directories (ork_lev2 PRIVATE ${TUIOINCD} )
target_include_directories (ork_lev2 PRIVATE $ENV{QTDIR}/include/QtGui/$ENV{QTVER})
#target_include_directories (ork_lev2 PRIVATE $ENV{OBT_BUILDS}/igl/external/glad/include )
target_include_directories (ork_lev2 PRIVATE $ENV{OBT_BUILDS}/igl/external/tetgen )

IF(${APPLE})
ELSE()
  target_link_options(ork_lev2 PUBLIC -Wl,--no-undefined )
  target_link_libraries(ork_lev2 PUBLIC X11 )
ENDIF()
###################################

install(TARGETS ork_lev2 LIBRARY DESTINATION $ENV{OBT_STAGE}/lib )

###################################

add_subdirectory (unittests)
add_subdirectory (integrationtests/singularity)
add_subdirectory (examples)
add_subdirectory (pyext)
IF(${APPLE})
  add_subdirectory (utils/osxhmdenum)
ELSE()
add_subdirectory (utils/luxhmdenum)
ENDIF()
