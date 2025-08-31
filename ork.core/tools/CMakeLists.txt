project(ork.python.wrapper)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unknown-pragmas -Wno-unused-variable -Wno-unused-parameter -Wno-missing-field-initializers")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")

add_executable(ork.python.wrapper ork_python_wrapper.cpp)
target_link_libraries(ork.python.wrapper PRIVATE c m )
set_target_properties(ork.python.wrapper PROPERTIES OUTPUT_NAME "ork.python")
install(TARGETS ork.python.wrapper DESTINATION $ENV{OBT_SUBSPACE_BIN_DIR} )
