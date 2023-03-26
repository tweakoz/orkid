////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
