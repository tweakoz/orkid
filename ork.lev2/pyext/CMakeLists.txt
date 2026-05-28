project (pyext_ork__lev2 CXX)
SET( PACKAGE_VERSION "0.1.1" )
include (GenerateExportHeader)

#list( PREPEND CMAKE_MODULE_PATH $ENV{OBT_STAGE}/share/cmake/pybind11 )
#set(Torch_DIR $ENV{OBT_PYPKG}/torch/share/cmake/Torch )
#list( PREPEND CMAKE_MODULE_PATH $ENV{OBT_PYPKG}/torch/share/cmake )

find_package (Python COMPONENTS Interpreter Development)
#find_package (Eigen3 3.3 REQUIRED NO_MODULE)
#find_package( pybind11 REQUIRED )
#find_package(Torch REQUIRED)

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR}/src )
set( ORKPKGBASE $ENV{OBT_PYPKG}/orkengine )
set( INSTALL_DEST ${ORKPKGBASE}/lev2 )
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")

###################################
file(GLOB sources ${SRCD}/*.cpp)

add_library( _lev2 SHARED ${sources} )

# Get pybind11 to emit which arg/type failed (instead of the generic
# "could not convert default argument into a Python object" error).
target_compile_definitions(_lev2 PRIVATE PYBIND11_DETAILED_ERROR_MESSAGES=1)

# Force OBT's pybind11 ($OBT_STAGE/include/pybind11/) to come BEFORE
# pytorch's bundled pybind11 ($OBT_PYPKG/torch/include/pybind11/) on the
# compile include path. Without this, _lev2 gets pytorch's (newer)
# pybind11 internals version while _core uses OBT's (older) — they end
# up with separate type registries, and lev2's bindings can't see fvec4
# etc. registered in core.
target_include_directories(_lev2 BEFORE PRIVATE $ENV{OBT_STAGE}/include)

ork_std_target_opts(_lev2)
ork_std_target_opts_core(_lev2)
ork_std_target_opts_lev2(_lev2)
ork_torch_opts(_lev2) # torch should go last, so it cannot override the other libs

target_include_directories (_lev2 PRIVATE $ENV{OBT_PYTHON_HEADER_PATH} )
# igl crap again..
target_include_directories (_lev2 PRIVATE $ENV{OBT_STAGE}/include/eigen3 )

target_link_libraries(_lev2 LINK_PRIVATE openvdb openvdb_ax tbb )

SET_TARGET_PROPERTIES( _lev2
  PROPERTIES
    PREFIX ""
)

if( APPLE )
  set_target_properties( _lev2
    PROPERTIES
      LINK_FLAGS "-undefined dynamic_lookup"
      SUFFIX     ".so"
      # torch/lib needed: _lev2 links @rpath/libtorch*.dylib. This SET
      # runs after ork_torch_opts(_lev2) and would otherwise clobber the
      # rpath that adds — so include torch/lib here explicitly. (cmake
      # INSTALL_RPATH is a ;-separated list.)
      INSTALL_RPATH "$ENV{OBT_STAGE}/lib;$ENV{OBT_PYPKG}/torch/lib"
      BUILD_WITH_INSTALL_RPATH TRUE
  )
else()
  target_link_options(_lev2 PUBLIC -Wl,--no-undefined )
  target_link_libraries(_lev2 LINK_PRIVATE $ENV{OBT_PYPKG}/torch/lib/libtorch_python.so )
  set_target_properties( _lev2
    PROPERTIES
      # ;-separated cmake list — was ":" which made one malformed rpath
      # entry "<stage>/lib:<pypkg>/torch/lib" that resolved neither.
      INSTALL_RPATH "$ENV{OBT_STAGE}/lib;$ENV{OBT_PYPKG}/torch/lib"
      BUILD_WITH_INSTALL_RPATH TRUE
  )
endif()


install(TARGETS _lev2 LIBRARY DESTINATION ${INSTALL_DEST} )

###################################

configure_file( pyfiles/__lev2_init__.py ${INSTALL_DEST}/__init__.py )
#configure_file( pyfiles/setup.py ${INSTALL_DEST}/setup.py)
