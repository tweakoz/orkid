////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _MINIORK_TOOL_FILTER_SCENEOBJECT
#define _MINIORK_TOOL_FILTER_SCENEOBJECT

namespace ork {
namespace tool {

class SceneObjectFilter : public CObject 
{
public:

	static void ManualClassInit(CClass *pClass);

	static std::string GetClassName() { return std::string("ork::tool::SceneObjectFilter"); }

	static CObject *ManualObjectFactory(const CClass *pclass, IZoneManager* pZone)
	{
		return TManualObjectFactory<SceneObjectFilter>(pclass, pZone);
	}

	const static CClass* spKernelClass;

	SceneObjectFilter(const CClass *pClass);


};

} 
}

#endif
