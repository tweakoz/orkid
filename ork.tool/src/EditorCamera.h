////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <pkg/ent/entity.h>
#include <pkg/ent/component.h>
#include <pkg/ent/componenttable.h>
#include <ork/math/TransformNode.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/camera/cameraman.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { class XgmModel; class GfxMaterial3DSolid; } }
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class EditorCamArchetype : public Archetype
{
	RttiDeclareConcrete( EditorCamArchetype, Archetype );

	void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const final {}
	void DoCompose(ork::ent::ArchComposer& composer) final;

public:

	EditorCamArchetype();

};

///////////////////////////////////////////////////////////////////////////////

class EditorCamControllerData : public ent::ComponentData
{
	RttiDeclareConcrete( EditorCamControllerData, ent::ComponentData );

	lev2::CCamera_persp*					mPerspCam;

    ent::ComponentInst* CreateComponent(ent::Entity* pent) const final;

public:


	EditorCamControllerData();
	const lev2::CCamera* GetCamera() const { return mPerspCam; }
	ork::Object* CameraAccessor() { return mPerspCam; }

};

///////////////////////////////////////////////////////////////////////////////

class EditorCamControllerInst : public ent::ComponentInst
{
	RttiDeclareAbstract( EditorCamControllerInst, ent::ComponentInst );

	const EditorCamControllerData&			mCD;
	
	void DoUpdate(ent::SceneInst* sinst) final;
    bool DoLink(SceneInst *psi) final;
    bool DoStart(SceneInst *psi, const CMatrix4 &world) final;

public:
	const EditorCamControllerData&	GetCD() const { return mCD; }

	EditorCamControllerInst( const EditorCamControllerData& cd, ork::ent::Entity* pent );

};

///////////////////////////////////////////////////////////////////////////////

} }

