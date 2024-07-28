cmake_minimum_required (VERSION 3.13.4)
include (GenerateExportHeader)
project (Orkid)

################################################################################

set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_STANDARD_REQUIRED on)

################################################################################

set(CMAKE_INSTALL_RPATH "$ENV{OBT_SUBSPACE_LIB_DIR}")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

#############################################################################################################

function(orkid_find_python)

  #################################
  # hints for find_package
  #  to find the python for the active subspace 
  #################################

  set(CMAKE_FIND_DEBUG_MODE OFF)
  set(PYTHON_EXECUTABLE $ENV{OBT_PYTHONHOME}/bin/python3)
  set(PYTHON_LIBRARY $ENV{OBT_PYTHONHOME}/lib/libpython3.12.dylib)
  set(PYTHON_LIBRARY $ENV{OBT_PYTHONHOME}/lib/libpython3.12.so)

  set(Python3_FIND_STRATEGY "LOCATION")
  set(Python3_ROOT_DIR $ENV{OBT_PYTHONHOME} )
  #set(Python3_FIND_VIRTUALENV ONLY)

  find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
  find_package(pybind11 REQUIRED)

  #################################
  # export found python variables
  # to parent scope
  #################################

  #message( ${Python3_INCLUDE_DIRS} )
  set(Python3_INCLUDE_DIRS ${Python3_INCLUDE_DIRS} PARENT_SCOPE)
  set(Python3_LIBRARY_DIRS ${Python3_LIBRARY_DIRS} PARENT_SCOPE)
  set(Python3_RUNTIME_LIBRARY_DIRS ${Python3_RUNTIME_LIBRARY_DIRS} PARENT_SCOPE)
  set(Python3_LINK_OPTIONS ${Python3_LINK_OPTIONS} PARENT_SCOPE)
  set(Python3_LIBRARIES ${Python3_LIBRARIES} PARENT_SCOPE)

  #################################

endfunction()

orkid_find_python()

#message(STATUS "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}")

find_package(ObtOpenBlas REQUIRED)
IF(${APPLE})
ELSE()
find_package(ObtPipewire REQUIRED)
ENDIF()

################################################################################
# enable python for a given target
#  meant to be used by orkid, and orkid based projects
#  as it will function correctly for the host and other subspaces
################################################################################

function(enable_python_on_target the_target)
  set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS ${Python3_INCLUDE_DIRS} ${PYBIND11_INCLUDE_DIRS} )
  target_include_directories(${the_target} PUBLIC ${Python3_INCLUDE_DIRS} )
  target_link_directories(${the_target} PUBLIC ${Python3_RUNTIME_LIBRARY_DIRS} )
  target_link_directories(${the_target} PUBLIC ${Python3_LIBRARY_DIRS} )
  target_link_options(${the_target} PUBLIC ${Python3_LINK_OPTIONS})
  target_link_libraries(${the_target} PUBLIC ${Python3_LIBRARIES} )
  set_property( TARGET ${the_target} APPEND PROPERTY TGT_PUBLIC_LIBPATHS PUBLIC $ENV{OBT_PYTHON_LIB_PATH}  )
endfunction()

#############################################################################################################

function(enable_memdebug_on_target the_target)
  target_compile_options(${the_target} PUBLIC -fno-omit-frame-pointer)
  target_compile_options(${the_target} PUBLIC -fsanitize=address)
  target_compile_options(${the_target} PUBLIC -fsanitize=undefined)
  target_compile_options(${the_target} PUBLIC -fsanitize=leak)
  target_link_options(${the_target} PUBLIC -fsanitize=address)
endfunction()

#############################################################################################################

SET(BUILD_SHARED_LIBS ON)
find_package(Boost REQUIRED COMPONENTS system filesystem program_options)

#############################################################################################################

set( ORKROOT $ENV{ORKID_WORKSPACE_DIR} )
set( ORK_CORE_INCD ${ORKROOT}/ork.core/inc )
set( ORK_LEV2_INCD ${ORKROOT}/ork.lev2/inc )
set( ORK_ECS_INCD ${ORKROOT}/ork.ecs/inc )
set( ORK_ECS_SRCD ${ORKROOT}/ork.ecs/src )

################################################################################

IF(${APPLE})
  IF( "${ARCHITECTURE}" STREQUAL "x86_64" )
    set( HOMEBREW_PREFIX  /usr/local )
  ELSEIF( "${ARCHITECTURE}" STREQUAL "AARCH64" )
    set( HOMEBREW_PREFIX  /opt/homebrew )
  ENDIF()
