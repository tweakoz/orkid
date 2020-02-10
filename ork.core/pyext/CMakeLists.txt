project (pyext_ork_core CXX)
SET( PACKAGE_VERSION "0.1.1" )
include (GenerateExportHeader)

set( CMAKE_MODULE_PATH $ENV{ORKID_WORKSPACE_DIR}:$ENV{OBT_STAGE}/share/cmake/pybind11 )

FIND_PACKAGE( pybind11 REQUIRED )
FIND_PACKAGE(PythonInterp 3 REQUIRED )
FIND_PACKAGE(PythonLibs 3 REQUIRED)

###################################

  MESSAGE("YO")
  INCLUDE_DIRECTORIES(
    ${PYTHON_INCLUDE_DIRS}
    ${PYBIND11_INCLUDE_DIRS}
  )

  ADD_LIBRARY( orkcore SHARED pyext.cpp )

  # The library must not have any prefix and should be located in
  # a subfolder that includes the package name. The setup will be
  # more complicated otherwise.
  SET_TARGET_PROPERTIES( orkcore
    PROPERTIES
      PREFIX ""
      LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/orkcore"
  )

  # This is required for linking the library under Mac OS X. Moreover,
  # the suffix ensures that the module can be found by the interpreter
  # later on.
  IF( APPLE )
    SET_TARGET_PROPERTIES( orkcore
      PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
        SUFFIX     ".so"
    )
  ENDIF()


  # Place the initialization file in the output directory for the Python
  # bindings. This will simplify the installation.
  CONFIGURE_FILE( orkcore/__init__.py
    ${CMAKE_CURRENT_BINARY_DIR}/orkcore/__init__.py
  )

  # Ditto for the setup file.
  CONFIGURE_FILE( orkcore/setup.py
    ${CMAKE_CURRENT_BINARY_DIR}/orkcore/setup.py
  )

IF(${APPLE})
    target_include_directories (orkcore PRIVATE /usr/local/include)
    target_link_libraries(orkcore LINK_PRIVATE ork_core)
ELSEIF(${UNIX})
ENDIF()

set( INCD ${CMAKE_CURRENT_SOURCE_DIR}/../inc )

install(TARGETS orkcore LIBRARY DESTINATION $ENV{OBT_STAGE}/lib )
target_include_directories (orkcore PRIVATE ${INCD} )

set( INSTALL_DEST $ENV{OBT_STAGE}/lib/python3.8/site-packages/orkcore )
install(TARGETS orkcore LIBRARY DESTINATION ${INSTALL_DEST} )
install(FILES orkcore/__init__.py DESTINATION ${INSTALL_DEST})
install(FILES orkcore/setup.py DESTINATION ${INSTALL_DEST})
