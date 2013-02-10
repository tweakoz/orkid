////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <miniork_tool_pch.h>
#include <file/file.h>
//#include <file/tinyxml/tinyxml.h>
#include "SceneObjectFilter.h"

///////////////////////////////////////////////////////////////////////////////

#ifdef ORK_CONFIG_EDITORBUILD

namespace ork {
namespace tool {

///////////////////////////////////////////////////////////////////////////////

class SceneObjectFilterInterface : public ork::lev2::CAssetFilterInterface
{
public:
	virtual bool ConvertAsset( const std::string & inf, const std::string & outf );
	SceneObjectFilterInterface();
};

///////////////////////////////////////////////////////////////////////////////

SceneObjectFilter::SceneObjectFilter( const CClass *pClass )
	: CObject( pClass )
{
}

///////////////////////////////////////////////////////////////////////////////

void SceneObjectFilter::ManualClassInit( CClass *pClass )
{
	struct iface 
	{
		static IInterface::Context *GetInterface( void )
		{
			static SceneObjectFilterInterface iface;
			return & iface;
		}
	};

	pClass->AddNamedInterface( "Convert", reinterpret_cast<IInterface::Callback>( iface::GetInterface ) );
	pClass->AddNamedInterface( "ClearLog", reinterpret_cast<IInterface::Callback>( 0 ) );
	
}

///////////////////////////////////////////////////////////////////////////////

SceneObjectFilterInterface::SceneObjectFilterInterface()
	: CAssetFilterInterface( "sceneobjectfilter" )
{
}

///////////////////////////////////////////////////////////////////////////////

bool SceneObjectFilterInterface::ConvertAsset(const std::string& FromFileName, const std::string& ToFileName)
{
	ork::CMemoryManager::GetCurrentMemoryManager().Push();

	std::string ProjectSceneClassName = CSystem::GetGlobalStringVariable( "ProjectSceneClassName" );
	Scene *pscene = safe_cobject_downcast<Scene>(CClassManager::CreateObject( ProjectSceneClassName.c_str(), CMiniorkApplication::GetRef().GetCurrentContext() ));
	CMiniorkApplication::GetRef().GetCurrentContext()->SetScene("new",pscene);
	//pscene->SetApplication(mApplication);
	{
		//ork::CMiniorkApplication::SetSceneName(ork::CFileEnv::filespec_to_name(FromFileName).c_str());
		ork::ApplicationContext* applicationContext = ork::CMiniorkApplication::GetRef().GetCurrentContext();

		orkvector<ork::CObject*> deserialized;
		applicationContext->ObjectManager().DeSerializeFile(	FromFileName.c_str(),
																	deserialized,
																	applicationContext,
																	0 );

		OrkAssert(!deserialized.empty());

		SceneObject* psobj = safe_cobject_downcast<SceneObject>( deserialized[0] );

		if( psobj )
		{
			applicationContext->ObjectManager().SerializeFile( ToFileName.c_str(), psobj );
		}
		else
		{
			OrkNonFatalAssertI(false, "Scene Not Found!\n");
		}
	}
	//CMiniorkApplication::GetRef().GetCurrentContext()->SetScene(0);
	OrkDelete(pscene);
	ork::CMemoryManager::GetCurrentMemoryManager().Pop();

	return false;
}

///////////////////////////////////////////////////////////////////////////////

} 
}

#endif
