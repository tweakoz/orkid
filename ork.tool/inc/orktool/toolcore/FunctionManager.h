////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef MINIORKTOOL_FUNCTIONMANAGER_H
#define MINIORKTOOL_FUNCTIONMANAGER_H

#include <ork/kernel/core/singleton.h>
#include <ork/reflect/Functor.h>

namespace ork {

class FunctionManager : public NoRttiSingleton<FunctionManager>
{
	friend class ork::NoRttiSingleton<FunctionManager>;

public:
	typedef orkmap<const std::string, ork::reflect::IFunctor*> FunctionsByNameMap;

	void ExecuteFunction(tokenlist& tokens);

	void RegisterFunction(const std::string& name, ork::reflect::IFunctor* functor);

	const FunctionsByNameMap &GetFunctionsByName();

protected:

	FunctionsByNameMap mFunctionsByName;

private:

	FunctionManager();

};

}

#endif // MINIORKTOOL_FUNCTIONMANAGER_H
