project (pyext_ork__lev2 CXX)
SET( PACKAGE_VERSION "0.1.1" )
include (GenerateExportHeader)

set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR}:$ENV{OBT_STAGE}/share/cmake/pybind11 )

find_package (Python COMPONENTS Interpreter Development)
FIND_PACKAGE( pybind11 REQUIRED )
INCLUDE_DIRECTORIES( ${PYTHON_INCLUDE_DIRS} ${PYBIND11_INCLUDE_DIRS} )

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR} )
set( ORKPKGBASE $ENV{OBT_STAGE}/lib/python3.8/site-packages/orkengine )
set( INSTALL_DEST ${ORKPKGBASE}/lev2 )
set( INCD $ENV{ORKID_WORKSPACE_DIR}/ork.core/inc )
set( INCD2 $ENV{ORKID_WORKSPACE_DIR}/ork.lev2/inc )

###################################
file(GLOB_RECURSE src_pyext ${SRCD}/*.cpp)

ADD_LIBRARY( _lev2 SHARED ${src_pyext} )

target_include_directories (_lev2 PRIVATE ${INCD} ${INCD2} )
target_link_libraries(_lev2 LINK_PRIVATE ork_lev2)

SET_TARGET_PROPERTIES( _lev2
  PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${INSTALL_DEST}
)

IF( APPLE )
  SET_TARGET_PROPERTIES( _lev2
    PROPERTIES
      LINK_FLAGS "-undefined dynamic_lookup"
      SUFFIX     ".so"
  )
  target_include_directories (_lev2 PRIVATE /usr/local/include)
ENDIF()

###################################

CONFIGURE_FILE( pyfiles/__lev2_init__.py ${INSTALL_DEST}/__init__.py )
#CONFIGURE_FILE( pyfiles/setup.py ${INSTALL_DEST}/setup.py)
