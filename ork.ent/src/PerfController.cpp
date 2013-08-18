////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/PerfController.h>
#include <ork/dataflow/dataflow.h>
#if defined(_DARWIN)
#include <dispatch/dispatch.h>
#endif
//#include <orktool/qtui/qtui_tool.h>
//#include <orktool/ged/ged.h>
//#include <orktool/ged/ged_delegate.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::PerfControllerArchetype, "PerfControllerArchetype" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::PerfControllerComponentInst, "PerfControllerComponentInst" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::PerfControllerComponentData, "PerfControllerComponentData" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::PerfProgramData, "PerfProgramData" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::PerfProgramTarget, "PerfProgramTarget" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::PerfControlEvent, "PerfControlEvent" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::PerfSnapShotEvent, "PerfSnapShotEvent" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { 
namespace ent {
///////////////////////////////////////////////////////////////////////////////
extern SceneInst* GetEditorSceneInst();
extern void SceneTopoChanged();
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void PerfControlEvent::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
void PerfSnapShotEvent::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
void PerfControllerArchetype::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
PerfControllerArchetype::PerfControllerArchetype()
{
}
///////////////////////////////////////////////////////////////////////////////
void PerfControllerArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<PerfControllerComponentData>();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void PerfProgramData::AddTarget( const char* name, PerfProgramTarget* ptarget )
{
	mTargets.AddSorted( AddPooledString(name), ptarget );
}
void PerfProgramData::Clear()
{
	for( orklut<PoolString,ork::Object*>::const_iterator it=mTargets.begin(); it!=mTargets.end(); it++ )
	{
		if( it->second )
			delete it->second;
	}
	mTargets.clear();
}

///////////////////////////////////////////////////////////////////////////////
void PerfControllerComponentData::Describe()
{
	ork::ent::RegisterFamily<PerfControllerComponentData>(ork::AddPooledLiteral("control"));

	ork::reflect::RegisterProperty( "CurrentProgram", & PerfControllerComponentData::mCurrentProgram );
	ork::reflect::RegisterProperty( "AutoTargets", & PerfControllerComponentData::mAutoTargets );

	ork::reflect::RegisterProperty( "MorphA", & PerfControllerComponentData::GetMorphA, & PerfControllerComponentData::SetMorphA );
	ork::reflect::RegisterProperty( "MorphB", & PerfControllerComponentData::GetMorphB, & PerfControllerComponentData::SetMorphB );
	ork::reflect::RegisterProperty( "MorphC", & PerfControllerComponentData::GetMorphC, & PerfControllerComponentData::SetMorphC );
	ork::reflect::RegisterProperty( "MorphD", & PerfControllerComponentData::GetMorphD, & PerfControllerComponentData::SetMorphD );
	ork::reflect::RegisterProperty( "MorphGroup", & PerfControllerComponentData::mMorphGroup );

	ork::reflect::RegisterMapProperty( "Programs", & PerfControllerComponentData::mPrograms );
	ork::reflect::AnnotatePropertyForEditor< PerfControllerComponentData >("Programs", "editor.factorylistbase", "PerfProgramData" );
	ork::reflect::AnnotatePropertyForEditor< PerfControllerComponentData >("Programs", "editor.map.policy.impexp", "true" );

	ork::reflect::AnnotateClassForEditor< PerfControllerComponentData >("editor.object.ops", ConstString("AdvPrg:perfctrladv NewPrg:perfctrlnew WriPrg:perfctrlwri WriMorphA:perfctrlwrimorph ") );
}
///////////////////////////////////////////////////////////////////////////////
void PerfControllerComponentData::SetMorphA( const float& fv )
{	mfMorphA=fv;
	mMorphGroup = mkGroupAString;
	ork::dataflow::morph_event me;
	me.meType = dataflow::EMET_MORPH;
	me.mfMorphValue = fv;
	me.mMorphGroup = mMorphGroup;
	MorphEvent( & me );
}
void PerfControllerComponentData::SetMorphB( const float& fv )
{	mfMorphB=fv;
	mMorphGroup = mkGroupBString;
	ork::dataflow::morph_event me;
	me.meType = dataflow::EMET_MORPH;
	me.mfMorphValue = fv;
	me.mMorphGroup = mMorphGroup;
	MorphEvent( & me );
}
void PerfControllerComponentData::SetMorphC( const float& fv )
{	mfMorphC=fv;
	mMorphGroup = mkGroupCString;
	ork::dataflow::morph_event me;
	me.meType = dataflow::EMET_MORPH;
	me.mfMorphValue = fv;
	me.mMorphGroup = mMorphGroup;
	MorphEvent( & me );
}
void PerfControllerComponentData::SetMorphD( const float& fv )
{	mfMorphD=fv;
	mMorphGroup = mkGroupDString;
	ork::dataflow::morph_event me;
	me.meType = dataflow::EMET_MORPH;
	me.mfMorphValue = fv;
	me.mMorphGroup = mMorphGroup;
	MorphEvent( & me );
}
void PerfControllerComponentData::GetMorphA( float& outf ) const
{	outf = mfMorphA;
}
void PerfControllerComponentData::GetMorphB( float& outf ) const
{	outf = mfMorphB;
}
void PerfControllerComponentData::GetMorphC( float& outf ) const
{	outf = mfMorphC;
}
void PerfControllerComponentData::GetMorphD( float& outf ) const
{	outf = mfMorphD;
}
///////////////////////////////////////////////////////////////////////////////
void PerfControllerComponentData::MorphEvent(const ork::dataflow::morph_event* me)
{
	orkvector<std::string> AutoTargets;
	ork::SplitString( mAutoTargets.c_str(), AutoTargets, " " );
	int inumtargets = int(AutoTargets.size());
	for( int it=0; it<inumtargets; it++ )
	{
		const std::string& EntityName = AutoTargets[it];
		PoolString psobjname = AddPooledString( EntityName.c_str() );
#if defined(ORK_OSXX)
		ent::SceneInst* psi = GetEditorSceneInst();
		printf( "SCENEINST<%p> EntityName<%s>\n", psi, EntityName.c_str() );
		if( psi )
		{
			ent::Entity* pent = psi->FindEntity( psobjname );
			if( pent )
			{	//psse.PushNode( EntityName.c_str() );
				((ork::Object*)pent)->Notify( me );
				//psse.PopNode();
			}
		}
#endif
	}
	//itPRG->second = program;
}
///////////////////////////////////////////////////////////////////////////////
void PerfControllerComponentData::AdvanceProgram()
{
	const char* pstr = mCurrentProgram.c_str();
	bool bhaveprog = (pstr!=0) && (strlen(pstr)>0);

	orklut<PoolString,ork::Object*>::iterator it = (false==bhaveprog) ? mPrograms.end() : mPrograms.find(mCurrentProgram);

	it++;

	if( it==mPrograms.end() )
	{
		it=mPrograms.begin();
	}
	if( it!=mPrograms.end() )
	{
		mCurrentProgram = it->first;
	}
	
}
///////////////////////////////////////////////////////////////////////////////
void PerfControllerComponentData::WriteProgram()
{
	////////////////////////////////////////////////////////

	if( 0 == mAutoTargets.c_str() )
		return;
		
	orklut<PoolString,ork::Object*>::iterator itPRG=mPrograms.find(mCurrentProgram);
	if( itPRG==mPrograms.end() )
		return;
	if( 0 == itPRG->second )
		return;
		
	PerfProgramData* program = new PerfProgramData; 
	//rtti::autocast(it->second);
	//program->Clear();
	
	////////////////////////////////////////////////////////

	orkvector<std::string> AutoTargets;

	ork::SplitString( mAutoTargets.c_str(), AutoTargets, " " );

	int inumtargets = int(AutoTargets.size());
	
#if defined(ORK_OSXX)
	for( int it=0; it<inumtargets; it++ )
	{
		PerfSnapShotEvent psse;
		psse.SetProgram( program );
		
		const std::string& EntityName = AutoTargets[it];

		PoolString psobjname = AddPooledString( EntityName.c_str() );
		
		ent::SceneInst* psi = GetEditorSceneInst();
		
		printf( "SCENEINST<%p> EntityName<%s>\n", psi, EntityName.c_str() );
		
		if( psi )
		{
			ent::Entity* pent = psi->FindEntity( psobjname );
			
			if( pent )
			{
				psse.PushNode( EntityName.c_str() );
				((ork::Object*)pent)->Notify( & psse );
				psse.PopNode();
			}
		}
	}
	itPRG->second = program;

	SceneTopoChanged();
#endif
	////////////////////////////////////////////////////////
	
}
///////////////////////////////////////////////////////////////////////////////
void PerfControllerComponentData::AddProgram( PoolString name, PerfProgramData* program )
{
	////////////////////////////////////////////////////////

	if( 0 == mAutoTargets.c_str() )
		return;
		
	////////////////////////////////////////////////////////

	orkvector<std::string> AutoTargets;
	mPrograms.AddSorted( name, program );

	ork::SplitString( mAutoTargets.c_str(), AutoTargets, " " );

	int inumtargets = int(AutoTargets.size());
	
#if defined(ORK_OSXX)
	for( int it=0; it<inumtargets; it++ )
	{
		PerfSnapShotEvent psse;
		psse.SetProgram( program );
		
		const std::string& EntityName = AutoTargets[it];

		PoolString psobjname = AddPooledString( EntityName.c_str() );
		
		ent::SceneInst* psi = GetEditorSceneInst();
		
		printf( "SCENEINST<%p> EntityName<%s>\n", psi, EntityName.c_str() );
		
		if( psi )
		{
			ent::Entity* pent = psi->FindEntity( psobjname );
			
			if( pent )
			{
				psse.PushNode( EntityName.c_str() );
				((ork::Object*)pent)->Notify( & psse );
				psse.PopNode();
			}
		}
	}
	SceneTopoChanged();
#endif
	////////////////////////////////////////////////////////
	
}
///////////////////////////////////////////////////////////////////////////////
PerfControllerComponentData::PerfControllerComponentData()
	: mfMorphA(0.0f)
	, mfMorphB(0.0f)
	, mfMorphC(0.0f)
	, mfMorphD(0.0f)
	, mkGroupAString( AddPooledString( "a" ) )
	, mkGroupBString( AddPooledString( "b" ) )
	, mkGroupCString( AddPooledString( "c" ) )
	, mkGroupDString( AddPooledString( "d" ) )
{
	mMorphGroup = mkGroupAString;
	mCurrentProgram = AddPooledString("none");
}
///////////////////////////////////////////////////////////////////////////////
PerfControllerComponentData::~PerfControllerComponentData()
{
}
///////////////////////////////////////////////////////////////////////////////
ent::ComponentInst* PerfControllerComponentData::CreateComponent(ent::Entity* pent) const
{
	return new PerfControllerComponentInst( *this, pent );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void PerfControllerComponentInst::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
PerfControllerComponentInst::PerfControllerComponentInst( const PerfControllerComponentData& data, ent::Entity* pent )
	: ork::ent::ComponentInst( & data, pent )
	, mPCCD( data )
{
}
///////////////////////////////////////////////////////////////////////////////
PerfControllerComponentInst::~PerfControllerComponentInst()
{
}
///////////////////////////////////////////////////////////////////////////////
void PerfControllerComponentInst::ChangeProgram( ent::SceneInst* psi, const PoolString& progname )
{
	// apply all data change entries to thier cooresponding entity components
	
	const orklut<PoolString,ork::Object*>& progs = mPCCD.GetPrograms();
	orklut<PoolString,ork::Object*>::const_iterator it=progs.find(mPCCD.GetCurrentProgram());
	if( it!=progs.end() )
	{
		if( it->second )
		{	
			PerfProgramData* ppd = rtti::autocast(it->second);

			if( ppd )
			{
				const orklut<PoolString,ork::Object*>& tgts = ppd->GetTargets();
				
				mCurrentProgram = mPCCD.GetCurrentProgram();
				
				for( orklut<PoolString,ork::Object*>::const_iterator it_tgt=tgts.begin(); it_tgt!=tgts.end(); it_tgt++ )
				{
					if( it_tgt->second )
					{
						PerfProgramTarget* ptgt = rtti::autocast(it_tgt->second);
						PerfControlEvent pce;
						std::string tgtname = ptgt->GetTargetName().c_str();
						std::string tgtval = ptgt->GetValue().c_str();
													
						pce.mTarget.set( tgtname.c_str() );
						pce.mValue.set( tgtval.c_str() );

						std::string EntityName = pce.PopTargetNode();
						std::string KeyName = pce.mTarget.c_str();

						PoolString psobjname = AddPooledString( EntityName.c_str() );
						ent::Entity* pent = psi->FindEntity( psobjname );
						
						printf( "ApplyProgramTarget<%s> Entity<%s> Key<%s> Val<%s>\n", tgtname.c_str(), EntityName.c_str(), KeyName.c_str(), tgtval.c_str() );

						if( pent )
						{
							((ork::Object*)pent)->Notify( & pce );
						}
					}
				}

			}
		}	
	}
	mCurrentProgram = progname;
}
///////////////////////////////////////////////////////////////////////////////
void PerfControllerComponentInst::DoUpdate(ent::SceneInst* psi)
{
	if( mCurrentProgram != mPCCD.GetCurrentProgram() )
	{
		ChangeProgram( psi, mPCCD.GetCurrentProgram() );
	}
	
}
///////////////////////////////////////////////////////////////////////////////
bool PerfControllerComponentInst::DoLink( ork::ent::SceneInst* psi )
{
	ChangeProgram( psi, mPCCD.GetCurrentProgram() );
	return true;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
std::string PerfControlEvent::PopTargetNode() // remove leading target path element
{
	std::string rval;
	FixedString<256> str_left;
	str_left = mTarget;
	char* pfirstdot = (char*) strstr( str_left.c_str(), "." );
	if( pfirstdot )
	{	pfirstdot[0] = 0;
		int ilen = int(pfirstdot-str_left.c_str());
		rval = str_left.c_str();
		mTarget.set(pfirstdot+1);
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
PerfControlEvent::PerfControlEvent()
{
}
PoolString PerfControlEvent::ValueAsPoolString() const
{
	return AddPooledString(mValue.c_str());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
PerfSnapShotEvent::PerfSnapShotEvent()
	: mSnapShotProgram(0)
{
}
///////////////////////////////////////////////////////////////////////////////
PerfSnapShotEvent::str_type PerfSnapShotEvent::GenNodeName() const
{
	str_type rval;
	
	for( size_t in=0; in<mNodeStack.size(); in++ )
	{
		const str_type& node = mNodeStack[in];
		rval += node;
		if( in<(mNodeStack.size()-1) )
			rval += ".";
	}
	
	return rval;
}
void PerfSnapShotEvent::AddTarget( const char* tgtval ) const
{
	str_type NodeName = GenNodeName();
	PerfProgramTarget* target = new PerfProgramTarget( NodeName.c_str(), tgtval );
	GetProgram()->AddTarget( NodeName.c_str(), target );
}
///////////////////////////////////////////////////////////////////////////////
void PerfSnapShotEvent::PushNode( str_type str ) const
{
	mNodeStack.push_back(str);
}
///////////////////////////////////////////////////////////////////////////////
void PerfSnapShotEvent::PopNode() const
{
	mNodeStack.pop_back(); 
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void PerfProgramData::Describe()
{
	ork::reflect::RegisterMapProperty( "Targets", & PerfProgramData::mTargets );
	ork::reflect::AnnotatePropertyForEditor< PerfProgramData >("Targets", "editor.factorylistbase", "PerfProgramTarget" );
	ork::reflect::AnnotatePropertyForEditor< PerfProgramData >("Targets", "editor.map.policy.impexp", "true" );
}
///////////////////////////////////////////////////////////////////////////////
PerfProgramData::PerfProgramData()
{
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void PerfProgramTarget::Describe()
{
	ork::reflect::RegisterProperty( "Name", & PerfProgramTarget::mTargetName );
	ork::reflect::RegisterProperty( "Value", & PerfProgramTarget::mValue );
}
///////////////////////////////////////////////////////////////////////////////
PerfProgramTarget::PerfProgramTarget()
{
}
///////////////////////////////////////////////////////////////////////////////
PerfProgramTarget::PerfProgramTarget(const char* tname, const char* tval)
	: mTargetName( AddPooledString( tname ) )
	, mValue( AddPooledString( tval ) )
{
}
}}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
namespace ork { 
namespace tool {
#if defined(ORK_OSXX)
dispatch_queue_t EditSafeQueue();
///////////////////////////////////////////////////////////////////////////////
class perfctrlnew : public tool::ged::IOpsDelegate
{
	RttiDeclareConcrete( perfctrlnew, tool::ged::IOpsDelegate );
	virtual void Execute( ork::Object* ptarget )
	{
		SetProgress(0.0f);
		SetProgress(1.0f);
		tool::ged::IOpsDelegate::RemoveTask( perfctrlnew::GetClassStatic(), ptarget );
		//dispatch_async( EditSafeQueue(),
		//^{
			/////////////////////////////////////////
			ork::ent::PerfControllerComponentData* pccd = rtti::autocast(ptarget);
			const orklut<PoolString,ork::Object*>& prgs = pccd->GetPrograms();
			static int gautocount = 0;
			ork::ent::PerfProgramData* pnewprog = new ork::ent::PerfProgramData;
			FixedString<256> NameStr;
			bool bnameok = false;
			while(false==bnameok)
			{
				NameStr.format( "AutoProg%03d", (void*) gautocount );
				PoolString nsps = AddPooledString( NameStr.c_str() );
				if( prgs.find(nsps)!=prgs.end() )
				{	gautocount++;
					continue;
				}
				bnameok=true;
			}
			pccd->AddProgram( AddPooledString( NameStr.c_str() ), pnewprog );
			/////////////////////////////////////////
			gautocount++;
		//});
	}
};
///////////////////////////////////////////////////////////////////////////////
void perfctrlnew::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class perfctrladv : public tool::ged::IOpsDelegate
{
	RttiDeclareConcrete( perfctrladv, tool::ged::IOpsDelegate );
	virtual void Execute( ork::Object* ptarget )
	{
		SetProgress(0.0f);
		/////////////////////////////////////////
		ork::ent::PerfControllerComponentData* pccd = rtti::autocast(ptarget);
		pccd->AdvanceProgram();
		/////////////////////////////////////////
		SetProgress(1.0f);
		tool::ged::IOpsDelegate::RemoveTask( perfctrladv::GetClassStatic(), ptarget );
	}
};
///////////////////////////////////////////////////////////////////////////////
void perfctrladv::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class perfctrlwri : public tool::ged::IOpsDelegate
{
	RttiDeclareConcrete( perfctrlwri, tool::ged::IOpsDelegate );
	virtual void Execute( ork::Object* ptarget )
	{
		SetProgress(0.0f);
		SetProgress(1.0f);
		tool::ged::IOpsDelegate::RemoveTask( perfctrlwri::GetClassStatic(), ptarget );
		//dispatch_async( EditSafeQueue(),
		//^{
			ork::ent::PerfControllerComponentData* pccd = rtti::autocast(ptarget);
			pccd->WriteProgram();
		//});
	}
};
void perfctrlwri::Describe(){}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class perfctrlwrimorph : public tool::ged::IOpsDelegate
{
	RttiDeclareConcrete( perfctrlwrimorph, tool::ged::IOpsDelegate );
	virtual void Execute( ork::Object* ptarget )
	{
		SetProgress(0.0f);
		SetProgress(1.0f);
		tool::ged::IOpsDelegate::RemoveTask( perfctrlwrimorph::GetClassStatic(), ptarget );
		//dispatch_async( EditSafeQueue(),
		//^{
			ork::ent::PerfControllerComponentData* pccd = rtti::autocast(ptarget);
			dataflow::morph_event me;
			me.meType = ork::dataflow::EMET_WRITE;
			me.mfMorphValue = 0.0f; // TODO get value
			me.mMorphGroup = AddPooledString("a");
			pccd->MorphEvent(&me);
			//pccd->WriteProgram();
		//});
	}
};
void perfctrlwrimorph::Describe(){}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#endif

}}

#if defined(ORK_OSXX)
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::perfctrlnew,"perfctrlnew");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::perfctrladv,"perfctrladv");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::perfctrlwri,"perfctrlwri");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::perfctrlwrimorph,"perfctrlwrimorph");
#endif

