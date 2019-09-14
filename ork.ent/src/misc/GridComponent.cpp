////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/renderer/renderer.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/gfx/camera.h>
///////////////////////////////////////////////////////////////////////////////
#include "GridComponent.h"
#include <ork/lev2/gfx/dbgfontman.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::GridArchetype, "GridArchetype" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::GridControllerInst, "GridControllerInst" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::GridControllerData, "GridControllerData" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void GridArchetype::Describe()
{
}

GridArchetype::GridArchetype()
{
}

///////////////////////////////////////////////////////////////////////////////

void GridArchetype::DoLinkEntity( Simulation* psi, Entity *pent ) const
{
    struct yo
    {
        yo()
        {
            mGridMaterial = new lev2::GfxMaterial3DSolid( lev2::GfxEnv::GetRef().GetLoaderTarget() );
            mGridMaterial->SetColorMode( lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR  );
            mGridMaterial->mRasterState.SetBlending(lev2::EBLENDING_OFF);
        }
        ~yo()
        {
            delete mGridMaterial;
        }
        const GridArchetype* parch;
        Entity *pent;
        lev2::GfxMaterial3DSolid*  mGridMaterial;

        static void RenderCallback( ork::lev2::RenderContextInstData& rcid,
                                    ork::lev2::GfxTarget* targ,
                                    const ork::lev2::CallbackRenderable* pren )
        {
            const yo* pyo = pren->GetUserData0().Get<const yo*>();
            auto mtl = pyo->mGridMaterial;

            const GridArchetype* parch = pyo->parch;
            const Entity* pent = pyo->pent;
            const GridControllerInst* ssci = pent->GetTypedComponent<GridControllerInst>();
            const GridControllerData& cd = ssci->GetCD();
            auto texture = cd.GetTexture();

            if(texture)
                mtl->SetColorMode( lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR  );
            else
                mtl->SetColorMode( lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR  );

            mtl->SetTexture(texture);

            mtl->_enablePick = true;

            bool IsPickState = targ->FBI()->IsPickState();
            float fphase = ssci->GetPhase();

            const auto& RCFD = targ->GetRenderContextFrameData();
            const auto& CCC = RCFD->GetCameraCalcCtx();
            const auto CAMDAT = RCFD->GetCameraData();
            const auto& FRUS = CAMDAT->GetFrustum();

            fvec2 topl(-1,-1), topr(+1,-1);
            fvec2 botl(-1,+1), botr(+1,+1);
            fray3 ray_topl, ray_topr, ray_botl, ray_botr;

            CAMDAT->projectDepthRay(topl,ray_topl);
            CAMDAT->projectDepthRay(topr,ray_topr);
            CAMDAT->projectDepthRay(botr,ray_botr);
            CAMDAT->projectDepthRay(botl,ray_botl);
            fplane3 groundplane(0,1,0,0);

            float dtl, dtr, dbl, dbr;
            bool does_topl_isect = groundplane.Intersect(ray_topl,dtl);
            bool does_topr_isect = groundplane.Intersect(ray_topr,dtr);
            bool does_botr_isect = groundplane.Intersect(ray_botr,dbr);
            bool does_botl_isect = groundplane.Intersect(ray_botl,dbl);

            if(true) //does_topl_isect&&does_topr_isect&&does_botl_isect&&does_botr_isect)
            {
                fvec3 topl(-10000.0f,0,-10000.0f); //ray_topl.mOrigin + ray_topl.mDirection*dtl;
                fvec3 topr(+10000.0f,0,-10000.0f); // = ray_topr.mOrigin + ray_topr.mDirection*dtr;
                fvec3 botr(+10000.0f,0,+10000.0f); // = ray_botr.mOrigin + ray_botr.mDirection*dbr;
                fvec3 botl(-10000.0f,0,+10000.0f); // = ray_botl.mOrigin + ray_botl.mDirection*dbl;

                //printf("topl<%g %g %g>\n", topl.x, topl.y, topl.z );
                //printf("topr<%g %g %g>\n", topr.x, topr.y, topr.z );
                //printf("botr<%g %g %g>\n", botr.x, botr.y, botr.z );
                //printf("botl<%g %g %g>\n", botl.x, botl.y, botl.z );

                fvec2 uv0(topl.x,topl.z);
                fvec2 uv1(topr.x,topr.z);
                fvec2 uv2(botr.x,botr.z);
                fvec2 uv3(botl.x,botl.z);

                auto v0 = lev2::SVtxV12C4T16( topl, uv0*0.05, 0xff000000 );
                auto v1 = lev2::SVtxV12C4T16( topr, uv1*0.05, 0xff0000ff );
                auto v2 = lev2::SVtxV12C4T16( botr, uv2*0.05, 0xffff00ff );
                auto v3 = lev2::SVtxV12C4T16( botl, uv3*0.05, 0xffff0000 );

                auto& VB = lev2::GfxEnv::GetSharedDynamicVB();
                lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
                vw.Lock( targ, &VB, 6 );

                vw.AddVertex( v0 );
                vw.AddVertex( v1 );
                vw.AddVertex( v2 );

                vw.AddVertex( v0 );
                vw.AddVertex( v2 );
                vw.AddVertex( v3 );

                vw.UnLock(targ);

                const fmtx4& PMTX = CCC.mPMatrix;
                const fmtx4& VMTX = CCC.mVMatrix;

                auto mtxi = targ->MTXI();
                auto gbi = targ->GBI();
                mtxi->PushMMatrix(fmtx4());
                mtxi->PushVMatrix(VMTX);
                mtxi->PushPMatrix(PMTX);
                targ->PushModColor( fcolor4::Green() );
                targ->PushMaterial( mtl );
                    gbi->DrawPrimitive( vw, ork::lev2::EPRIM_TRIANGLES, 6 );
                targ->PopModColor( );
                mtxi->PopPMatrix();
                mtxi->PopVMatrix();
                mtxi->PopMMatrix();

            }
            else
            {
                printf( "itl<%d> itr<%d> ibl<%d> ibr<%d>\n",
                          int(does_topl_isect),
                          int(does_topr_isect),
                          int(does_botl_isect),
                          int(does_botr_isect)
                            );
            }
        }
        static void QueueToLayerCallback(ork::ent::DrawableBufItem&cdb)
        {
            //AssertOnOpQ2( UpdateSerialOpQ() );

        }
    };

    #if 1 //DRAWTHREADS
    CallbackDrawable* pdrw = new CallbackDrawable(pent);
    pent->AddDrawable( AddPooledLiteral("Default"), pdrw );
    pdrw->SetRenderCallback( yo::RenderCallback );
    pdrw->SetQueueToLayerCallback( yo::QueueToLayerCallback );
    pdrw->SetOwner(  & pent->GetEntData() );
    pdrw->SetSortKey(0);

    yo* pyo = new yo;
    pyo->parch = this;
    pyo->pent = pent;

    Drawable::var_t ap;
    ap.Set<const yo*>( pyo );
    pdrw->SetUserDataA( ap );
#endif

}

