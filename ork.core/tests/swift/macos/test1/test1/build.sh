#!/bin/bash

# Orkid Swift Test 1 - Build Script

set -e  # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
STAGING_DIR="${HOME}/.staging-sep26"
ORKID_INCLUDE="${SCRIPT_DIR}/../../../../../inc"
ORKID_LIB="${STAGING_DIR}/lib"

echo "=== Building Orkid Swift Test 1 ==="
echo "Script dir: ${SCRIPT_DIR}"
echo "Staging dir: ${STAGING_DIR}"
echo "Orkid include: ${ORKID_INCLUDE}"
echo "Orkid lib: ${ORKID_LIB}"
echo ""

# Compile Swift to object file
echo "Compiling Swift source..."
swiftc \
    -c Sources/main.swift \
    -o /tmp/test1.o \
    -import-objc-header "${ORKID_INCLUDE}/ork/swift/orkid_swift_bridge.h" \
    -I "${ORKID_INCLUDE}" \
    -Xcc -I"${ORKID_INCLUDE}" \
    -v

echo ""
echo "Linking executable..."
swiftc \
    /tmp/test1.o \
    -o test1 \
    -L "${ORKID_LIB}" \
    -lork_core \
    -Xlinker -rpath -Xlinker "${ORKID_LIB}" \
    -v

echo ""
echo "=== Build Complete ==="
echo "Executable: ${SCRIPT_DIR}/test1"
echo ""
echo "To run: ./test1"