ENDIF()

################################################################################

IF(${APPLE})
    set(CMAKE_OSX_DEPLOYMENT_TARGET 14.2)
    set(CMAKE_OSX_SYSROOT $ENV{OBT_MACOS_SDK_DIR})
    set(CMAKE_MACOSX_RPATH 1)
    LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "$ENV{OBT_STAGE}/lib" isSystemDir)
    IF("${isSystemDir}" STREQUAL "-1")
       SET(CMAKE_INSTALL_RPATH "$ENV{OBT_STAGE}/lib")
    ENDIF("${isSystemDir}" STREQUAL "-1")

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
ENDIF()

##############################

IF( "${ARCHITECTURE}" STREQUAL "x86_64" )
    add_compile_options(-march=native)
ELSEIF( "${ARCHITECTURE}" STREQUAL "AARCH64" )
ENDIF()

#############################################################################################################

function(ork_std_target_set_incdirs the_target)

  set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_STAGE}/include/eigen3 )
  set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_STAGE}/include)
  set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_STAGE}/include/tuio/oscpack)

  # IGL (its a beast, needs a cmake update)
  IF( "${ARCHITECTURE}" STREQUAL "x86_64" )
    set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_BUILDS}/igl/include )
    set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_BUILDS}/igl/external/triangle )

    IF(${APPLE})
    ELSE()
      set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS /usr/include/libdrm )
    ENDIF()

  ENDIF()

  # use homebrew last
  IF(${APPLE})
    set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS ${HOMEBREW_PREFIX}/include)
  ENDIF()

  IF( "${ARCHITECTURE}" STREQUAL "AARCH64" )
    set_property( TARGET ${the_target} APPEND PROPERTY TGT_INCLUDE_PATHS $ENV{OBT_BUILDS}/sse2neon )
  ENDIF()

endfunction()

#############################################################################################################

function(ork_std_target_set_defs the_target)

  set( def_list "" )

  list(APPEND def_list -DBUILD_WITH_EASY_PROFILER)

  IF(${BUILDING_ORKID})
    list(APPEND def_list -DBUILDING_ORKID)
    ELSE()
    list(APPEND def_list -DUSING_ORKID)
  ENDIF()


  IF(PROFILER)
    list(APPEND def_list -DBUILD_WITH_EASY_PROFILER)
  ENDIF()

  IF( "${ARCHITECTURE}" STREQUAL "x86_64" )
    list(APPEND def_list -DORK_ARCHITECTURE_X86_64)
  ELSEIF( "${ARCHITECTURE}" STREQUAL "AARCH64" )
    list(APPEND def_list -DORK_ARCHITECTURE_ARM_64)
    list(APPEND def_list -DKLEIN_ARCHITECTURE_ARM)
  ELSE()
    MESSAGE( FATAL_ERROR "unsupported architecture ${ARCHITECTURE}")
  ENDIF()

  if(${APPLE})
    list(APPEND def_list -DOSX -DORK_OSX )
  ELSE()
    list(APPEND def_list -DORK_CONFIG_IX -DLINUX -DGCC )
    list(APPEND def_list -D_REENTRANT -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE )
  ENDIF()

  list(SORT def_list)

  set_property( TARGET ${the_target} APPEND PROPERTY TGT_DEFINITIIONS ${def_list} )

endfunction()

#############################################################################################################

function(ork_std_target_set_opts the_target)

  set( opt_list "" )

  list(APPEND opt_list -Wall -Wpedantic)
  list(APPEND opt_list -Wno-deprecated -Wno-register -Wno-switch-enum)
  list(APPEND opt_list -Wno-unused-command-line-argument)
  list(APPEND opt_list -Wno-unused -Wno-extra-semi)
  list(APPEND opt_list -fPIE -fPIC )
  list(APPEND opt_list -fextended-identifiers)
  list(APPEND opt_list -fexceptions)
  list(APPEND opt_list -fvisibility=default)
  list(APPEND opt_list -fno-common -fno-strict-aliasing )
  list(APPEND opt_list -g  )


  IF(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND opt_list -frounding-math) # CGAL!
  ENDIF()

  #list(SORT def_list)

  set_property( TARGET ${the_target} APPEND PROPERTY TGT_OPTIONS ${opt_list} )

endfunction()

#############################################################################################################

