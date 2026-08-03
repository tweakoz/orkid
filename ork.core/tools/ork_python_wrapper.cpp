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

#if defined(__APPLE__)
#include <dlfcn.h>
#include <dirent.h>
#endif

//////////////////////////////////////////
// python launcher with OBT_STAGE support
//
// On Linux this exists to set DYLD_LIBRARY_PATH / sanitizer-injection env vars
// before execv'ing into the stage python, since SIP would otherwise strip those.
//
// On macOS we additionally need this binary to *be* the python process (not
// just exec into one). macOS 26+ enforces TCC privacy decisions (camera,
// microphone, etc.) against the running Mach-O image's CFBundleIdentifier.
// If we execv into pyvenv/bin/python3.12, TCC re-evaluates against that
// binary's identity (no embedded Info.plist), and AVCaptureSession silently
// drops all sample buffers even when AVAuthorizationStatus says Authorized.
//////////////////////////////////////////

#if defined(__APPLE__)
// Try to load libpython and call Py_BytesMain. Returns the exit code on
// success, or -1 if libpython couldn't be found / loaded. Caller is expected
// to fall back to execv on failure.
// Find libpython3.<minor>.dylib in pyvenv/lib without hard-coding the minor
// version, so this keeps working when the bundled python is bumped. Returns
// the full path, or empty string if none found.
static std::string find_libpython(const std::string& obt_stage) {
    const std::string libdir = obt_stage + "/pyvenv/lib";
    DIR* d = opendir(libdir.c_str());
    if (!d) return "";

    std::string match;
    while (dirent* ent = readdir(d)) {
        const std::string name = ent->d_name;
        // libpython3.<something>.dylib (skip plain "libpython3.dylib" symlinks too — they work either way)
        if (name.rfind("libpython3", 0) == 0 &&
            name.size() > 6 && name.compare(name.size() - 6, 6, ".dylib") == 0) {
            match = libdir + "/" + name;
            break;
        }
    }
    closedir(d);
    return match;
}

static int macos_inproc_python(int argc, char* argv[], const std::string& obt_stage) {
    const std::string libpython = find_libpython(obt_stage);

    void* handle = libpython.empty()
        ? nullptr
        : dlopen(libpython.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        // Fallback: rely on DYLD_LIBRARY_PATH we set above (resolves via SONAME)
        handle = dlopen("libpython3.dylib", RTLD_NOW | RTLD_GLOBAL);
    }
    if (!handle) {
        fprintf(stderr, "[ork.python] dlopen libpython (searched %s/pyvenv/lib) failed: %s\n",
                obt_stage.c_str(), dlerror());
        return -1;
    }

    using py_bytes_main_t = int (*)(int, char**);
    auto Py_BytesMain = reinterpret_cast<py_bytes_main_t>(dlsym(handle, "Py_BytesMain"));
    if (!Py_BytesMain) {
        fprintf(stderr, "[ork.python] dlsym Py_BytesMain failed: %s\n", dlerror());
        return -1;
    }

    // argv[0] must be the venv's python interpreter path (NOT a bare name) so
    // CPython's path init finds pyvenv.cfg and sets sys.prefix to this venv.
    // With a bare name it can't locate the venv and falls back to libpython's
    // compile-time baked prefix (the build host's staging dir), which doesn't
    // exist on a deployed machine -> Py_FatalError / Abort trap 6. This mirrors
    // the execv fallback below (obt_stage + "/pyvenv/bin/python3"). It only
    // changes what Python reports as sys.executable; the running Mach-O image
    // (and thus the TCC identity) is still this ork.python binary.
    std::vector<char*> py_argv;
    py_argv.reserve(static_cast<size_t>(argc) + 1);
    std::string argv0 = obt_stage + "/pyvenv/bin/python3";
    py_argv.push_back(const_cast<char*>(argv0.c_str()));
    for (int i = 1; i < argc; i++) {
        py_argv.push_back(argv[i]);
    }
    py_argv.push_back(nullptr);

    return Py_BytesMain(static_cast<int>(py_argv.size()) - 1, py_argv.data());
}
#endif

int main(int argc, char* argv[]) {
    // Get OBT_STAGE from environment
    const char* obt_stage = getenv("OBT_STAGE");
    if (!obt_stage) {
        fprintf(stderr, "Error: OBT_STAGE environment variable not set\n");
        return 1;
    }

    // orkid's sub-interpreter machinery (ork::python::Context2) is GIL-OFF by design.
    // Under PYTHON_GIL=1 the sub-interpreter's GIL becomes a real lock and CPython's
    // first-import-of-a-C-extension rule cross-attaches to the MAIN interpreter's GIL
    // (import_run_extension -> switch_to_main_interpreter), which deadlocks against a
    // render thread holding _subInterpMutex — see test_gil_ecs_regression.py. The 0
    // (don't-clobber) form so an explicitly-set PYTHON_GIL from the caller survives.
    setenv("PYTHON_GIL", "0", 0);

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

#if defined(__APPLE__)
    // Run python in-process so TCC sees the embedded Info.plist on this binary.
    int rc = macos_inproc_python(argc, argv, std::string(obt_stage));
    if (rc >= 0) return rc;
    // Fall through to execv on failure (e.g. libpython not findable).
    fprintf(stderr, "[ork.python] in-process python failed; falling back to execv\n");
#endif

    // Build path to orkids custom python executable
    std::string python_path = std::string(obt_stage) + "/pyvenv/bin/python3";

    // Build argument list for execv.
    // argv[0] MUST be the venv's python interpreter path (NOT a bare name) so
    // CPython's path init finds pyvenv.cfg and sets sys.prefix to this venv.
    // With a bare name ("ork.python") it can't locate the venv and falls back
    // to libpython's compile-time baked prefix (the build host's staging dir) —
    // which loads the wrong orkengine on the build host and, on a deployed
    // machine where that dir is absent, aborts with Py_FatalError. This mirrors
    // the macOS in-process path above (macos_inproc_python), which sets argv0 to
    // this same path for the identical reason.
    std::vector<char*> exec_args;
    exec_args.push_back(strdup(python_path.c_str()));

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
