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
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/proctex/proctex.h>
#include <ork/kernel/timer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { class XgmModel; class GfxMaterial3DSolid; } }
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class ProcTexOutputBase : public ork::Object
{
	RttiDeclareAbstract( ProcTexOutputBase, ork::Object );
public:
	void DoLinkEntity( Simulation* psi, Entity *pent );
protected:
	ProcTexOutputBase();
	virtual void OnLinkEntity( Simulation* psi, Entity *pent ) = 0;
	float _timeperupdate = 0.0f;
	float _elapsed = 0.0f;
	ork::Timer _timer;
};

class ProcTexOutputQuad : public ProcTexOutputBase
{
	RttiDeclareConcrete( ProcTexOutputQuad, ProcTexOutputBase );
	void OnLinkEntity( Simulation* psi, Entity *pent ) final;
public:
	ProcTexOutputQuad();
	mutable lev2::GfxMaterial3DSolid* mMaterial;
	float mScale;
};

class ProcTexOutputSkybox : public ProcTexOutputBase
{
	RttiDeclareConcrete( ProcTexOutputSkybox, ProcTexOutputBase );
	void OnLinkEntity( Simulation* psi, Entity *pent ) final;
public:
	ProcTexOutputSkybox();
	float mVerticalAdjust;
	float mScale;
	mutable lev2::GfxMaterial3DSolid* mMaterial;
};

class ProcTexOutputDynTex : public ProcTexOutputBase
{
	RttiDeclareConcrete( ProcTexOutputDynTex, ProcTexOutputBase );
public:
	ProcTexOutputDynTex();
	~ProcTexOutputDynTex();
	void OnLinkEntity( Simulation* psi, Entity *pent ) final;
	ork::PoolString mDynTexPath;
	mutable lev2::TextureAsset* mAsset;
};

class ProcTexOutputBake : public ProcTexOutputBase
{
	RttiDeclareConcrete( ProcTexOutputBake, ProcTexOutputBase );
public:
	ProcTexOutputBake();
	void OnLinkEntity( Simulation* psi, Entity *pent ) final;
	//int GetNumExportFrames() const { return mNumExportFrames; }
	//bool IsBaking() const { return mPerformingBake; }
	//void IncrementFrame() const;
	//int GetFrameIndex() const { return mBakeFrameIndex; }
	//const file::Path& GetWritePath() const { return mWritePath; }
	ork::PoolString mDynTexPath;
	int mNumExportFrames;
	bool mPerformingBake;
	int mBakeFrameIndex;
	ork::PoolString mWritePath;

};

///////////////////////////////////////////////////////////////////////////////

class ProcTexControllerData : public ent::ComponentData
{
	RttiDeclareConcrete( ProcTexControllerData, ent::ComponentData );

public:

	ProcTexControllerData();
	~ProcTexControllerData();

	proctex::ProcTex& GetTemplate() const { return mTemplate; }

	int GetBufferDim() const { return mBufferDim; }
	float GetMaxFrameRate() const { return mMaxFrameRate; }

	bool NeedsRefresh() const { return mNeedsRefresh; }
	void DidRefresh() const { mNeedsRefresh=false; }

	ProcTexOutputBase* GetOutput() const { return mOutput; }

private:

	ent::ComponentInst* createComponent(ent::Entity* pent) const final;

	ork::Object* TemplateAccessor() { return & mTemplate; }
	void OutputGetter(ork::rtti::ICastable*& val) const;
	void OutputSetter(ork::rtti::ICastable* const & val);

	mutable proctex::ProcTex mTemplate;
	int mBufferDim;
	float mMaxFrameRate;
	mutable bool mNeedsRefresh;
	ProcTexOutputBase* mOutput;
};

///////////////////////////////////////////////////////////////////////////////

class ProcTexControllerInst : public ent::ComponentInst
{
	RttiDeclareAbstract( ProcTexControllerInst, ent::ComponentInst );

	const ProcTexControllerData&		mCD;

	void DoUpdate(ent::Simulation* sinst) final;

public:
	const ProcTexControllerData&	GetCD() const { return mCD; }

	proctex::ProcTexContext mContext;

	ProcTexControllerInst( const ProcTexControllerData& cd, ork::ent::Entity* pent );
};

///////////////////////////////////////////////////////////////////////////////

class ProcTexArchetype : public Archetype
{
	RttiDeclareConcrete( ProcTexArchetype, Archetype );

	void DoLinkEntity( Simulation* psi, Entity *pent ) const final;
	void DoStartEntity(Simulation* psi, const fmtx4 &world, Entity *pent ) const final {}
	void DoCompose(ork::ent::ArchComposer& composer) final;

public:

	ProcTexArchetype();

};

///////////////////////////////////////////////////////////////////////////////

} }
