////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <string>

//////////////////////////////////////////
// python launcher with OBT_STAGE support
// this is mostly needed for macOS (to overcome SIP restrictions on DYLD_LIBRARY_PATH)
//   DYLD_LIBRARY_PATH is currently needed for Vulkan SDK ICD mechanism
// maybe it will do more in the future
// maybe it will be removed in the future
// for now it takes the place of ork.python in orkid extended python commands with shebangs
//////////////////////////////////////////

int main(int argc, char* argv[]) {
    // Get OBT_STAGE from environment
    const char* obt_stage = getenv("OBT_STAGE");
    if (!obt_stage) {
        fprintf(stderr, "Error: OBT_STAGE environment variable not set\n");
        return 1;
    }
    
    // Build DYLD_LIBRARY_PATH
    std::string dyld_path = std::string(obt_stage) + "/lib:/opt/homebrew/lib";
    
    // Check if DYLD_LIBRARY_PATH already exists and append if so
    const char* existing_dyld = getenv("DYLD_LIBRARY_PATH");
    if (existing_dyld && strlen(existing_dyld) > 0) {
        dyld_path = dyld_path + ":" + existing_dyld;
    }
    
    // Set the environment variable
    setenv("DYLD_LIBRARY_PATH", dyld_path.c_str(), 1);
    //setenv("DYLD_INSERT_LIBRARIES", "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/17/lib/darwin/libclang_rt.tsan_osx_dynamic.dylib", 1 );
    setenv("DYLD_INSERT_LIBRARIES", "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/clang/17/lib/darwin/libclang_rt.asan_osx_dynamic.dylib", 1 );


    // Build path to orkids custom python executable
    std::string python_path = std::string(obt_stage) + "/pyvenv/bin/python3";
    
    // Build argument list for execv
    std::vector<char*> exec_args;
    exec_args.push_back(strdup("ork.python"));
    
    // Add all original arguments (script name and any additional args)
    for (int i = 1; i < argc; i++) {
        exec_args.push_back(argv[i]);
    }
    exec_args.push_back(nullptr);
    
    // Execute ork.python with the modified environment
    execv(python_path.c_str(), exec_args.data());
    
    // If we get here, execv failed
    perror("execv failed");
    fprintf(stderr, "Failed to execute: %s\n", python_path.c_str());
    return 1;
}