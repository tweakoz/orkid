project (pyext_ork_lev2 CXX)
SET( PACKAGE_VERSION "0.1.1" )
include (GenerateExportHeader)

set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR}:$ENV{OBT_STAGE}/share/cmake/pybind11 )

find_package (Python COMPONENTS Interpreter Development)
FIND_PACKAGE( pybind11 REQUIRED )


###################################

  INCLUDE_DIRECTORIES(
    ${PYTHON_INCLUDE_DIRS}
    ${PYBIND11_INCLUDE_DIRS}
  )



  ADD_LIBRARY( lev2 SHARED pyext.cpp pyext_gfx.cpp pyext_meshutil.cpp pyext_primitives.cpp pyext_scenegraph.cpp )

  # The library must not have any prefix and should be located in
  # a subfolder that includes the package name. The setup will be
  # more complicated otherwise.
  SET_TARGET_PROPERTIES( lev2
    PROPERTIES
      PREFIX ""
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lev2"
  )

  # This is required for linking the library under Mac OS X. Moreover,
  # the suffix ensures that the module can be found by the interpreter
  # later on.
  IF( APPLE )
    SET_TARGET_PROPERTIES( lev2
      PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
        SUFFIX     ".so"
    )
  ENDIF()

  #set_target_properties(lev2 PROPERTIES OUTPUT_NAME ork/lev2)


  # Place the initialization file in the output directory for the Python
  # bindings. This will simplify the installation.
  CONFIGURE_FILE( lev2/__init__.py
    ${CMAKE_CURRENT_BINARY_DIR}/lev2/__init__.py
  )

  # Ditto for the setup file.
  CONFIGURE_FILE( lev2/setup.py
    ${CMAKE_CURRENT_BINARY_DIR}/lev2/setup.py
  )

IF(${APPLE})
    target_include_directories (lev2 PRIVATE /usr/local/include)
ENDIF()

target_link_libraries(lev2 LINK_PRIVATE ork_lev2)


set( INCD $ENV{ORKID_WORKSPACE_DIR}/ork.core/inc )
set( INCD2 $ENV{ORKID_WORKSPACE_DIR}/ork.lev2/inc )

install(TARGETS lev2 LIBRARY DESTINATION $ENV{OBT_STAGE}/lib )
target_include_directories (lev2 PRIVATE ${INCD} ${INCD2} )

set( INSTALL_DEST $ENV{OBT_STAGE}/lib/python3.8/site-packages/lev2 )
install(TARGETS lev2 LIBRARY DESTINATION ${INSTALL_DEST} )
install(FILES lev2/__init__.py DESTINATION ${INSTALL_DEST})
install(FILES lev2/setup.py DESTINATION ${INSTALL_DEST})
