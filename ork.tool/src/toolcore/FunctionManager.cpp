////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>

#include <ork/orktypes.h>

#include <orktool/toolcore/FunctionManager.h>

namespace ork {

void FunctionManager::ExecuteFunction(tokenlist& tokens)
{
	FunctionsByNameMap::iterator it = mFunctionsByName.find(tokens.front());
	if(it != mFunctionsByName.end())
	{
		ork::reflect::IInvokation* invokation = it->second->CreateInvokation();

		tokens.pop_front();

		int param = 0;
		for(ork::tokenlist::iterator iter = tokens.begin(); iter != tokens.end(); ++iter, param++)
		{
			if(param >= invokation->GetNumParameters())
				break;
			SetInvokationParameter(invokation, param, (*iter).c_str());
		}

		it->second->invoke(invokation);

		delete invokation;
	}
}

void FunctionManager::RegisterFunction(const std::string& name, ork::reflect::IFunctor* functor)
{
	mFunctionsByName[name] = functor;
}

const FunctionManager::FunctionsByNameMap &FunctionManager::GetFunctionsByName()
{
	return mFunctionsByName;
}

FunctionManager::FunctionManager() {}

}
