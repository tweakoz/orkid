###################################
# Swift Tests for macOS
###################################

message(STATUS "Configuring Swift tests for macOS")

# Get absolute paths - CMAKE_CURRENT_LIST_DIR points to the directory containing this file
get_filename_component(SWIFT_TESTS_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(ORK_CORE_DIR "${SWIFT_TESTS_DIR}/../../.." ABSOLUTE)

set(SWIFT_BRIDGE_HEADER ${ORK_CORE_DIR}/inc/ork/swift/orkid_swift_bridge.h)
set(SWIFT_INCLUDE_DIR ${ORK_CORE_DIR}/inc)
set(SWIFT_MODULE_SRC_DIR ${ORK_CORE_DIR}/swext/src)

# Find swiftc compiler
find_program(SWIFTC swiftc)
if(NOT SWIFTC)
  message(WARNING "swiftc not found - Swift tests will not be built")
  return()
endif()

message(STATUS "Found swiftc: ${SWIFTC}")

# Build OrkCore Swift module
file(GLOB ORKCORE_SWIFT_SOURCES "${SWIFT_MODULE_SRC_DIR}/*.swift")
ork_build_swift_module(
  NAME OrkCore
  MODULE_DIR ${SWIFT_MODULE_SRC_DIR}
  SOURCES ${ORKCORE_SWIFT_SOURCES}
  BRIDGE_HEADER ${SWIFT_BRIDGE_HEADER}
  INCLUDE_DIR ${SWIFT_INCLUDE_DIR}
)

# Timer test
ork_add_swift_test(
  NAME timer
  SOURCE_DIR ${SWIFT_TESTS_DIR}/timer/src
  MAIN_SOURCE main.swift
  BRIDGE_HEADER ${SWIFT_BRIDGE_HEADER}
  INCLUDE_DIR ${SWIFT_INCLUDE_DIR}
)

# Math types test (vec3)
ork_add_swift_test(
  NAME vec3
  SOURCE_DIR ${SWIFT_TESTS_DIR}/vec3/src
  MAIN_SOURCE main.swift
  BRIDGE_HEADER ${SWIFT_BRIDGE_HEADER}
  INCLUDE_DIR ${SWIFT_INCLUDE_DIR}
)

# Math types test (vec4)
ork_add_swift_test(
  NAME vec4
  SOURCE_DIR ${SWIFT_TESTS_DIR}/vec4/src
  MAIN_SOURCE main.swift
  BRIDGE_HEADER ${SWIFT_BRIDGE_HEADER}
  INCLUDE_DIR ${SWIFT_INCLUDE_DIR}
)

# Math types test (mat4)
ork_add_swift_test(
  NAME mat4
  SOURCE_DIR ${SWIFT_TESTS_DIR}/mat4/src
  MAIN_SOURCE main.swift
  BRIDGE_HEADER ${SWIFT_BRIDGE_HEADER}
  INCLUDE_DIR ${SWIFT_INCLUDE_DIR}
)

# VarMap test
ork_add_swift_test(
  NAME varmap
  SOURCE_DIR ${SWIFT_TESTS_DIR}/varmap/src
  MAIN_SOURCE main.swift
  BRIDGE_HEADER ${SWIFT_BRIDGE_HEADER}
  INCLUDE_DIR ${SWIFT_INCLUDE_DIR}
)

# Callback test
ork_add_swift_test(
  NAME callback
  SOURCE_DIR ${SWIFT_TESTS_DIR}/callback/src
  MAIN_SOURCE main.swift
  BRIDGE_HEADER ${SWIFT_BRIDGE_HEADER}
  INCLUDE_DIR ${SWIFT_INCLUDE_DIR}
)
