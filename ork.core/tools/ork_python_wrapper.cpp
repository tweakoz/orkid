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

    // Auto-detect sanitizer: compile-time definition from CMake (-DSANITIZER=ADDRESS),
    // with runtime override via ORK_SANITIZER env var.
    // This ensures the sanitizer runtime is pre-loaded via DYLD_INSERT_LIBRARIES
    // before Python loads any .so files (required because SIP strips DYLD_INSERT_LIBRARIES
    // from child processes, but this wrapper is a native binary that exec's Python).
    {
        std::string san;
#ifdef ORK_SANITIZER
        // Built with sanitizer — auto-inject unless explicitly disabled
        const char* env_san = getenv("ORK_SANITIZER");
        if (env_san && std::string(env_san) == "off") {
            // Allow ORK_SANITIZER=off to suppress injection
        } else {
            san = ORK_SANITIZER;
        }
#else
        // Not built with sanitizer — only inject if explicitly requested
        const char* env_san = getenv("ORK_SANITIZER");
        if (env_san) san = env_san;
#endif
        // Normalize to lowercase
        for (auto& c : san) c = tolower(c);

        std::string lib_name;
        if (san == "address")        lib_name = "libclang_rt.asan_osx_dynamic.dylib";
        else if (san == "thread")    lib_name = "libclang_rt.tsan_osx_dynamic.dylib";
        else if (san == "undefined") lib_name = "libclang_rt.ubsan_osx_dynamic.dylib";

        if (!lib_name.empty()) {
            FILE* pipe = popen("xcrun clang --print-resource-dir 2>/dev/null", "r");
            if (pipe) {
                char buf[512];
                std::string resource_dir;
                while (fgets(buf, sizeof(buf), pipe))
                    resource_dir += buf;
                pclose(pipe);
                while (!resource_dir.empty() && (resource_dir.back() == '\n' || resource_dir.back() == '\r'))
                    resource_dir.pop_back();
                if (!resource_dir.empty()) {
                    std::string dylib_path = resource_dir + "/lib/darwin/" + lib_name;
                    setenv("DYLD_INSERT_LIBRARIES", dylib_path.c_str(), 1);
                    fprintf(stderr, "[ork.python_wrapper] Injecting sanitizer: %s\n", dylib_path.c_str());
                }
            }
        }
    }


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