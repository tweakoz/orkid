////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/stream/FileInputStream.h>
#include <ork/application/application.h>
#include <ork/kernel/string/string.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include "PerformanceAnalyzer.h"

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::PerformanceAnalyzerArchetype, "PerformanceAnalyzerArchetype");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::PerfAnalyzerControllerData, "PerfAnalyzerControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::PerfAnalyzerControllerInst, "PerfAnalyzerControllerInst");

namespace ork { namespace ent {

////////////////////////////////////////////////////////////////////////////////

void PerfAnalyzerControllerData::Describe()
{
	reflect::RegisterProperty("EnableDisplay", &PerfAnalyzerControllerData::mbEnable);
}
PerfAnalyzerControllerData::PerfAnalyzerControllerData()
	: mbEnable(true)
{
}
ent::ComponentInst* PerfAnalyzerControllerData::createComponent(ent::Entity* pent) const
{
	return new PerfAnalyzerControllerInst( *this, pent );
}

////////////////////////////////////////////////////////////////////////////////

void PerfAnalyzerControllerInst::Describe()
{
}
PerfAnalyzerControllerInst::PerfAnalyzerControllerInst(const PerfAnalyzerControllerData& cd, ork::ent::Entity* pent)
	: ent::ComponentInst(&cd,pent)
	, mCD(cd)
	, iupdsampleindex(0)
	, idrwsampleindex(0)
	, favgupdate(0.0f)
	, favgdraw(0.0f)
{
	for( int i=0; i<kmaxsamples; i++ )
	{
		updbeg[i] = 0.0f;
		updend[i] = 0.0f;
		drwbeg[i] = 0.0f;
		drwend[i] = 0.0f;
	}
}
void PerfAnalyzerControllerInst::DoUpdate(ent::Simulation* sinst)
{
	bool bpopped = true;

	while( bpopped )
	{
		PerfItem2 pi;
		bpopped = PerfMarkerPop( pi );
		if( bpopped )
		{
			if( 0 == strcmp(pi.mpMarkerName, "ork.sceneinst.update.begin") )
			{
				updbeg[iupdsampleindex] = pi.mfMarkerTime;
			}
			if( 0 == strcmp(pi.mpMarkerName, "ork.sceneinst.update.end") )
			{
				updend[iupdsampleindex] = pi.mfMarkerTime;
				iupdsampleindex ++;
				iupdsampleindex%=kmaxsamples;
			}
			if( 0 == strcmp(pi.mpMarkerName, "ork.viewport.draw.begin") )
			{
				drwbeg[idrwsampleindex] = pi.mfMarkerTime;
			}
			if( 0 == strcmp(pi.mpMarkerName, "ork.viewport.draw.end") )
			{
				drwend[idrwsampleindex] = pi.mfMarkerTime;
				idrwsampleindex ++;
				idrwsampleindex%=kmaxsamples;
			}
		}
	}

	favgupdate = 0.0f;
	favgdraw = 0.0f;

	int inumup = 0;
	for( int i=1; i<kmaxsamples; i++ )
	{
		float fb1 = updbeg[i-1];
		float fb2 = updbeg[i];
		if( fb1<fb2 )
		{	favgupdate += (fb2-fb1);
			inumup++;
		}
	}
	int inumds = 0;
	for( int i=1; i<kmaxsamples; i++ )
	{
		float fb1 = drwbeg[i-1];
		float fb2 = drwbeg[i];
		if( fb1<fb2 )
		{	favgdraw += (fb2-fb1);
			inumds++;
		}
	}
	favgupdate /= float(inumup);
	favgdraw /= float(inumds);

}

////////////////////////////////////////////////////////////////////////////////

void PerformanceAnalyzerArchetype::Describe()
{
	//reflect::RegisterProperty("ArchetypeAsset", &ReferenceArchetype::mArchetypeAsset);
	//reflect::AnnotatePropertyForEditor<ReferenceArchetype>("ArchetypeAsset", "editor.class", "ged.factory.assetlist");
	//reflect::AnnotatePropertyForEditor<ReferenceArchetype>("ArchetypeAsset", "editor.assettype", "refarch");
	//reflect::AnnotatePropertyForEditor<ReferenceArchetype>("ArchetypeAsset", "editor.assetclass", "ArchetypeAsset");
	//reflect::AnnotateClassForEditor<ReferenceArchetype>( "editor.instantiable", false );
	//reflect::AnnotatePropertyForEditor<ReferenceArchetype>( "Components", "editor.visible", "false" );
}

////////////////////////////////////////////////////////////////////////////////

PerformanceAnalyzerArchetype::PerformanceAnalyzerArchetype()
{
}

////////////////////////////////////////////////////////////////////////////////

void PerformanceAnalyzerArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<PerfAnalyzerControllerData>();
}

////////////////////////////////////////////////////////////////////////////////

/*void PerformanceAnalyzerArchetype::DoComposeEntity( Entity *pent ) const
{
	/////////////////////////////////////////////////
	const ent::EntData& pentdata = pent->GetEntData();
	/////////////////////////////////////////////////
}*/

////////////////////////////////////////////////////////////////////////////////

void PerformanceAnalyzerArchetype::DoStartEntity(Simulation* inst, const fmtx4 &world, Entity *pent) const
{
	const PerfAnalyzerControllerInst* ssci = pent->GetTypedComponent<PerfAnalyzerControllerInst>();
	if( ssci )
	{
		const PerfAnalyzerControllerData&	cd = ssci->GetCD();
		if( cd.mbEnable )
		{
			PerfMarkerEnable();
		}
		else
		{
			PerfMarkerDisable();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void PerformanceAnalyzerArchetype::DoLinkEntity(Simulation* inst, Entity *pent) const
{
	struct yo
	{
		const PerformanceAnalyzerArchetype* parch;
		Entity *pent;
		Simulation* psi;

		static void doit( lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const lev2::CallbackRenderable* pren )
		{
			const yo* pyo = pren->GetDrawableDataA().Get<const yo*>();

			const PerformanceAnalyzerArchetype* parch = pyo->parch;
			const Entity* pent = pyo->pent;
			const Simulation* pSI = pyo->psi;
			const PerfAnalyzerControllerInst* ssci = pent->GetTypedComponent<PerfAnalyzerControllerInst>();
			const PerfAnalyzerControllerData&	cd = ssci->GetCD();
			ork::lev2::GfxTarget* pTARG = rcid.GetRenderer()->GetTarget();
			bool IsPickState = pTARG->FBI()->IsPickState();

			if( cd.mbEnable )
			{

				float frawdeltatime = pSI->GetUpDeltaTime();

				pTARG->MTXI()->PushUIMatrix();
				pTARG->PushModColor( fcolor4::Green() );
				ork::lev2::FontMan::PushFont("d24");
				ork::lev2::FontMan::GetRef().BeginTextBlock(pTARG);
				int y=pTARG->GetH()-24;
				ork::lev2::FontMan::DrawText( pTARG, 16, y-=24, "AvgUpd<%f> UPS<%f>", ssci->favgupdate, 1.0f/ssci->favgupdate );
				ork::lev2::FontMan::DrawText( pTARG, 16, y-=24, "AvgDrw<%f> FPS<%f>", ssci->favgdraw, 1.0f/ssci->favgdraw );
				ork::lev2::FontMan::DrawText( pTARG, 16, y-=24, "RawDT<%f>", frawdeltatime );
				ork::lev2::FontMan::GetRef().EndTextBlock(pTARG);
				ork::lev2::FontMan::PopFont();
				pTARG->PopModColor( );
				pTARG->MTXI()->PopUIMatrix();
			}
		}
		static void BufferCB(ork::ent::DrawableBufItem&cdb)
		{

		}
	};

	#if 1 //DRAWTHREADS
	CallbackDrawable* pdrw = new CallbackDrawable(pent);
	pent->AddDrawable( AddPooledLiteral("Default"), pdrw );
	pdrw->SetRenderCallback( yo::doit );
	pdrw->SetQueueToLayerCallback( yo::BufferCB );
	pdrw->SetOwner(  & pent->GetEntData() );
	pdrw->SetSortKey(0x7fffffff);

	yo* pyo = new yo;
	pyo->parch = this;
	pyo->pent = pent;
	pyo->psi = inst;

	anyp ap;
	ap.Set<const yo*>( pyo );
	pdrw->SetUserDataA( ap );
#endif
}

////////////////////////////////////////////////////////////////////////////////
void PerformanceAnalyzerArchetype::DoStopEntity(Simulation* psi, Entity *pent) const
{
	PerfMarkerDisable();
}

} } // ork::ent
