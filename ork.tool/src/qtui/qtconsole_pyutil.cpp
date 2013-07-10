////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <pkg/ent/editor/editor.h>
#include <pkg/ent/editor/edmainwin.h>
#include <ork/kernel/prop.h>
//#include <dispatch/dispatch.h>
#include <pkg/ent/ReferenceArchetype.h>

namespace e = ork::ent;

namespace ork {
namespace tool {
/////////////////////////////////////////////////////////////
static e::SceneEditorBase& get_editor()
{
	e::EditorMainWindow* pwin = e::gEditorMainWindow;
	return pwin->mEditorBase;
}
/////////////////////////////////////////////////////////////
void PyNewScene()
{
	ent::NewSceneReq nsr;
	get_editor().QueueOpSync(nsr);
}
/////////////////////////////////////////////////////////////
void PyNewEntity(const std::string& name,const std::string& archname="")
{
	std::string _archname = archname;
	
	e::Archetype* parch = (_archname!="") ? rtti::autocast(get_editor().FindSceneObject(_archname.c_str())) : nullptr;
	//////////////////////
	// attempt to make an appropriate archetype
	//////////////////////
	auto GenArch = [=](const std::string& alias,const std::string& aname,const std::string& classname) -> e::Archetype*
	{
		e::SceneObject* fso = get_editor().FindSceneObject(_archname.c_str());
		bool bMATCH = (_archname == alias) && (nullptr==fso);
		printf( "bmatch<%d> archname<%s> fso<%p>\n", int(bMATCH), _archname.c_str(), fso );
		e::Archetype* rarch = bMATCH ? get_editor().NewArchetype(classname) : nullptr;
		if( rarch )
		{
			get_editor().EditorRenameSceneObject( rarch, aname.c_str() );
		}
		return rarch;
	};

	if( nullptr == parch && (_archname=="") )
	{
		_archname = name;
	}


	if( nullptr == parch ) parch = GenArch( "ecam", "/arch/editcam", "EditorCamArchetype" );

	Future new_ent;
	e::NewEntityReq ner(new_ent);
	ner.mArchetype = parch;
	get_editor().QueueOpASync(ner);
	auto pent = new_ent.GetResult().Get<e::EntData*>();

	get_editor().EditorRenameSceneObject( pent, name.c_str() );
}
/////////////////////////////////////////////////////////////
void PyNewRefArch(const std::string& name)
{
	e::ReferenceArchetype* parch = rtti::autocast(get_editor().FindSceneObject(name.c_str()));
	if( 0 == parch )
	{
		parch = get_editor().NewReferenceArchetype(name);

	}
}
/////////////////////////////////////////////////////////////
}}
