###################################
# Swift Tests for macOS
###################################

message(STATUS "Configuring Swift tests for macOS")

# Get absolute paths - CMAKE_CURRENT_LIST_DIR points to the directory containing this file
get_filename_component(SWIFT_TESTS_DIR "${CMAKE_CURRENT_LIST_DIR}" ABSOLUTE)
get_filename_component(ORK_CORE_DIR "${SWIFT_TESTS_DIR}/../../.." ABSOLUTE)

set(SWIFT_BRIDGE_HEADER ${ORK_CORE_DIR}/inc/ork/swift/orkid_swift_bridge.h)
set(SWIFT_INCLUDE_DIR ${ORK_CORE_DIR}/inc)

# Find swiftc compiler
find_program(SWIFTC swiftc)
if(NOT SWIFTC)
  message(WARNING "swiftc not found - Swift tests will not be built")
  return()
endif()

message(STATUS "Found swiftc: ${SWIFTC}")

# test1 - Timer test
ork_add_swift_test(
  NAME test1
  SOURCE_DIR ${SWIFT_TESTS_DIR}/test1/test1/Sources
  MAIN_SOURCE main.swift
  BRIDGE_HEADER ${SWIFT_BRIDGE_HEADER}
  INCLUDE_DIR ${SWIFT_INCLUDE_DIR}
)

# test2_math - Math types test (vec3)
ork_add_swift_test(
  NAME test2_math
  SOURCE_DIR ${SWIFT_TESTS_DIR}/test2_math/test2_math/Sources
  MAIN_SOURCE main.swift
  BRIDGE_HEADER ${SWIFT_BRIDGE_HEADER}
  INCLUDE_DIR ${SWIFT_INCLUDE_DIR}
)

# test3_vec4 - Math types test (vec4)
ork_add_swift_test(
  NAME test3_vec4
  SOURCE_DIR ${SWIFT_TESTS_DIR}/test3_vec4/test3_vec4/Sources
  MAIN_SOURCE main.swift
  BRIDGE_HEADER ${SWIFT_BRIDGE_HEADER}
  INCLUDE_DIR ${SWIFT_INCLUDE_DIR}
)
