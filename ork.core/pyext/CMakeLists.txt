project (pyext_ork_core CXX)
SET( PACKAGE_VERSION "0.1.1" )
include (GenerateExportHeader)

list( PREPEND CMAKE_MODULE_PATH $ENV{OBT_STAGE}/share/cmake/pybind11 )

#message( "ORK_CORE" )
#message( $ENV{OBT_PYTHONHOME} )
#message( ${Python3_INCLUDE_DIRS} )

###################################

set( ORKPKGBASE $ENV{OBT_PYPKG}/orkengine )
set( INSTALL_DEST ${ORKPKGBASE}/core )
set( INCD $ENV{ORKID_WORKSPACE_DIR}/ork.core/inc )

###################################

file(GLOB SOURCES *.cpp)

ADD_LIBRARY( _core SHARED ${SOURCES} )

ork_std_target_opts(_core)

target_include_directories (_core PRIVATE ${INCD} )
target_link_libraries(_core LINK_PUBLIC ork_core)
target_link_libraries(_core LINK_PUBLIC ${ObtOpenBlas_LIBRARY} )

install(TARGETS _core LIBRARY DESTINATION ${INSTALL_DEST} )

SET_TARGET_PROPERTIES( _core
  PROPERTIES
    PREFIX ""
)

IF( APPLE )
  SET_TARGET_PROPERTIES( _core
    PROPERTIES
      LINK_FLAGS "-undefined dynamic_lookup"
      SUFFIX     ".so"
      INSTALL_RPATH "$ENV{OBT_STAGE}/lib"
      BUILD_WITH_INSTALL_RPATH TRUE
  )
ENDIF()

###################################


CONFIGURE_FILE( pyfiles/__orkengine_init__.py ${ORKPKGBASE}/__init__.py )
CONFIGURE_FILE( pyfiles/__core_init__.py ${INSTALL_DEST}/__init__.py )
#CONFIGURE_FILE( pyfiles/setup.py ${INSTALL_DEST}/setup.py)
