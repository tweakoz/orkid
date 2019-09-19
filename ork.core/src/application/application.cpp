////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/rtti/Class.h>
#include <ork/kernel/string/ResizableString.h>
#include <ork/kernel/string/PoolString.h>

#include <ork/util/Context.hpp>

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::Application, "Application");

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

PoolString AddPooledString(const PieceString &ps) { return Application::AddPooledString(ps); }
PoolString AddPooledLiteral(const ConstString &cs) { return Application::AddPooledLiteral(cs); }
PoolString FindPooledString(const PieceString &ps) { return Application::FindPooledString(ps); }

Application* Application::gctx = 0;

Application::Application()
{
	OrkAssert(gctx==0);
	gctx = this;
}

///////////////////////////////////////////////////////////////////////////////

anyp Application::GetProperty( const std::string& key ) const
{
    auto it=mAppPropertyMap.find(key);
    auto rval = (it==mAppPropertyMap.end())?anyp():it->second;
    return rval;
}
void Application::SetProperty( const std::string& key, anyp val )
{
    mAppPropertyMap[key]=val;
}

///////////////////////////////////////////////////////////////////////////////

PoolString Application::AddPooledString(const PieceString &string)
{
	PoolString result = FindPooledString(string);
	if(result)
		return result;

	ResizableString copy(string);
	const char *data = copy.c_str();
	new(&copy) ResizableString;

    ork::Application* papp = ApplicationStack::Top();
	OrkAssert(papp);

	return papp->GetStringPool().String(data);
}

///////////////////////////////////////////////////////////////////////////////

PoolString Application::AddPooledLiteral(const ConstString &string)
{
	PoolString result = FindPooledString(string);
	if(result)
		return result;

    ork::Application* papp = ApplicationStack::Top();
	OrkAssert(papp);

	return papp->GetStringPool().Literal(string);
}

///////////////////////////////////////////////////////////////////////////////

PoolString Application::FindPooledString(const PieceString &string)
{
	//OrkAssert(ork::util::Context<ork::Application>::GetContext());

    auto pAPP = ApplicationStack::Top();
    PoolString result = pAPP->GetStringPool().Find(string);
    if(result)
        return result;

//	for(auto ApplicationStack::;
//			application; application = application->PreviousContext())
//	{
//		PoolString result = application->GetStringPool().Find(string);
//		if(result)
//			return result;
//	}
	return PoolString();
}

///////////////////////////////////////////////////////////////////////////////

void Application::Describe()
{
}

PoolString operator"" _pool(const char* s, size_t len) {
  return AddPooledString(s);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

template class ork::util::GlobalStack<ork::Application*>;
