////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#include <ork/kernel/spawner.h>
#include <ork/kernel/string/string.h>
#include <ork/file/path.h>
#include <ork/orkstd.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <vector>
#include <string.h>
#include <sstream>
#include <mutex>

#if ! defined(WIN32)
#include <sys/wait.h>
#include <unistd.h>
#endif
//#include <boost/algorithm/string.hpp>

#if defined(__APPLE__)
    extern char** environ;
#endif

namespace ork {
///////////////////////////////////////////////////////////////////////////////
// Helper function to parse command line respecting quoted arguments
///////////////////////////////////////////////////////////////////////////////
static std::vector<std::string> parseCommandLine(const std::string& cmdline) {
    std::vector<std::string> args;
    std::string arg;
    bool in_quotes = false;
    bool in_single_quotes = false;
    bool escape_next = false;
    
    for (size_t i = 0; i < cmdline.length(); ++i) {
        char c = cmdline[i];
        
        if (escape_next) {
            // Handle escaped characters
            if (c == 'n') arg += '\n';
            else if (c == 't') arg += '\t';
            else if (c == 'r') arg += '\r';
            else arg += c;  // For \", \', \\, etc.
            escape_next = false;
        } else if (c == '\\') {
            escape_next = true;
        } else if (c == '"' && !in_single_quotes) {
            in_quotes = !in_quotes;
            // Don't skip empty quoted strings
            if (!in_quotes && arg.empty()) {
                args.push_back("");
            }
        } else if (c == '\'' && !in_quotes) {
            in_single_quotes = !in_single_quotes;
            // Don't skip empty quoted strings
            if (!in_single_quotes && arg.empty()) {
                args.push_back("");
            }
        } else if (c == ' ' && !in_quotes && !in_single_quotes) {
            if (!arg.empty()) {
                args.push_back(arg);
                arg.clear();
            }
        } else {
            arg += c;
        }
    }
    
    // Handle any remaining argument
    if (!arg.empty() || in_quotes || in_single_quotes) {
        args.push_back(arg);
    }
    
    return args;
}

///////////////////////////////////////////////////////////////////////////////
// Mutex for thread-safe PID management
///////////////////////////////////////////////////////////////////////////////
static std::mutex g_spawner_mutex;

///////////////////////////////////////////////////////////////////////////////
// process spawn utils
///////////////////////////////////////////////////////////////////////////////

Spawner::Spawner()
    : mExecRet(0)
#if ! defined(WIN32)
	, mChildPID(-1)
#endif
{

}

///////////////////////////////////////////////////////////////////////////////

Spawner::~Spawner()
{
#if ! defined(WIN32)
    std::lock_guard<std::mutex> lock(g_spawner_mutex);
	if( mChildPID > 0 )
    {
        //printf( "KILLING PID<%d>\n", mChildPID );
        sendSignal(SIGKILL);
        // Try to collect the zombie to clean up
        int status;
        waitpid(mChildPID, &status, WNOHANG);
        mChildPID = -1;
    }
#endif
}

///////////////////////////////////////////////////////////////////////////////

void Spawner::sendSignal (int sig) {
#if ! defined(WIN32)
    // Check if process exists before sending signal
    if (mChildPID > 0) {
        // kill with signal 0 tests if process exists
        if (kill(mChildPID, 0) == 0) {
            kill(mChildPID, sig);
        } else {
            // Process doesn't exist anymore
            mChildPID = -1;
        }
    }
#endif
}

void Spawner::spawnSynchronous(){
  spawn();
  collectZombie();
}

// Helper to free memory in child process
static void freeChildMemory(char** env_vars, size_t num_env_vars, char** args, size_t num_args) {
    if (env_vars) {
        for (size_t i = 0; i < num_env_vars; ++i) {
            free(env_vars[i]);
        }
        free(env_vars);
    }
    if (args) {
        for (size_t i = 0; i < num_args; ++i) {
            free(args[i]);
        }
        free(args);
    }
}

void Spawner::spawn()
{
#if ! defined(WIN32)
	mChildPID = fork();

    //printf( "fork<%d>\n", mChildPID );
    //fflush(stdout);

    if( 0 == mChildPID ) // child
    {
        /////////////////////////////////////////////////////////////
        // build environ
        /////////////////////////////////////////////////////////////

        const Environment::env_map_t& emap = mEnvironment.RefMap();

        size_t inum_vars = emap.size();

        char** env_vars = (char**) malloc(sizeof(char*)*(inum_vars+1));

        size_t icounter = 0;
        for( const auto& item : emap )
        {
            const std::string& k = item.first;
            const std::string& v = item.second;
            std::string VAR = k + "=" + v;
            env_vars[icounter] = strdup(VAR.c_str());
            //printf( "SETENV<%s>\n", env_vars[icounter] );
            icounter++;
        }
        env_vars[icounter] = 0; // terminate envvar array

        //printf( "child cp0 numenvvars<%d>\n", int(inum_vars) );
        //fflush(stdout);

        /////////////////////////////////////////////////////////////
        // build args
        /////////////////////////////////////////////////////////////

        std::vector<std::string> vargs = parseCommandLine(mCommandLine);

        //vargs.insert(vargs.begin(),vargs[0]);

        size_t inum_args = vargs.size();

        //printf( "child cp1 numargs<%d>\n", int(inum_args) );
        //fflush(stdout);

        char** args =  (char**) malloc(sizeof(char*)*(inum_args+1));

        file::DecomposedPath decomposed_path;

        for( int i=0; i<inum_args; i++ )
        {
            const std::string& arg = vargs[i];

            if( 0 == i )
            {
                auto argpath = file::Path(arg);
                argpath.decompose(decomposed_path);

                auto exe = decomposed_path.mFile + decomposed_path.mExtension;

                args[i] = strdup(exe.c_str());
            }
            else
            {
                args[i] = strdup(arg.c_str());
            }

            //printf( "arg<%d> <%s>\n", i, args[i] );

            //kernel::glog.printf( "spawn arg<%d:%s>\n", i, args[i] );
        }
        args[inum_args] = 0; // terminate arg array

        //printf( "child cp2\n" );
        //fflush(stdout);

        /////////////////////////////////////////////////////////////
        // set cwd
        /////////////////////////////////////////////////////////////

        if( mWorkingDirectory.length() )
        {
            //printf( "child changing to directory<%s>\n", mWorkingDirectory.c_str() );
            int iret = chdir( mWorkingDirectory.c_str() );
            if (iret != 0) {
                perror("chdir failed");
                freeChildMemory(env_vars, inum_vars, args, inum_args);
                _exit(1);
            }
        }

        /////////////////////////////////////////////////////////////
        // exec
        /////////////////////////////////////////////////////////////

        //printf( "child calling exec exe<%s>\n", args[0] );

        #if defined(__APPLE__)
        	::environ = env_vars;
        	if( decomposed_path.mFolder.length() ){
            	//printf( "folder<%s>\n", decomposed_path.mFolder.c_str() );
            	mExecRet = execvP(args[0], decomposed_path.mFolder.c_str(), args);
        	}
        	else
            	mExecRet = execvp(args[0], args);
        #else
            mExecRet = execvpe( args[0], args, env_vars );
        #endif

       // kernel::glog.printf( "exec failed <child> mExecRet<%d> ERRNO<%d>\n", mExecRet, errno );

        perror("EXEC FAILED");
        
        // Clean up allocated memory before exiting
        freeChildMemory(env_vars, inum_vars, args, inum_args);
        
        // Use _exit() to avoid calling destructors in forked child
        _exit(1);
    }
    else if( mChildPID<0 )
    {
        OrkAssertI(false, "fork() failed"); // failed to fork
    }
    else // parent
    {
        //printf( "Spawned child pid<%d>\n", mChildPID );
    }
#endif
}

bool Spawner::is_alive()
{
#if defined(WIN32)
	return false;
#else
	int status;
    int err = waitpid(mChildPID, &status, WNOHANG);

    if (-1 == err) {
        printf("Spawner<%p>::is_alive: waitpid: %s\n", (void*) this, strerror(errno));
        return false;
    }

    return 0 == err;
#endif
}

/** Block until the child changes state. */
void Spawner::collectZombie () {
#if defined(WIN32)
#else
    std::lock_guard<std::mutex> lock(g_spawner_mutex);
    if (mChildPID <= 0) {
        return; // Already collected
    }
    
	int status;
    int err = waitpid(mChildPID, &status, 0);

    if (-1 == err) {
        printf("Spawner<%p>::collectZombie: waitpid: %s\n", (void*) this, strerror(errno));
    }
    
    // Reset child PID after collection to prevent double-kill in destructor
    mChildPID = -1;
#endif
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork
