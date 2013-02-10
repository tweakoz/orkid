////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <pkg/ent/component.h>
#include <pkg/ent/componenttable.h>
#include <ork/math/TransformNode.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/gfx/camera.h>
#include <ork/lev2/gfx/proctex/proctex.h>
#include <ork/kernel/timer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { class XgmModel; class GfxMaterial3DSolid; } }
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class ProcTexControllerData : public ent::ComponentData
{
	RttiDeclareConcrete( ProcTexControllerData, ent::ComponentData );

public:

	ProcTexControllerData();

	proctex::ProcTex& GetTemplate() const { return mTemplate; }

private:

	virtual ent::ComponentInst* CreateComponent(ent::Entity* pent) const;

	ork::Object* TemplateAccessor() { return & mTemplate; }


	mutable proctex::ProcTex mTemplate;

};

///////////////////////////////////////////////////////////////////////////////

class ProcTexControllerInst : public ent::ComponentInst
{
	RttiDeclareAbstract( ProcTexControllerInst, ent::ComponentInst );

	const ProcTexControllerData&		mCD;

	virtual void DoUpdate(ent::SceneInst* sinst);

public:
	const ProcTexControllerData&	GetCD() const { return mCD; }

	proctex::ProcTexContext mContext;
	ork::Timer 				mTimer;
	
	ProcTexControllerInst( const ProcTexControllerData& cd, ork::ent::Entity* pent );
};

///////////////////////////////////////////////////////////////////////////////

class ProcTexArchetype : public Archetype
{
	RttiDeclareConcrete( ProcTexArchetype, Archetype );

	/*virtual*/ void DoLinkEntity( SceneInst* psi, Entity *pent ) const;
	/*virtual*/ void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const {}
	/*virtual*/ void DoCompose(ork::ent::ArchComposer& composer);

public:

	ProcTexArchetype();

};

///////////////////////////////////////////////////////////////////////////////

} }