function(ork_std_target_opts_compiler the_target)

  ork_std_target_set_opts(${the_target})
  ork_std_target_set_defs(${the_target})
  ork_std_target_set_incdirs(${the_target})

  ################################################################################
  # standardized header search paths
  ################################################################################

  get_property( TGT_INCLUDE_PATHS TARGET ${the_target} PROPERTY TGT_INCLUDE_PATHS )
  target_include_directories(${the_target} PRIVATE ${TGT_INCLUDE_PATHS} )

  ################################################################################
  # standardized compile options
  ################################################################################

  get_property( TGT_OPTIONS TARGET ${the_target} PROPERTY TGT_OPTIONS )
  target_compile_options(${the_target} PRIVATE ${TGT_OPTIONS})

  ################################################################################
  # standardized definitions
  ################################################################################

  get_property( TGT_DEFINITIIONS TARGET ${the_target} PROPERTY TGT_DEFINITIIONS )
  target_compile_definitions(${the_target} PRIVATE ${TGT_DEFINITIIONS} )

endfunction()

#############################################################################################################

function(ork_lev2_target_opts_compiler the_target)
  ork_std_target_opts_compiler(${the_target})
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.core/inc )
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.lev2/inc )
  target_include_directories (${the_target} PRIVATE ${SRCD} )
  set_target_properties(${the_target} PROPERTIES LINKER_LANGUAGE CXX)
endfunction()

#############################################################################################################

function( ork_ecs_target_opts_compiler the_target)
  ork_lev2_target_opts_compiler(${the_target})
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.core/inc )
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.lev2/inc )
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.ecs/inc )
  target_include_directories (${the_target} PRIVATE ${ORKROOT}/ork.ecs/src )
  target_include_directories (${the_target} PRIVATE ${SRCD} )
  target_include_directories (${the_target} PRIVATE $ENV{OBT_STAGE}/include/luajit-2.1 )
endfunction()

#############################################################################################################

function(ork_std_target_set_libdirs the_target)

  list(APPEND private_libdir_list "$ENV{OBT_SUBSPACE_LIB_DIR}" )
  list(APPEND private_libdir_list "$ENV{OBT_STAGE}/lib" )

  ################################################################################
  # IGL (its a beast, needs a cmake update)
  ################################################################################

  IF( "${ARCHITECTURE}" STREQUAL "x86_64" )

    list(APPEND CMAKE_MODULE_PATH "$ENV{OBT_STAGE}/lib/cmake/igl" )
    set( LIBIGL_DIR $ENV{OBT_STAGE}/lib/cmake/igl )

    option(LIBIGL_USE_STATIC_LIBRARY "Use libigl as static library" OFF)
    #option(LIBIGL_WITH_ANTTWEAKBAR      "Use AntTweakBar"    OFF)

    option(LIBIGL_WITH_CGAL             "Use CGAL"           OFF)
    option(LIBIGL_WITH_COMISO           "Use CoMiso"         ON)
    option(LIBIGL_WITH_CORK             "Use Cork"           ON)
    option(LIBIGL_WITH_EMBREE           "Use Embree"         ON)
    #option(LIBIGL_WITH_LIM              "Use LIM"            OFF)
    #option(LIBIGL_WITH_MATLAB           "Use Matlab"         OFF)
    #option(LIBIGL_WITH_MOSEK            "Use MOSEK"          OFF)
    #option(LIBIGL_WITH_OPENGL           "Use OpenGL"         ON)
    #option(LIBIGL_WITH_OPENGL_GLFW      "Use GLFW"           ON)
    #option(LIBIGL_WITH_PNG              "Use PNG"            OFF)
    #option(LIBIGL_WITH_PYTHON           "Use Python"         OFF)
    option(LIBIGL_WITH_TETGEN           "Use Tetgen"         OFF)
    option(LIBIGL_WITH_TRIANGLE         "Use Triangle"       ON)
    #option(LIBIGL_WITH_VIEWER           "Use OpenGL viewer"  ON)
    #option(LIBIGL_WITH_XML              "Use XML"            OFF)
    find_package(LIBIGL REQUIRED)
    #include($ENV{OBT_BUILDS}/igl/cmake/libigl.cmake )

    list(APPEND private_libdir_list "$ENV{OBT_BUILDS}/igl/.build" )

  ENDIF()

  set_target_properties( ${the_target} PROPERTIES TGT_PRIVATE_LIBPATHS "${private_libdir_list}" )

endfunction()

#############################################################################################################

