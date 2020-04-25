project (pyext_ork__lev2qt CXX)
SET( PACKAGE_VERSION "0.1.1" )
include (GenerateExportHeader)

set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR}:$ENV{OBT_STAGE}/share/cmake/pybind11 )

find_package (Python COMPONENTS Interpreter Development)
#find_package (Eigen3 3.3 REQUIRED NO_MODULE)
find_package( pybind11 REQUIRED )

###################################

set( SRCD ${CMAKE_CURRENT_SOURCE_DIR} )
set( ORKPKGBASE $ENV{OBT_STAGE}/lib/python3.8/site-packages/orkengine )
set( INSTALL_DEST ${ORKPKGBASE}/lev2qt )
set( INCD $ENV{ORKID_WORKSPACE_DIR}/ork.core/inc )
set( INCD2 $ENV{ORKID_WORKSPACE_DIR}/ork.lev2/inc )

###################################
file(GLOB sources ${SRCD}/*.cpp)

add_library( _lev2qt SHARED ${sources} )
target_include_directories (_lev2qt PRIVATE ${SHIBOKENBINDINGSPATH} )
target_include_directories (_lev2qt PRIVATE ${PYSIDE_HEADER_PATH} )

target_include_directories (_lev2qt PRIVATE ${INCD} ${INCD2} )
target_include_directories( _lev2qt PRIVATE ${PYTHON_INCLUDE_DIRS} )
target_link_libraries(_lev2qt LINK_PUBLIC ork_core ork_lev2 )
target_link_libraries(_lev2qt LINK_PRIVATE ${SHIBOKEN_LIBRARY_FILE} )
add_library(ABC SHARED IMPORTED)
set_target_properties(ABC PROPERTIES
            IMPORTED_LOCATION ${PYSIDE_QTGUI_LIB}
            IMPORTED_IMPLIB ${PYSIDE_QTGUI_LIB}
            IMPORTED_NO_SONAME 0
            )

target_link_libraries(_lev2qt PUBLIC ABC )
target_link_libraries(_lev2qt PUBLIC ${PYTHON_LIBRARY_PATH} )
target_link_libraries(_lev2qt PUBLIC ${PYSIDE_LIBRARY_FILE} )
message(${PYSIDE_QTGUI_LIB})
#target_link_libraries(_lev2qt PUBLIC ${PYSIDE_QTGUI_LIB} )

target_link_options(_lev2qt PUBLIC -Wl,-rpath )
target_link_options(_lev2qt PUBLIC -Wl,--no-undefined )

set_target_properties( _lev2qt
  PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY ${INSTALL_DEST}
)

if( APPLE )
  set_target_properties( _lev2qt
    PROPERTIES
      LINK_FLAGS "-undefined dynamic_lookup"
      SUFFIX     ".so"
  )
  target_include_directories (_lev2qt PRIVATE /usr/local/include)
endif()

# igl crap again..
target_include_directories (_lev2qt PRIVATE $ENV{OBT_STAGE}/include/eigen3 )

###################################

configure_file( pyfiles/__lev2qt_init__.py ${INSTALL_DEST}/__init__.py )
#configure_file( pyfiles/setup.py ${INSTALL_DEST}/setup.py)