///////////////////////////////////////////////////////////////////////////////

void GridArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
    composer.Register<GridControllerData>();
}

///////////////////////////////////////////////////////////////////////////////

void GridControllerData::Describe()
{
    reflect::RegisterProperty( "SpinRate", & GridControllerData::mfSpinRate );

    reflect::AnnotatePropertyForEditor<GridControllerData>( "SpinRate", "editor.range.min", "-6.28" );
    reflect::AnnotatePropertyForEditor<GridControllerData>( "SpinRate", "editor.range.max", "6.28" );

    reflect::RegisterProperty("Texture", &GridControllerData::mTextureAsset);

    ork::reflect::AnnotatePropertyForEditor<GridControllerData>("Texture", "editor.class", "ged.factory.assetlist");
    ork::reflect::AnnotatePropertyForEditor<GridControllerData>("Texture", "editor.assettype", "lev2tex");
    ork::reflect::AnnotatePropertyForEditor<GridControllerData>("Texture", "editor.assetclass", "lev2tex");

    ork::reflect::RegisterProperty("Scale", &GridControllerData::mfScale);

    reflect::AnnotatePropertyForEditor<GridControllerData>( "Scale", "editor.range.min", "-1000.0" );
    reflect::AnnotatePropertyForEditor<GridControllerData>( "Scale", "editor.range.max", "1000.0" );
}

///////////////////////////////////////////////////////////////////////////////

GridControllerData::GridControllerData()
    : mfSpinRate( 0.0f )
    , mTextureAsset(0)
    , mfScale(1.0f)
{
}

///////////////////////////////////////////////////////////////////////////////

lev2::Texture* GridControllerData::GetTexture() const
{
    lev2::Texture* ptx = (mTextureAsset!=0) ? mTextureAsset->GetTexture() : 0;
    return ptx;
}

///////////////////////////////////////////////////////////////////////////////

void GridControllerInst::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

GridControllerInst::GridControllerInst( const GridControllerData& data, ent::Entity* pent )
    : ork::ent::ComponentInst( & data, pent )
    , mCD( data )
    , mPhase( 0.0f )
{
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* GridControllerData::createComponent(ent::Entity* pent) const
{
    return OrkNew GridControllerInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////

void GridControllerInst::DoUpdate(ent::Simulation* sinst)
{
    mPhase += mCD.GetSpinRate()*sinst->GetDeltaTime();
}


///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}}
