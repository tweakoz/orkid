project (pyext_ork__lev2 CXX)
SET( PACKAGE_VERSION "0.1.1" )
include (GenerateExportHeader)

set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR}:$ENV{OBT_STAGE}/share/cmake/pybind11 )

find_package (Python COMPONENTS Interpreter Development)
FIND_PACKAGE( pybind11 REQUIRED )
set( ORKPKGBASE $ENV{OBT_STAGE}/lib/python3.8/site-packages/orkengine )
set( INSTALL_DEST ${ORKPKGBASE}/lev2 )

###################################
INCLUDE_DIRECTORIES( ${PYTHON_INCLUDE_DIRS} ${PYBIND11_INCLUDE_DIRS} )

ADD_LIBRARY( _lev2 SHARED pyext.cpp pyext_gfx.cpp pyext_meshutil.cpp pyext_primitives.cpp pyext_scenegraph.cpp )


# The library must not have any prefix and should be located in
# a subfolder that includes the package name. The setup will be
# more complicated otherwise.
SET_TARGET_PROPERTIES( _lev2
  PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${INSTALL_DEST}
)
# This is required for linking the library under Mac OS X. Moreover,
# the suffix ensures that the module can be found by the interpreter
# later on.
IF( APPLE )
  SET_TARGET_PROPERTIES( _lev2
    PROPERTIES
      LINK_FLAGS "-undefined dynamic_lookup"
      SUFFIX     ".so"
  )
ENDIF()

CONFIGURE_FILE( pyfiles/__orkengine_init__.py ${ORKPKGBASE}/__init__.py )
CONFIGURE_FILE( pyfiles/__lev2_init__.py ${INSTALL_DEST}/__init__.py )
CONFIGURE_FILE( pyfiles/setup.py ${INSTALL_DEST}/setup.py)

IF(${APPLE})
    target_include_directories (_lev2 PRIVATE /usr/local/include)
ENDIF()

target_link_libraries(_lev2 LINK_PRIVATE ork_lev2)

set( INCD $ENV{ORKID_WORKSPACE_DIR}/ork.core/inc )
set( INCD2 $ENV{ORKID_WORKSPACE_DIR}/ork.lev2/inc )

target_include_directories (_lev2 PRIVATE ${INCD} ${INCD2} )
