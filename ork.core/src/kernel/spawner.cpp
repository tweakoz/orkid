////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/kernel/spawner.h>
#include <ork/kernel/string/string.h>
#include <ork/file/path.h>
#include <assert.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <vector>
#include <string.h>

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
	if( mChildPID > 0 )
    {
        printf( "KILLING PID<%d>\n", mChildPID );
        sendSignal(SIGKILL);
    }
#endif
}

///////////////////////////////////////////////////////////////////////////////

void Spawner::sendSignal (int sig) {
#if ! defined(WIN32)
	kill(mChildPID, sig);
#endif
}

void Spawner::spawnSynchronous(){
  spawn();
  collectZombie();
}

void Spawner::spawn()
{
#if ! defined(WIN32)
	mChildPID = fork();

    printf( "fork<%d>\n", mChildPID );
    fflush(stdout);

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
            printf( "SETENV<%s>\n", env_vars[icounter] );
            icounter++;
        }
        env_vars[icounter] = 0; // terminate envvar array

        //printf( "child cp0 numenvvars<%d>\n", int(inum_vars) );
        //fflush(stdout);

        /////////////////////////////////////////////////////////////
        // build args
        /////////////////////////////////////////////////////////////

        std::vector<std::string> vargs = SplitString(mCommandLine,' ');

        //vargs.insert(vargs.begin(),vargs[0]);

        size_t inum_args = vargs.size();

        printf( "child cp1 numargs<%d>\n", int(inum_args) );
        fflush(stdout);

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

            printf( "arg<%d> <%s>\n", i, args[i] );

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
            printf( "child changing to directory<%s>\n", mWorkingDirectory.c_str() );
            int iret = chdir( mWorkingDirectory.c_str() );
            assert(iret==0);
        }

        /////////////////////////////////////////////////////////////
        // exec
        /////////////////////////////////////////////////////////////

        printf( "child calling exec exe<%s>\n", args[0] );

        #if defined(__APPLE__)
        	::environ = env_vars;
        	if( decomposed_path.mFolder.length() ){
            	printf( "folder<%s>\n", decomposed_path.mFolder.c_str() );
            	mExecRet = execvP(args[0], decomposed_path.mFolder.c_str(), args);
        	}
        	else
            	mExecRet = execvp(args[0], args);
        #else
            mExecRet = execvpe( args[0], args, env_vars );
        #endif

       // kernel::glog.printf( "fork failed <child> mExecRet<%d> ERRNO<%d>\n", mExecRet, errno );

        perror("FORK FAILED");

        assert(false); // if exec fails, exit forked process
    }
    else if( mChildPID<0 )
    {
        assert(false); // failed to fork
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
	int status;
    int err = waitpid(mChildPID, &status, 0);

    if (-1 == err) {
        printf("Spawner<%p>::collectZombie: waitpid: %s\n", (void*) this, strerror(errno));
    }
#endif
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork
