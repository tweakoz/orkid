project (ork CXX)
include (GenerateExportHeader)
###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/inc )

###################################

file(GLOB_RECURSE src_application ${SRCD}/application/*.cpp)
file(GLOB_RECURSE src_asset ${SRCD}/asset/*.cpp)
file(GLOB_RECURSE src_dataflow ${SRCD}/dataflow/*.cpp)
file(GLOB_RECURSE src_event ${SRCD}/event/*.cpp)
file(GLOB_RECURSE src_file ${SRCD}/file/*.cpp)
file(GLOB_RECURSE src_kernel ${SRCD}/kernel/*.cpp)
file(GLOB_RECURSE src_math ${SRCD}/math/*.cpp)
file(GLOB_RECURSE src_object ${SRCD}/object/*.cpp)
file(GLOB_RECURSE src_reflect ${SRCD}/reflect/*.cpp)
file(GLOB_RECURSE src_rtti ${SRCD}/rtti/*.cpp)
file(GLOB_RECURSE src_stream ${SRCD}/stream/*.cpp)
file(GLOB_RECURSE src_util ${SRCD}/util/*.cpp)
file(GLOB_RECURSE src_gfx ${SRCD}/gfx/*.cpp)
file(GLOB_RECURSE src_hdl ${SRCD}/hdl/*.cpp)
file(GLOB_RECURSE src_opencl ${SRCD}/opencl/*.cpp)
file(GLOB_RECURSE src_python ${SRCD}/python/*.cpp)
file(GLOB_RECURSE src_test ${SRCD}/test/*.cpp)

set( orksrcs ${src_application} ${src_asset} ${src_dataflow} )
list( APPEND orksrcs ${src_event} ${src_file} ${src_kernel} )
list( APPEND orksrcs ${src_math} ${src_object} ${src_reflect} )
list( APPEND orksrcs ${src_rtti} ${src_stream} ${src_util} )
list( APPEND orksrcs ${src_gfx} ${src_hdl} ${src_opencl} ${src_python} ${src_test} )

###################################
# ISPC files (intel specific)
###################################

IF( "${ARCHITECTURE}" STREQUAL "x86_64" )

    gen_ispc_object_list( ${SRCD}/math/*.ispc
                          ork_core.dir/src/math/
                          ispc_math_object_list )

    list( APPEND orksrcs ${ispc_math_object_list} )

ENDIF()

###################################
# Regular (non-iOS) Build Target
###################################

IF(NOT BUILD_IOS_MINIMAL)

  if(NOT TARGET ObtOpenBlasLibTarget)
  #error()
  endif(NOT TARGET ObtOpenBlasLibTarget )

  add_library (ork_core SHARED ${orksrcs} )

ork_std_target_opts(ork_core)
enable_python_on_target(ork_core)
target_link_libraries(ork_core LINK_PRIVATE ork_utpp zmq curlpp curl sodium lz4 tar )

IF(${APPLE})
target_link_libraries(ork_core LINK_PRIVATE "-framework OpenCL" )
ELSE()
target_link_libraries(ork_core LINK_PRIVATE OpenCL )
ENDIF()

target_link_libraries(ork_core LINK_PRIVATE Boost::filesystem xxhash )
target_link_libraries(ork_core LINK_PUBLIC ${ObtOpenBlas_LIBRARIES} )

  find_package(Python COMPONENTS Interpreter Development REQUIRED)
  target_include_directories (ork_core PRIVATE ${INCD} )

  install(TARGETS ork_core LIBRARY DESTINATION $ENV{OBT_SUBSPACE_LIB_DIR} )

ENDIF()

###################################
# iOS Minimal Build Target
###################################

IF(BUILD_IOS_MINIMAL)

  message(STATUS "Configuring minimal iOS build for ork_core_ios")

  # Minimal file set for iOS - core utilities only
  file(GLOB src_math_ios
    ${SRCD}/math/cvector2_imp.cpp
    ${SRCD}/math/cvector3_imp.cpp
    ${SRCD}/math/cvector4_imp.cpp
    ${SRCD}/math/cmatrix3_imp.cpp
    ${SRCD}/math/cmatrix4_imp.cpp
    ${SRCD}/math/quaternion.cpp
    ${SRCD}/math/misc_math.cpp
    ${SRCD}/math/plane_imp.cpp
    ${SRCD}/math/box.cpp
    ${SRCD}/math/frustum.cpp
    ${SRCD}/math/collision_test.cpp
  )

  file(GLOB_RECURSE src_kernel_ios
    ${SRCD}/kernel/opq.cpp
    ${SRCD}/kernel/string/*.cpp
    ${SRCD}/kernel/memcpy.cpp
    ${SRCD}/kernel/prop.cpp
    ${SRCD}/kernel/proptype.cpp
    ${SRCD}/kernel/slashnode.cpp
    ${SRCD}/kernel/datablock.cpp
    ${SRCD}/kernel/datacache.cpp
    ${SRCD}/kernel/environment.cpp
    ${SRCD}/kernel/debug.cpp
  )

  file(GLOB src_file_ios
    ${SRCD}/file/path.cpp
    ${SRCD}/file/file.cpp
  )

  file(GLOB_RECURSE src_util_ios
    ${SRCD}/util/crc.cpp
    ${SRCD}/util/crc64.cpp
    ${SRCD}/util/md5.cpp
    ${SRCD}/util/hexdump.cpp
  )

  set(orksrcs_ios ${src_math_ios} ${src_kernel_ios} ${src_file_ios} ${src_util_ios})

  # Create shared library for iOS
  add_library(ork_core_ios SHARED ${orksrcs_ios})

  # iOS-specific compiler options
  target_compile_options(ork_core_ios PRIVATE
    -Wall -Wpedantic
    -Wno-deprecated -Wno-register
    -fPIE -fPIC
    -fexceptions
    -fvisibility=default
  )

  # iOS-specific definitions
  target_compile_definitions(ork_core_ios PRIVATE
    -DORK_IOS
    -DORK_CONFIG_IOS
    -DORK_ARCHITECTURE_ARM_64
    -DUSING_ORKID
  )

  # Include directories
  target_include_directories(ork_core_ios PRIVATE ${INCD})
  target_include_directories(ork_core_ios PRIVATE $ENV{OBT_STAGE}/include)

  # iOS frameworks and libraries (minimal dependencies)
  target_link_libraries(ork_core_ios LINK_PRIVATE
    "-framework Foundation"
    "-framework Accelerate"
  )

  # Only link Boost filesystem if available
  find_package(Boost COMPONENTS filesystem system)
  if(Boost_FOUND)
    target_link_libraries(ork_core_ios LINK_PRIVATE Boost::filesystem Boost::system)
  endif()

  # Install iOS library
  install(TARGETS ork_core_ios
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
  )

  # Install headers for iOS SDK distribution
  install(DIRECTORY ${INCD}/ork/
    DESTINATION include/ork
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
    PATTERN "*.inl"
    # Exclude Python, OpenCL, and other heavy dependencies
    PATTERN "python" EXCLUDE
    PATTERN "opencl" EXCLUDE
    PATTERN "lev2" EXCLUDE
  )

  message(STATUS "iOS target ork_core_ios configured")

ENDIF()

###################################

IF(NOT IOS_BUILD)
  add_subdirectory (pyext)
  add_subdirectory (examples)
  add_subdirectory (tests)
  add_subdirectory (utils)
  add_subdirectory (tools)
ENDIF()
