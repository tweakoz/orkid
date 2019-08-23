///////////////////////////////////////////////////////////////////////////////
// MicroOrk (Orkid)
// Copyright 1996-2013, Michael T. Mayers
// Provided under the MIT License (see LICENSE.txt)
///////////////////////////////////////////////////////////////////////////////

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
#if !defined(WIN32)
	pid_t mChildPID;
#endif
	int mExecRet;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
