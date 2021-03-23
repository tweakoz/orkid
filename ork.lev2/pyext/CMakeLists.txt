project (pyext_ork__lev2 CXX)
SET( PACKAGE_VERSION "0.1.1" )
include (GenerateExportHeader)

set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR}:$ENV{OBT_STAGE}/share/cmake/pybind11 )

find_package (Python COMPONENTS Interpreter Development)
#find_package (Eigen3 3.3 REQUIRED NO_MODULE)
find_package( pybind11 REQUIRED )

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR} )
set( ORKPKGBASE $ENV{OBT_STAGE}/lib/python3.8/site-packages/orkengine )
set( INSTALL_DEST ${ORKPKGBASE}/lev2 )
set( INCD $ENV{ORKID_WORKSPACE_DIR}/ork.core/inc )
set( INCD2 $ENV{ORKID_WORKSPACE_DIR}/ork.lev2/inc )

###################################
file(GLOB sources ${SRCD}/*.cpp)

add_library( _lev2 SHARED ${sources} )

ork_std_target_opts(_lev2)

target_include_directories (_lev2 PRIVATE ${INCD} ${INCD2} )


target_link_libraries(_lev2 LINK_PUBLIC ork_core ork_lev2 )

target_link_libraries(_lev2 PUBLIC ${PYTHON_LIBRARY_PATH} )
target_link_options(_lev2 PUBLIC -Wl,-rpath )

set_target_properties( _lev2
  PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${INSTALL_DEST}
)

if( APPLE )
  set_target_properties( _lev2
    PROPERTIES
      LINK_FLAGS "-undefined dynamic_lookup"
      SUFFIX     ".so"
  )
else()
  target_link_options(_lev2 PUBLIC -Wl,--no-undefined )
endif()

# igl crap again..
target_include_directories (_lev2 PRIVATE $ENV{OBT_STAGE}/include/eigen3 )

###################################

configure_file( pyfiles/__lev2_init__.py ${INSTALL_DEST}/__init__.py )
#configure_file( pyfiles/setup.py ${INSTALL_DEST}/setup.py)
add_subdirectory (lev2qt)
