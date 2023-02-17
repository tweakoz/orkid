////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

	void init_from_envp(char** envp);
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