function(ork_std_target_opts_compiler_module the_target )
  ork_std_target_set_opts(${the_target})
  ork_std_target_set_defs(${the_target})
  ork_std_target_set_incdirs(${the_target})
  ork_std_target_set_libdirs(${the_target})
endfunction()

#############################################################################################################

function(ork_std_target_opts_linker the_target)
  ork_std_target_set_libdirs(${the_target})
  get_property( TGT_PRIVATE_LIBPATHS TARGET ${the_target} PROPERTY TGT_PRIVATE_LIBPATHS )
  get_property( TGT_PUBLIC_LIBPATHS TARGET ${the_target} PROPERTY TGT_PUBLIC_LIBPATHS )

  target_link_directories(${the_target} PRIVATE ${TGT_PRIVATE_LIBPATHS} )
  target_link_directories(${the_target} PUBLIC ${TGT_PUBLIC_LIBPATHS} )

  set( BOOST_LIBS "" )
  list(APPEND BOOST_LIBS ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY}  )

  IF(${APPLE})
    target_link_directories(${the_target} PUBLIC ${HOMEBREW_PREFIX}/lib )
    target_link_libraries(${the_target} LINK_PRIVATE m pthread )
    target_link_libraries(${the_target} LINK_PRIVATE
          "-framework AppKit"
          "-framework IOKit"
          "-framework Accelerate"
    )
    target_link_libraries(${the_target} LINK_PRIVATE objc ${BOOST_LIBS} )
  ELSEIF(${UNIX})
    target_link_libraries(${the_target} LINK_PRIVATE rt dl pthread ${BOOST_LIBS} )
  ENDIF()

  target_link_libraries(${the_target} LINK_PUBLIC ${ObtOpenBlas_LIBRARIES} )

  target_link_libraries(${the_target} LINK_PUBLIC easy_profiler )

  
  endfunction()

#############################################################################################################

function(ork_std_target_opts the_target)
  ork_std_target_opts_compiler(${the_target})
  ork_std_target_opts_linker(${the_target})
endfunction()

function(ork_std_target_opts_exe the_target)
  ork_std_target_opts(${the_target})
  install(TARGETS ${the_target} DESTINATION $ENV{OBT_SUBSPACE_BIN_DIR} )
endfunction()


#############################################################################################################
# ISPC compile option
#############################################################################################################

function(declare_ispc_source_object src obj dep)
  add_custom_command(OUTPUT ${obj}
                     MAIN_DEPENDENCY ${src}
                     COMMENT "ISPC-Compile ${src}"
                     COMMAND ispc -O3 --target=avx ${src} -g -o ${obj} --colored-output
                     DEPENDS ${dep})
endfunction()

#############################################################################################################
# ISPC convenience method
#  I really dislike cmake as a language...
#############################################################################################################

function(gen_ispc_object_list
         ISPC_GLOB_SPEC
         ISPC_SUBDIR
         ISPC_OUTPUT_OBJECT_LIST )
  set(ISPC_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/${ISPC_SUBDIR})
  #message(${ISPC_GLOB_SPEC})
  #message(${ISPC_SUBDIR})
  #message(${ISPC_OUTPUT_DIR})
  file(GLOB_RECURSE SRC_ISPC ${ISPC_GLOB_SPEC} )
  foreach(SRC_ITEM ${SRC_ISPC})
    file(RELATIVE_PATH SRC_ITEM_RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src ${SRC_ITEM} )
    get_filename_component(ispc_name_we ${SRC_ITEM_RELATIVE} NAME_WE)
    get_filename_component(ispc_dir ${SRC_ITEM_RELATIVE} DIRECTORY)
    set(OBJ_ITEM ${ISPC_OUTPUT_DIR}/${ispc_name_we}.o)
    #message(${SRC_ITEM})
    #message(${ispc_name_we})
    #message(${ispc_dir})
    #message(${OBJ_ITEM})
    declare_ispc_source_object(${SRC_ITEM} ${OBJ_ITEM} ${SRC_ITEM} )
    list(APPEND _INTERNAL_ISPC_OUTPUT_OBJECT_LIST ${OBJ_ITEM} )
  endforeach(SRC_ITEM)
  #message(${_INTERNAL_ISPC_OUTPUT_OBJECT_LIST})
  set(${ISPC_OUTPUT_OBJECT_LIST} ${_INTERNAL_ISPC_OUTPUT_OBJECT_LIST} PARENT_SCOPE)
endfunction()

