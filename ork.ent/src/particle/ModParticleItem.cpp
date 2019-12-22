////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/enum_serializer.inl>
#include <pkg/ent/ParticleControllable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>

#include <ork/kernel/Array.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <pkg/ent/PerfController.h>
#include <pkg/ent/dataflow.h>

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::psys::ModParticleItem, "ModParticleItem");
INSTANTIATE_TRANSPARENT_RTTI(ork::psys::ModularSystem, "ModularSystem");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace psys {
///////////////////////////////////////////////////////////////////////////////

static bool gbusetemplate = false;

bool ModParticleItem::DoNotify(const event::Event* event) {
  if (const ObjectGedVisitEvent* pev = rtti::autocast(event)) {
    gbusetemplate = true;
    return true;
  }
  return false;
}

void ModularSystem::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

ModParticleItem::ModParticleItem() {
}

///////////////////////////////////////////////////////////////////////////////

void ModParticleItem::Describe() {
  ork::reflect::RegisterProperty("GraphPool", &ModParticleItem::GraphPoolAccessor);
  ork::reflect::RegisterProperty("Template", &ModParticleItem::TemplateAccessor);
}

//////////////////////////////////////////////////////////////////////////////

struct ModItemRenderData;

struct ModItemBufferDataDB {
  ork::lev2::particle::ParticlePoolRenderBuffer mParticleBuffer;
  // ork::recursive_mutex							mBufferMutex;
  float mfTimeElapsed;
  float mfStartTime;
  fmtx4 mMatrix;

  ModItemBufferDataDB() {
  } //: mBufferMutex("ModItemBufferDataDB::Mutex") {}
  ~ModItemBufferDataDB() {
  }

  //	void LockMIBDDB(int lid) { mBufferLock.Lock(lid); }
  // void UnLockMIBDDB() { mBufferLock.UnLock(); }
};

class ModItemBufferData {
public:
  ModItemBufferData()
      : mpEntity(0)
      , mpRendererModule(0)
      , mpDrawable(0)
      , mpMIRD(0)
      , mDB(ork::lev2::DrawableBuffer::kmaxbuffers + 1) {
  }
  ~ModItemBufferData() {
  }

  const ork::ent::Entity* mpEntity;
  RendererModule* mpRendererModule;
  lev2::CallbackDrawable* mpDrawable;
  ModItemRenderData* mpMIRD;

  ModItemBufferDataDB& GetDB(size_t idx) {
    if (idx > ork::lev2::DrawableBuffer::kmaxbuffers)
      idx = ork::lev2::DrawableBuffer::kmaxbuffers;

    return mDB[idx];
  }
  const ModItemBufferDataDB& GetDB(size_t idx) const {
    if (idx > ork::lev2::DrawableBuffer::kmaxbuffers)
      idx = ork::lev2::DrawableBuffer::kmaxbuffers;

    return mDB[idx];
  }

private:
  ork::Array<ModItemBufferDataDB> mDB;
};

struct ModItemRenderData {
  ork::ent::Entity* mEntity;
  const ModParticleItem* mpItem;
  ModularSystem* mpSystem;
  RendererModule* mpRenderer;
  ModItemBufferData mMIBD;

  ModItemRenderData(ork::ent::Entity* pent, const ModParticleItem* item, ModularSystem* system, RendererModule* prend)
      : mEntity(pent)
      , mpItem(item)
      , mpSystem(system)
      , mpRenderer(prend) {
    mMIBD.mpMIRD           = this;
    mMIBD.mpEntity         = pent;
    mMIBD.mpRendererModule = prend;
  }
  ~ModItemRenderData() {
    mEntity = 0;
  }
  static void QueueToLayerCallback(lev2::DrawableBufItem& cdb) {
    ork::opq::assertOnQueue2(updateSerialQueue());

    ModItemRenderData* pmird = cdb.mUserData0.Get<ModItemRenderData*>();

    if (0 == pmird->mpItem) {
      return;
    }

    ModItemBufferData& mibd      = pmird->mMIBD;
    const ork::ent::Entity* pent = mibd.mpEntity;

    int ibufindex = cdb.miBufferIndex;

    ModItemBufferDataDB& db = mibd.GetDB(ibufindex);
    {
      cdb.mUserData1        = lev2::DrawableBufItem::var_t(&db);
      ModularSystem& System = *pmird->mpSystem;
      db.mfTimeElapsed      = System.GetElapsed();

      db.mfStartTime = pmird->mpItem->GetStartTime();

      db.mMatrix = fmtx4::Identity;
      if (false == pmird->mpItem->IsWorldSpace()) {
        pent->GetDagNode().GetMatrix(db.mMatrix);
      }
      if (pmird->mpRenderer) {
        const ork::lev2::particle::Pool<ork::lev2::particle::BasicParticle>* ptclpool = pmird->mpRenderer->GetPool();
        if (ptclpool) {
          db.mParticleBuffer.Update(*ptclpool);
        }
      }
    }
  }
  static void enqueueToRenderQueueCallback(
      ork::lev2::RenderContextInstData& rcid,
      ork::lev2::GfxTarget* targ,
      const ork::lev2::CallbackRenderable* pren) {
    ork::opq::assertOnQueue2(mainThreadQueue());

    //////////////////////////////////////////
    if (targ->FBI()->IsPickState())
      return;
    //////////////////////////////////////////

    ModItemRenderData* pdata = pren->GetDrawableDataA().Get<ModItemRenderData*>();
    ModItemBufferData& mibd  = pdata->mMIBD;

    ModItemBufferDataDB* pbufd = pren->GetUserData1().Get<ModItemBufferDataDB*>();

    ModItemBufferDataDB& db = (*pbufd);
    {
      const ModParticleItem& Item = *pdata->mpItem;
      ModularSystem& System       = *pdata->mpSystem;
      //////////////////////////////////////////
      if (db.mfTimeElapsed < db.mfStartTime)
        return;
      //////////////////////////////////////////
      RendererModule* Renderer = pdata->mpRenderer;
      Renderer->Render(db.mMatrix, rcid, db.mParticleBuffer, targ);
      //////////////////////////////////////////
    }
  }
};

//////////////////////////////////////////////////////////////////////////////

NovaParticleSystem* ModParticleItem::CreateSystem(
    ork::ent::Entity* pent) const { //////////////////////////////////////////////////////////////////////////////
  ModularSystem* psystem = new ModularSystem(*this);
  return psystem;
}

bool ModParticleItem::PostDeserialize(reflect::IDeserializer& ideser) {
  mpgraphpool.BindTemplate(mTemplate);
  return true;
}

//////////////////////////////////////////////////////////////////////////////

ModularSystem::ModularSystem(const ModParticleItem& item)
    : NovaParticleSystem(item)
    , mItem(item)
    , mGraphInstance(0) {
  mElapsed = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

ModularSystem::~ModularSystem() {
  psys_graph* ptemplate = &mItem.GetTemplate();
  bool bISTEMPLATE      = (mGraphInstance == ptemplate);
  orkprintf("ModularSystem::~ModularSystem this<%p>\n", this);
  orkprintf("    GI<%p>\n", mGraphInstance);
  orkprintf("    TEMPL<%p>\n", ptemplate);
  if (mGraphInstance && (false == bISTEMPLATE)) {
    mItem.GetGraphPool().Free(mGraphInstance);
  }
}

///////////////////////////////////////////////////////////////////////////////

void ModularSystem::SetEmitterEnable(bool bv) {
  if (mGraphInstance) {
    mGraphInstance->SetEmitEnable(bv);
  }
}

///////////////////////////////////////////////////////////////////////////////

void ModularSystem::DoUpdate(float fdt) {
  ork::lev2::particle::Context* pctx = GetParticleContext();

  if (mGraphInstance) {
    mGraphInstance->Update(pctx, fdt);
  }
}

///////////////////////////////////////////////////////////////////////////////

bool ModularSystem::DoNotify(const event::Event* event) {
  if (const ork::ent::PerfSnapShotEvent* psse = rtti::autocast(event)) {
    psse->PushNode(mName.c_str()); // push psys name
    psys_graph& template_graph                                    = mItem.GetTemplate();
    const orklut<ork::PoolString, ork::Object*>& template_modules = template_graph.Modules();
    for (orklut<ork::PoolString, ork::Object*>::const_iterator itm = template_modules.begin(); itm != template_modules.end();
         itm++) {
      const ork::PoolString& module_name = itm->first;
      dataflow::dgmodule* pmodule        = rtti::autocast(itm->second);

      psse->PushNode(module_name.c_str()); // push psys name
      static_cast<ork::Object*>(pmodule)->Notify(psse);
      psse->PopNode();
    }
    psse->PopNode();
  } else if (const ork::ent::PerfControlEvent* pce = rtti::autocast(event)) {
    PoolString k = AddPooledString(pce->mTarget.c_str());

    ent::PerfControlEvent pce2 = *pce;
    std::string KeyName        = pce2.mTarget.c_str();
    std::string SystemName     = pce2.PopTargetNode();
    std::string ModuleName     = pce2.PopTargetNode();

    printf(
        "ModularSystem<%p:%s> PerfControlEvent<%p> key<%s> ModuleName<%s>\n",
        this,
        mName.c_str(),
        pce,
        k.c_str(),
        ModuleName.c_str());

    if (0 == strcmp(SystemName.c_str(), mName.c_str())) {
      psys_graph& template_graph = mItem.GetTemplate();

      dataflow::dgmodule* pmodule = template_graph.GetChild(AddPooledString(ModuleName.c_str()));
      printf(" Module<%s:%p>\n", ModuleName.c_str(), pmodule);
      static_cast<ork::Object*>(pmodule)->Notify(&pce2);
    }
  }
  return true;
}

void ModularSystem::DoReset() {
  // orkprintf( "DoReset modular psys<%p>\n", this );
}

void ModularSystem::DoStartSystem(const ork::ent::Simulation* psi, ork::ent::Entity* pent) {
  // orkprintf( "DoStartSystem modular psys<%p>\n", this );

  if (mGraphInstance) {
    ork::ent::DataflowRecieverComponentInst* dflowreciever = pent->GetTypedComponent<ork::ent::DataflowRecieverComponentInst>();

    if (dflowreciever) {
      ork::dataflow::dyn_external& dgmod = dflowreciever->RefExternal();
      mGraphInstance->BindExternal(&dgmod);
    }

    mGraphInstance->PrepForStart();

  } else {
    orkprintf("WARNING NOT ENOUGH GRAPH INSTANCES\n");
  }
}

///////////////////////////////////////////////////////////////////////////////

void ModularSystem::DoLinkSystem(ork::ent::Simulation* psi, ork::ent::Entity* pent) {
  ///////////////////////////////////////
  mParticleControllerInst = pent->GetTypedComponent<ork::psys::ParticleControllableInst>();
  OrkAssert(mParticleControllerInst != 0);
  ////////////////////////////////////////

  mGraphInstance = gbusetemplate ? &mItem.GetTemplate() : mItem.GetGraphPool().Allocate();
  orkprintf("ModularSystem::DoLinkSystem this<%p>\n", this);
  orkprintf("    GI<%p>\n", mGraphInstance);
  orkprintf("    TEMPL<%p>\n", &mItem.GetTemplate());

  if (mGraphInstance) {
    if (mGraphInstance->IsComplete()) {
      size_t inummods = mGraphInstance->Modules().size();
      for (size_t im = 0; im < inummods; im++) {
        std::pair<PoolString, ork::Object*> pr = mGraphInstance->Modules().GetItemAtIndex(im);
        Module* pmod                           = rtti::autocast(pr.second);
        if (nullptr == pmod)
          continue;
        RendererModule* prenderer_mod = rtti::autocast(pr.second);
        if (prenderer_mod) {
          mRenderers.push_back(prenderer_mod);
        }
        ork::lev2::particle::Context* ctx = &mParticleControllerInst->GetParticleContext();
        pmod->Link(ctx);
        ///////////////////////////////
        // link module to template
        ///////////////////////////////
        std::pair<PoolString, ork::Object*> pr2 = mItem.GetTemplate().Modules().GetItemAtIndex(im);
        Module* pmod_template                   = rtti::autocast(pr2.second);
        pmod->SetTemplateModule(pmod_template);
      }
    }

    float fsort = mItem.GetSortValue();
    int isort   = (0x7f << 24) + int(fsort * 100.0f);
    //////////////////////////////////////////////////////////////////////////////
    int inumrenderers = GetNumRenderers();
    for (int ir = 0; ir < inumrenderers; ir++) {
      RendererModule* renderer = GetRenderer(ir);

      auto pdrw = new lev2::CallbackDrawable(pent);
      pdrw->SetRenderCallback(ModItemRenderData::enqueueToRenderQueueCallback);
      pdrw->SetQueueToLayerCallback(ModItemRenderData::QueueToLayerCallback);
      pdrw->SetOwner(&pent->GetEntData());
      pdrw->SetSortKey(isort);

      ////////////////////////////////////////////////////////
      // Callback Drawable DoubleBuffer Instance Data Destroyer
      ////////////////////////////////////////////////////////
      class Destroyer : public lev2::ICallbackDrawableDataDestroyer {
        ModItemRenderData* mpMIRD;
        virtual void Destroy() {
          if (mpMIRD)
            delete mpMIRD;
        }

      public:
        Destroyer(ModItemRenderData* pird)
            : mpMIRD(pird) {
        }
      };
      ////////////////////////////////////////////////////////
      ////////////////////////////////////////////////////////

      ModItemRenderData* mird = new ModItemRenderData(pent, &mItem, this, renderer);
      pdrw->SetDataDestroyer(new Destroyer(mird));

      ///////////////////////////////////////////////////////////
      // ModItemBufferData* srec = new ModItemBufferData;

      mird->mMIBD.mpDrawable = pdrw;

      lev2::Drawable::var_t ap;
      ap.Set(mird);
      pdrw->SetUserDataA(ap);
      ///////////////////////////////////////////////////////////

      pent->addDrawableToDefaultLayer(pdrw);
    }
    //////////////////////////////////////////////////////////////////////////////
  }
}

//////////////////////////////////////////////////////////////////////////////
}} // namespace ork::psys
///////////////////////////////////////////////////////////////////////////////
