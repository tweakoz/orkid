project (pyext_ork__ecssim CXX)
SET( PACKAGE_VERSION "0.1.1" )
include (GenerateExportHeader)

#list( PREPEND CMAKE_MODULE_PATH $ENV{OBT_STAGE}/share/cmake/pybind11 )

find_package (Python COMPONENTS Interpreter Development)
#find_package (Eigen3 3.3 REQUIRED NO_MODULE)
#find_package( pybind11 REQUIRED )

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR} )
set( ORKPKGBASE $ENV{OBT_PYPKG}/orkengine )
set( INSTALL_DEST ${ORKPKGBASE}/ecssim )
set( INCD $ENV{ORKID_WORKSPACE_DIR}/ork.core/inc )
set( INCD2 $ENV{ORKID_WORKSPACE_DIR}/ork.lev2/inc )
set( INCD3 $ENV{ORKID_WORKSPACE_DIR}/ork.ecs/inc )

###################################
file(GLOB sources ${SRCD}/*.cpp)

add_library( _ecssim SHARED ${sources} )

ork_std_target_opts(_ecssim)

target_include_directories (_ecssim PRIVATE ${INCD} ${INCD2} ${INCD3} )


target_link_libraries(_ecssim LINK_PRIVATE ork_core ork_lev2 ork_ecs )

#target_link_libraries(_ecssim PUBLIC ${PYTHON_LIBRARY_PATH} )
#target_link_options(_ecssim PUBLIC -Wl,-rpath )

set_target_properties( _ecssim
  PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${INSTALL_DEST}
)

if( APPLE )
  set_target_properties( _ecssim
    PROPERTIES
      LINK_FLAGS "-undefined dynamic_lookup"
      SUFFIX     ".so"
      INSTALL_RPATH "$ENV{OBT_STAGE}/lib"
      BUILD_WITH_INSTALL_RPATH TRUE
  )
else()
  target_link_options(_ecssim PUBLIC -Wl,--no-undefined )
endif()

# igl crap again..
target_include_directories (_ecssim PRIVATE $ENV{OBT_STAGE}/include/eigen3 )

###################################

configure_file( pyfiles/__ecssim_init__.py ${INSTALL_DEST}/__init__.py )
#configure_file( pyfiles/setup.py ${INSTALL_DEST}/setup.py)
