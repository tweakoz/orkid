////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef ORK_NOVA_ENT_PARTICLECONTROLLABLE_H
#define ORK_NOVA_ENT_PARTICLECONTROLLABLE_H
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/entity.h>
#include <ork/lev2/gfx/particle/particle.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/dataflow/dataflow.h>
#include <ork/math/gradient.h>
#include <ork/kernel/any.h>
#include <ork/lev2/gfx/particle/modular_particles.h>

using namespace ork::lev2::particle;

namespace ork { namespace lev2 { class RenderContextInstData; class CallbackRenderable; } }

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace psys {
///////////////////////////////////////////////////////////////////////////////

class NovaParticleSystem;

class NovaParticleItemBase : public ork::lev2::particle::ParticleItemBase
{
	RttiDeclareAbstract(NovaParticleItemBase, ork::lev2::particle::ParticleItemBase);
	
public:

	virtual NovaParticleSystem* CreateSystem( ork::ent::Entity* pent ) const { return 0; }

};

///////////////////////////////////////////////////////////////////////////////

class NovaParticleSystem : public ParticleSystemBase
{
	RttiDeclareAbstract(NovaParticleSystem, ParticleSystemBase);

public:

	NovaParticleSystem(const NovaParticleItemBase&pib) : ParticleSystemBase(pib), mDrawable(0) {}
	ork::ent::Drawable* GetDrawable() const { return mDrawable; }
	void LinkSystem( ork::ent::SceneInst* psi, ork::ent::Entity* pent ) { DoLinkSystem(psi,pent); }

	void StartSystem( const ork::ent::SceneInst* psi, ork::ent::Entity*pent);

	void SetName( const ork::PoolString& n ) { mName=n; }
	ork::PoolString GetName() const { return mName; }
	
protected:
	
	ork::ent::Drawable*	mDrawable;
	ork::PoolString mName;

private:

	virtual void DoLinkSystem( ork::ent::SceneInst* psi, ork::ent::Entity* pent ) {}
	virtual void DoStartSystem( const ork::ent::SceneInst* psi, ork::ent::Entity*pent ) {}
};

///////////////////////////////////////////////////////////////////////////////

class ParticleControllableData : public ork::ent::ComponentData
{
	///////////////////////////////////////////////////////
	RttiDeclareConcrete(ParticleControllableData, ork::ent::ComponentData)
	///////////////////////////////////////////////////////

public:
	///////////////////////////////////////////////////////
	ParticleControllableData();
	~ParticleControllableData();
	ork::ent::ComponentInst *CreateComponent(ork::ent::Entity *pent) const override;
	const ork::orklut<ork::PoolString,ParticleItemBase*>& GetItems() const { return mItems; }
	ork::PoolString GetEntAttachment() const { return mEntAttachment; }
	
	bool IsDefaultEnable() const { return mDefaultEnable; }
	void SetDefaultEnable(bool enable) { mDefaultEnable = enable; }
private:

	ork::orklut<ork::PoolString, ParticleItemBase*>	mItems;
	bool mDefaultEnable;
	ork::PoolString mEntAttachment;

	const char* GetShortSelector() const { return "psys"; } // virtual

};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ParticleControllableInst : public ork::ent::ComponentInst
{
	RttiDeclareAbstract(ParticleControllableInst, ork::ent::ComponentInst)

public:

	ParticleControllableInst(const ParticleControllableData &data, ork::ent::Entity *pent);
	~ParticleControllableInst();
	
	bool IsEnabled() const { return mbEnable; }
	void SetEnable(bool enable) { mbEnable = enable; }

	const ParticleControllableData&									GetData() const { return mData; }
	ork::lev2::particle::Context&									GetParticleContext() { return mParticleContext; }
	const ork::lev2::particle::Context&								GetParticleContext() const { return mParticleContext; }

	void Reset();

private:

	void DoUpdate(ork::ent::SceneInst *inst) override;
	bool DoNotify(const ork::event::Event *event) override;
	bool DoStart(ork::ent::SceneInst *inst, const ork::CMatrix4 &world) override;
	bool DoLink(ork::ent::SceneInst *inst ) override;
	const ParticleControllableData&	mData;
	bool							mbEnable;
	orkvector<NovaParticleSystem*>	mSystems;
	float 							mPrevTime;

	ork::lev2::particle::Context	mParticleContext;
	ork::ent::Entity*				mpAttachEntity;
};

///////////////////////////////////////////////////////////////////////////////

class ModParticleItem : public NovaParticleItemBase
{
	RttiDeclareConcrete(ModParticleItem, NovaParticleItemBase);

	mutable psys_graph								mTemplate;
	mutable ork::lev2::particle::psys_graph_pool	mpgraphpool;

	NovaParticleSystem* CreateSystem( ork::ent::Entity* pent ) const override;
	ork::Object* GraphPoolAccessor() { return & mpgraphpool; }
	ork::Object* TemplateAccessor() { return & mTemplate; }

	bool DoNotify(const event::Event *event) override;

	bool PostDeserialize(reflect::IDeserializer &ideser) override;

public:

	ModParticleItem();
	psys_graph_pool& GetGraphPool() const { return mpgraphpool; }
	psys_graph& GetTemplate() const { return mTemplate; }
};	

///////////////////////////////////////////////////////////////////////////////

class ModularSystem : public NovaParticleSystem
{
	RttiDeclareAbstract(ModularSystem, NovaParticleSystem);

public:

	const ModParticleItem&			mItem;

	ModularSystem( const ModParticleItem& item );
	~ModularSystem();

	void DoReset() override; 
	void DoUpdate(float fdt) override;
	psys_graph* GraphInstance() { return mGraphInstance; }
	int GetNumRenderers() const { return int(mRenderers.size()); }
	RendererModule* GetRenderer(int idx) { return mRenderers[idx]; }
	void SetEmitterEnable( bool bv );
	
	ork::lev2::particle::Context* GetParticleContext() 
	{
		ork::lev2::particle::Context* pctx =  mParticleControllerInst ? (&mParticleControllerInst->GetParticleContext()) : 0;
		return pctx;
	}

private:

	psys_graph*							mGraphInstance;
	orkvector<RendererModule*>			mRenderers;
	ParticleControllableInst*			mParticleControllerInst;

	void DoLinkSystem( ork::ent::SceneInst* psi, ork::ent::Entity* pent ) override;
	void DoStartSystem( const ork::ent::SceneInst* psi, ork::ent::Entity*pent ) override;


	bool DoNotify(const event::Event *event) override;
};

///////////////////////////////////////////////////////////////////////////////

class ParticleArchetype : public ork::ent::Archetype
{
	RttiDeclareConcrete(ParticleArchetype, ork::ent::Archetype);
public:
	ParticleArchetype();
	
private:
	void DoCompose(ork::ent::ArchComposer& composer) override; 
	void DoStartEntity(ork::ent::SceneInst*, const ork::CMatrix4& mtx, ork::ent::Entity* pent ) const override;
	void DoLinkEntity(ork::ent::SceneInst* inst, ork::ent::Entity *pent) const override;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} } // namespace prodigy::ent
///////////////////////////////////////////////////////////////////////////////
#endif // ORK_NOVA_ENT_PARTICLECONTROLLABLE_H
///////////////////////////////////////////////////////////////////////////////
