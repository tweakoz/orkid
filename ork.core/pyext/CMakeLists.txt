project (pyext_ork_core CXX)
SET( PACKAGE_VERSION "0.1.1" )
include (GenerateExportHeader)

set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR}:$ENV{OBT_STAGE}/share/cmake/pybind11 )

find_package (Python COMPONENTS Interpreter Development)
FIND_PACKAGE( pybind11 REQUIRED )
INCLUDE_DIRECTORIES( ${PYTHON_INCLUDE_DIRS} ${PYBIND11_INCLUDE_DIRS} )

###################################

set( ORKPKGBASE $ENV{OBT_STAGE}/lib/python3.8/site-packages/orkengine )
set( INSTALL_DEST ${ORKPKGBASE}/core )
set( INCD $ENV{ORKID_WORKSPACE_DIR}/ork.core/inc )

###################################

ADD_LIBRARY( _core SHARED pyext.cpp pyext_math.cpp )

target_include_directories (_core PRIVATE ${INCD} )
target_link_libraries(_core LINK_PRIVATE ork_core)

SET_TARGET_PROPERTIES( _core
  PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${INSTALL_DEST}
)

IF( APPLE )
  SET_TARGET_PROPERTIES( _core
    PROPERTIES
      LINK_FLAGS "-undefined dynamic_lookup"
      SUFFIX     ".so"
  )
  target_include_directories (_core PRIVATE /usr/local/include)
ENDIF()

###################################

CONFIGURE_FILE( pyfiles/__orkengine_init__.py ${ORKPKGBASE}/__init__.py )
CONFIGURE_FILE( pyfiles/__core_init__.py ${INSTALL_DEST}/__init__.py )
#CONFIGURE_FILE( pyfiles/setup.py ${INSTALL_DEST}/setup.py)
