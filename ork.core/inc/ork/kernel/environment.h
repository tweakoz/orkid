////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <string>

namespace ork {
namespace file {
	class Path;
}
///////////////////////////////////////////////////////////////////////////////

class Environment
{
public:
	typedef std::map<std::string,std::string> env_map_t;

	Environment();

	//void init_from_envp(char** envp);
	void init_from_global_env();


	void set( const std::string& k, const std::string& v );
	bool has( const std::string& k ) const;
	bool get( const std::string& k, std::string& vout ) const;
	void appendPath( const std::string& k, const file::Path& v );
	void prependPath( const std::string& k, const file::Path& v );

	void dump() const;

	const env_map_t& RefMap() const { return mEnvMap; }

private:

	env_map_t mEnvMap;

};

extern Environment genviron;

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
