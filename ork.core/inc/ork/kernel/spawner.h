////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/environment.h>
#if !defined(WIN32)
#include <sys/wait.h>
#endif

namespace ork{
///////////////////////////////////////////////////////////////////////////////

struct Spawner
{
	Spawner();
	~Spawner();

	void spawn();
	void spawnSynchronous();
	bool is_alive();
  void sendSignal (int sig);
  void collectZombie ();

	Environment mEnvironment;
	std::string mWorkingDirectory;
	std::string mCommandLine;

	int mExecRet;

#if !defined(WIN32)
	pid_t mChildPID;
#endif

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
