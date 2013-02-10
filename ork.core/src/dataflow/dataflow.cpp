////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 



#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
template class ork::orklut<ork::PoolString,ork::dataflow::module*>;

typedef ork::dataflow::outplug<float>			OrkDataflowOutPlugFloat;
typedef ork::dataflow::inplug<float>			OrkDataflowInpPlugFloat;
typedef ork::dataflow::outplug<ork::CVector3>	OrkDataflowOutPlugFloat3;
typedef ork::dataflow::inplug<ork::CVector3>	OrkDataflowInpPlugFloat3;
typedef ork::dataflow::floatinplugxf<ork::dataflow::floatxf> OrkDataflowFloatInpPlugXf; 
typedef ork::dataflow::vect3inplugxf<ork::dataflow::vect3xf> OrkDataflowFloat3InpPlugXf; 
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::graph,"dflow/graph");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::module,"dflow/module");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::dgmodule,"dflow/dgmodule");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::plugroot,"dflow/plugroot");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::inplugbase,"dflow/inplugbase");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::outplugbase,"dflow/outplugbase");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::floatinplug,"dflow/floatinplug");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::vect3inplug,"dflow/vect3inplug");

INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::modscabias,"dflow/ModScaleBias");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::floatxfitembase,"dflow/floatxfitembase");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::floatxfmsbcurve,"dflow/floatxfmsbcurve");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::floatxfmodstep,"dflow/floatxfmodstep");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::floatxf,"dflow/floatxf");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::vect3xf,"dflow/vect3xf");

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowOutPlugFloat,"dflow/outplug<float>");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowInpPlugFloat,"dflow/inplug<float>");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowOutPlugFloat3,"dflow/outplug<vect3>");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowInpPlugFloat3,"dflow/inplug<vect3>");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowFloatInpPlugXf,"dflow/inplugxf<float,floatxf>");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowFloat3InpPlugXf,"dflow/inplugxf<vect3,vect3xf>");

INSTANTIATE_TRANSPARENT_RTTI( ork::dataflow::morph_event, "dflow/morph_event" );

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace dataflow {
///////////////////////////////////////////////////////////////////////////////

void morph_event::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

bool gbGRAPHLIVE = false;
void plugroot::Describe(){}
void inplugbase::Describe(){}
void outplugbase::Describe() {}
///////////////////////////////////////////////////////////////////////////////
template<> int MaxFanout<float>() { return 0; }
template<> void inplug<float>::Describe(){}
template<> void outplug<float>::Describe(){}
template class outplug<float>;
///////////////////////////////////////////////////////////////////////////////
template<>
const float& outplug<float>::GetInternalData() const
{	static const float kdefault = 0.0f;
	if( 0 == mOutputData ) return kdefault;
	return *mOutputData;
}
///////////////////////////////////////////////////////////////////////////////
template<> const float& outplug<float>::GetValue() const
{
	return GetInternalData();
}

///////////////////////////////////////////////////////////////////////////////
template<> int MaxFanout<CVector3>() { return 0; }
template<> void inplug<CVector3>::Describe(){}
template<> void outplug<CVector3>::Describe(){}
template class outplug<CVector3>;
///////////////////////////////////////////////////////////////////////////////
template<> const CVector3& outplug<CVector3>::GetInternalData() const
{	static const CVector3 kdefault;
	if( 0 == mOutputData )
	{
		return kdefault;
	}
	return *mOutputData;
}
///////////////////////////////////////////////////////////////////////////////
template<> const CVector3& outplug<CVector3>::GetValue() const
{
	return GetInternalData();
}
///////////////////////////////////////////////////////////////////////////////
void floatinplug::Describe()
{
	ork::reflect::RegisterProperty( "value", & floatinplug::GetValAccessor, & floatinplug::SetValAccessor );
	ork::reflect::AnnotatePropertyForEditor< floatinplug >( "value", "editor.visible", "false" );
}
///////////////////////////////////////////////////////////////////////////////
template <> void floatinplugxf< floatxf >::Describe()
{
	ork::reflect::RegisterProperty( "xf", & floatinplugxf< floatxf >::XfAccessor );
}
///////////////////////////////////////////////////////////////////////////////
void vect3inplug::Describe()
{
	ork::reflect::RegisterProperty( "value", & vect3inplug::GetValAccessor, & vect3inplug::SetValAccessor );
	ork::reflect::AnnotatePropertyForEditor< vect3inplug >( "value", "editor.visible", "false" );
}
///////////////////////////////////////////////////////////////////////////////
template <> void vect3inplugxf< vect3xf >::Describe()
{
	ork::reflect::RegisterProperty( "xf", & vect3inplugxf< vect3xf >::XfAccessor );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void modscabias::Describe()
{
	ork::reflect::RegisterProperty( "mod", & modscabias::mfMod );
	ork::reflect::RegisterProperty( "scale", & modscabias::mfScale );
	ork::reflect::RegisterProperty( "bias", & modscabias::mfBias );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "mod", "editor.range.min", "0.0f" );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "mod", "editor.range.max", "16.0f" );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "scale", "editor.range.min", "-16.0f" );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "scale", "editor.range.max", "16.0f" );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "bias", "editor.range.min", "-16.0f" );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "bias", "editor.range.max", "16.0f" );
}
void floatxfitembase::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
void floatxfmsbcurve::Describe()
{
	ork::reflect::RegisterProperty( "curve", & floatxfmsbcurve::CurveAccessor );
	ork::reflect::RegisterProperty( "ModScaleBias", & floatxfmsbcurve::ModScaleBiasAccessor );

	ork::reflect::RegisterProperty( "domodscabia", & floatxfmsbcurve::mbDoModScaBia );
	ork::reflect::RegisterProperty( "docurve", & floatxfmsbcurve::mbDoCurve );

	static const char* EdGrpStr =
      "sort:// domodscabia docurve ModScaleBias curve";

	any16 a16;
	a16.Set<const char*>(EdGrpStr);
	reflect::AnnotateClassForEditor<floatxfmsbcurve>( "editor.prop.groups", a16 );

}
///////////////////////////////////////////////////////////////////////////////
float floatxfmsbcurve::transform( float input ) const
{	
	if( mbDoModScaBia || mbDoCurve )
	{
		float fsca = (GetScale()*input)+GetBias();
		float modout = (GetMod()>0.0f) ? std::fmod(fsca,GetMod()) : fsca;
		float biasout = modout;
		input = biasout;
	}
	if( mbDoCurve )
	{
		float clampout = (input<0.0f) ? 0.0f : (input>1.0f) ? 1.0f : input;
		input = mMultiCurve1d.Sample(clampout);
	}
	return input;
}
///////////////////////////////////////////////////////////////////////////////
void floatxfmodstep::Describe()
{
	ork::reflect::RegisterProperty( "Mod", & floatxfmodstep::mMod );
	ork::reflect::RegisterProperty( "Step", & floatxfmodstep::miSteps );
	ork::reflect::RegisterProperty( "OutputBias", & floatxfmodstep::mOutputBias );
	ork::reflect::RegisterProperty( "OutputScale", & floatxfmodstep::mOutputScale );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "Mod", "editor.range.min", "0.01f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "Mod", "editor.range.max", "16.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "Step", "editor.range.min", "1.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "Step", "editor.range.max", "128.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "OutputBias", "editor.range.min", "-16.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "OutputBias", "editor.range.max", "16.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "OutputScale", "editor.range.min", "-1600.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "OutputScale", "editor.range.max", "1600.0f" );

}
///////////////////////////////////////////////////////////////////////////////
float floatxfmodstep::transform( float input ) const
{	
	int isteps = miSteps > 0 ? miSteps : 1;
	
	float fclamped = (input<0.0f) ? 0.0f : (input>1.0f) ? 1.0f : input;
	input = (mMod>0.0f) ? (std::fmod(input,mMod)/mMod) : fclamped;
	float finpsc = input*float(isteps); // 0..1 -> 0..4
	int   iinpsc = int(std::floor(finpsc)); 
	float fout = float(iinpsc)/float(isteps);
	return (fout*mOutputScale)+mOutputBias;
}
///////////////////////////////////////////////////////////////////////////////
floatxf::floatxf()
	: miTest(0)
{
}
///////////////////////////////////////////////////////////////////////////////
floatxf::~floatxf()
{
}
///////////////////////////////////////////////////////////////////////////////
void floatxf::Describe()
{
	//ork::reflect::RegisterProperty( "Test", & floatxf::miTest );

	ork::reflect::RegisterMapProperty( "Transforms", & floatxf::mTransforms );
	ork::reflect::AnnotatePropertyForEditor< floatxf >("Transforms", "editor.factorylistbase", "dflow/floatxfitembase" );
	ork::reflect::AnnotatePropertyForEditor< floatxf >("Transforms", "editor.map.policy.impexp", "true" );

	/*static const char* EdGrpStr =
      "sort:// Test";

	any16 a16;
	a16.Set<const char*>(EdGrpStr);
	reflect::AnnotateClassForEditor<floatxf>( "editor.prop.groups", a16 );
*/
}
///////////////////////////////////////////////////////////////////////////////
float floatxf::transform( float input ) const
{
	if( ! mTransforms.Empty() )
	{
		for( auto it=mTransforms.begin(); it!=mTransforms.end(); it++ )
		{
			floatxfitembase* pitem = rtti::autocast(it->second);
			if( pitem )
			{
				input = pitem->transform(input);
			}
		}

	}
	return input;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void vect3xf::Describe()
{

}
///////////////////////////////////////////////////////////////////////////////
CVector3 vect3xf::transform( const CVector3& input ) const
{	
	CVector3 output;
	output.SetX( mTransformX.transform( input.GetX() ) );
	output.SetY( mTransformX.transform( input.GetY() ) );
	output.SetZ( mTransformX.transform( input.GetZ() ) );
	return output;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/*bool inplugbase::IsDirty() const
{
	bool rv = false;
	const outplugbase* pcon = GetConnected();
	if( pcon )
	{
		rv |= pcon->IsDirty();
	}
	return rv;
}*/

plugroot::plugroot( module*pmod, EPlugDir edir, EPlugRate epr, const std::type_info& tid, const char* pname ) 
    : mePlugDir(edir)
    , mModule(pmod)
    , mTypeId(tid)
    , mePlugRate( epr )
    , mPlugName( pname ? ork::AddPooledLiteral( pname ) : ork::AddPooledLiteral( "noname" ) )
{}
void plugroot::SetDirty( bool bv )
{
	mbDirty=bv;
	DoSetDirty(bv);
	//mModule
}
///////////////////////////////////////////////////////////////////////////////
void morphable::HandleMorphEvent(const morph_event* me)
{
	switch( me->meType )
	{
		case EMET_WRITE:
			break;
		case EMET_MORPH:
			Morph1D( me );
			break;
			
	}

}
///////////////////////////////////////////////////////////////////////////////
inplugbase::inplugbase(	module* pmod, EPlugRate epr,const std::type_info& tid, const char* pname ) 
	: plugroot(pmod,EPD_INPUT,epr,tid, pname)
	, mExternalOutput(0)
	, mpMorphable(0)
{
}
///////////////////////////////////////////////////////////////////////////////
inplugbase::~inplugbase()
{
	Disconnect();
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::DoSetDirty( bool bd )
{	if( bd )
	{	GetModule()->SetInputDirty( this );
		for( orkvector<outplugbase*>::const_iterator	it=mInternalOutputConnections.begin();
														it!=mInternalOutputConnections.end();
														it++ )
		{
			(*it)->SetDirty(bd);
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::ConnectInternal( outplugbase* vt )
{	mInternalOutputConnections.push_back( vt );
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::ConnectExternal( outplugbase* vt )
{	mExternalOutput = vt;
	vt->mExternalInputConnections.push_back(this);
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::SafeConnect( graph& gr, outplugbase* vt )
{	//OrkAssert( GetDataTypeId() == vt->GetDataTypeId() );
	bool cc = gr.CanConnect( this, vt );
	OrkAssert( cc );
	ConnectExternal( vt );
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::Disconnect()
{	if( mExternalOutput )
	{	mExternalOutput->Disconnect(this);
	}
	mExternalOutput = 0;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
outplugbase::outplugbase( module*pmod, EPlugRate epr, const std::type_info& tid, const char* pname )
	: plugroot(pmod,EPD_OUTPUT,epr,tid,pname)
	, mpRegister(0)
{
}
///////////////////////////////////////////////////////////////////////////////
outplugbase::~outplugbase()
{	while( GetNumExternalOutputConnections() )
	{	inplugbase* pcon = GetExternalOutputConnection(GetNumExternalOutputConnections()-1);
		pcon->Disconnect();
	}
}
///////////////////////////////////////////////////////////////////////////////
void outplugbase::DoSetDirty( bool bd )
{	if( bd )
	{	GetModule()->SetOutputDirty( this );
		for( orkvector<inplugbase*>::const_iterator it=mExternalInputConnections.begin();
													it!=mExternalInputConnections.end();
													it++ )
		{
			(*it)->SetDirty(bd);
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
void outplugbase::Disconnect( inplugbase* pinplug )
{	orkvector<inplugbase*>::iterator it= std::find(mExternalInputConnections.begin(),mExternalInputConnections.end(), pinplug );
	if( it!=mExternalInputConnections.end() )
	{
		mExternalInputConnections.erase(it);
	}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void module::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
void module::SetInputDirty( inplugbase* plg )
{	DoSetInputDirty( plg );
}
///////////////////////////////////////////////////////////////////////////////
void module::SetOutputDirty( outplugbase* plg )
{	DoSetOutputDirty( plg );
}
///////////////////////////////////////////////////////////////////////////////
void module::AddDependency( outplugbase& pout, inplugbase& pin )
{	pin.ConnectInternal( & pout );
	//DepPlugSet::value_type v( & pin, & pout );
	//mDependencies.insert( v );
}
///////////////////////////////////////////////////////////////////////////////
bool module::IsDirty()
{	bool rval = false;
	int inumout = this->GetNumOutputs();
	for( int i=0; i<inumout; i++ )
	{	outplugbase* poutput = GetOutput(i);
		rval |= poutput->IsDirty();
	}
	if( false == rval )
	{	int inumchi=GetNumChildren();
		for( int ic=0; ic<inumchi; ic++ )
		{	module* pchild = GetChild(ic);
			rval |= pchild->IsDirty();
		}
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
inplugbase* module::GetInputNamed( const PoolString& named ) 
{	int inuminp = GetNumInputs();
	for( int ip=0; ip<inuminp; ip++ )
	{	inplugbase* rval = GetInput(ip);
		if( named == rval->GetName() )
		{
			return rval;
		}
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
outplugbase* module::GetOutputNamed( const PoolString& named )
{	int inumout = GetNumOutputs();
	for( int ip=0; ip<inumout; ip++ )
	{	outplugbase* rval = GetOutput(ip);
		if( named == rval->GetName() )
		{
			return rval;
		}
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
module* module::GetChildNamed( const ork::PoolString& named ) const
{	int inumchi = GetNumChildren();
	for( int ic=0; ic<inumchi; ic++ )
	{	module* rval = GetChild(ic);
		if( named == rval->GetName() )
		{
			return rval;
		}
	}
	return 0;
}
/*bool module::IsOutputDirty(const ork::dataflow::outplugbase *pplug) const
{
	bool bv = false;
	for( DepPlugSet::const_iterator it=mDependencies.begin(); it!=mDependencies.end(); it++ )
	{
		const DepPlugSet::value_type& v = *it;

		inplugbase* pin = v.first;
		const outplugbase* pout = v.second;

		if( pout == pplug )
		{
			bv |= pin->IsDirty();
		}
	}
	return bv;
}*/
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void dgmodule::Describe()
{
	ork::reflect::RegisterProperty( "mgvpos", & dgmodule::mgvpos );
	ork::reflect::AnnotatePropertyForEditor< dgmodule >("mgvpos", "editor.visible", "false" );
}
///////////////////////////////////////////////////////////////////////////////
dgmodule::dgmodule()
	: mAffinity( dataflow::scheduler::CpuAffinity )
	, mParent(0)
	, mKey()
{
}
///////////////////////////////////////////////////////////////////////////////
void dgmodule::DivideWork( const scheduler& sch,  cluster* clus ) 
{	clus->AddModule( this );
	DoDivideWork( sch, clus );
}
///////////////////////////////////////////////////////////////////////////////
void dgmodule::DoDivideWork( const scheduler& sch,  cluster* clus ) 
{	workunit* wu = new workunit(this,clus,0);
	wu->SetAffinity( GetAffinity() );
	clus->AddWorkUnit(wu);
}
///////////////////////////////////////////////////////////////////////////////
void dgmodule::ReleaseWorkUnit( workunit* wu )
{	OrkAssert( wu->GetModule() == this );
	delete wu;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void graph::Describe()
{	ork::reflect::RegisterMapProperty("Modules", &graph::mModules);
	ork::reflect::AnnotatePropertyForEditor< graph >("Modules", "editor.factorylistbase", "dflow/dgmodule" );
	ork::reflect::RegisterProperty( "zzz_connections", & graph::SerializeConnections, & graph::DeserializeConnections );
	ork::reflect::AnnotatePropertyForEditor< graph >("zzz_connections", "editor.visible", "false" );

	ork::reflect::AnnotateClassForEditor< graph >("editor.object.ops", ConstString("dfgraph:dflowgraphedit import:dflowgraphimport export:dflowgraphexport") );
}
///////////////////////////////////////////////////////////////////////////////
graph::graph()
	: mbTopologyIsDirty( true )
	, mScheduler( 0 )
	, mbAccumulateWork( false )
	, mbInProgress(false)
	, mMutex( "GraphMutex" )
	, mExternal(0)
{
	mChildrenTopoSorted.LockForWrite().SetKeyPolicy(ork::EKEYPOLICY_MULTILUT);
	mChildrenTopoSorted.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
graph::~graph()
{	SetScheduler( 0 );

	for( orklut<ork::PoolString,ork::Object*>::const_iterator it=mModules.begin(); it!=mModules.end(); it++ )
	{
		ork::Object* pobj = it->second;
		delete pobj;
	}

}
///////////////////////////////////////////////////////////////////////////////
graph::graph( const graph& oth )
	: mbTopologyIsDirty( true )
	, mScheduler( 0 )
	, mbAccumulateWork( false )
	, mbInProgress(false)
	, mMutex( "GraphMutex" )
	, mExternal(0)
{
}
///////////////////////////////////////////////////////////////////////////////
bool graph::DoNotify(const ork::event::Event *event)
{	if( const ItemRemovalEvent* pev = rtti::autocast(event) )
	{	if( pev->mProperty == graph::GetClassStatic()->Description().FindProperty("Modules") )
		{	ork::PoolString ps = pev->mKey.Get<ork::PoolString>();
			ork::Object* pobj = pev->mOldValue.Get<ork::Object*>();
			delete pobj;
			return true;
		}
	}
	else if( const MapItemCreationEvent* pev = rtti::autocast(event) )
	{	if( pev->mProperty == graph::GetClassStatic()->Description().FindProperty("Modules") )
		{
			PoolString psname = pev->mKey.Get<PoolString>();
			ork::Object* pnewobj = pev->mNewItem.Get<ork::Object*>();
			dgmodule* pdgmod = rtti::autocast(pnewobj);
			
			pdgmod->SetParent(this);
			pdgmod->SetName( psname );
		}
	}
	return true;
}
///////////////////////////////////////////////////////////////////////////////
void graph::Clear()
{	mSourcePlugs.clear();
	mSinkPlugs.clear();
	while( false==mModuleQueue.empty() ) mModuleQueue.pop();
	mbTopologyIsDirty = true;
	mScheduler = 0;
	mbAccumulateWork = false;
	mbInProgress = false;
}
///////////////////////////////////////////////////////////////////////////////
void graph::ReInit()
{
	for( orklut<ork::PoolString,ork::Object*>::const_iterator it=mModules.begin(); it!=mModules.end(); it++ )
	{
		ork::Object* pobj = it->second;
		delete pobj;
	}
	Clear();
}
///////////////////////////////////////////////////////////////////////////////
bool graph::CanConnect( const inplugbase* pin, const outplugbase* pout ) const
{	return ((&pin->GetDataTypeId()) == (&pout->GetDataTypeId()));
}
///////////////////////////////////////////////////////////////////////////////
const orklut<int,dgmodule*>& graph::LockTopoSortedChildrenForRead(int lid) const
{	const orklut<int,dgmodule*>& topos = mChildrenTopoSorted.LockForRead(lid);
	OrkAssert( IsTopologyDirty() == false );
	return topos;
}
///////////////////////////////////////////////////////////////////////////////
orklut<int, dgmodule*>& graph::LockTopoSortedChildrenForWrite(int lid)
{	orklut<int,dgmodule*>& topos = mChildrenTopoSorted.LockForWrite(lid);
	return topos;
}
///////////////////////////////////////////////////////////////////////////////
void graph::UnLockTopoSortedChildren() const
{	mChildrenTopoSorted.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
void graph::SetScheduler( scheduler* psch )
{	if( psch )
	{	psch->AddGraph( this );
	}
	else if( mScheduler )
	{	mScheduler->RemoveGraph( this );
	}
	mScheduler = psch;
}
///////////////////////////////////////////////////////////////////////////////
void graph::UnBindExternal()
{
	mMutex.Lock();
	{
		mExternal = 0;
		mbTopologyIsDirty = true;
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
void graph::BindExternal( dyn_external* pext )
{
	mMutex.Lock();
	{
		mExternal = pext;
		mbTopologyIsDirty = true;
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
void graph::AddChild( const PoolString& named, dgmodule* pchild )
{	pchild->SetName( named );
	mMutex.Lock();
	{
		mModules.AddSorted( named, pchild );
		pchild->SetParent(this);
		mbTopologyIsDirty = true;
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
void graph::AddChild( const char* named, dgmodule* pchild )
{
	AddChild( AddPooledString(named), pchild );
}
///////////////////////////////////////////////////////////////////////////////
void graph::RemoveChild( dgmodule* pchild )
{	mMutex.Lock();
	{
		OrkAssert(false); // not implemented yet
		//for( orklut<PoolString,module*>::iterator it = mChildren.f
		//mChildren.RemoveItem
		//mbTopologyIsDirty = true;
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
dgmodule* graph::GetChild( const PoolString& named ) const
{	dgmodule *pret = 0;
	orklut<PoolString,Object*>::const_iterator it = mModules.find( named );
	if( it != mModules.end() )
	{
		pret = rtti::autocast( it->second );
	}
	return pret;
}
///////////////////////////////////////////////////////////////////////////////
dgmodule* graph::GetChild( size_t indexed ) const
{	dgmodule* pret = rtti::autocast(mModules.GetItemAtIndex(indexed).second);
	return pret;
}
///////////////////////////////////////////////////////////////////////////////
dyn_external* graph::GetExternal() const
{
	return mExternal;
}
///////////////////////////////////////////////////////////////////////////////
void graph::RefreshTopology(dgcontext& ctx )
{	if( false == IsComplete() ) return;
	size_t inumchild = GetNumChildren();
	dgqueue dq(this,ctx);
	mMutex.Lock();
	{	while( dq.NumPending() )
		{	///////////////////////////////////////
			orklut<int,dgmodule*> PendingAndReady(ork::EKEYPOLICY_MULTILUT);
			for( std::set<dgmodule*>::iterator it = dq.pending.begin(); it!=dq.pending.end(); it++ )
			{	dgmodule* pmod = *it;
				if( false==dq.HasPendingInputs( pmod ) )
				{	int ikey = (pmod->Key().mDepth*16)+pmod->Key().mModifier;
					PendingAndReady.AddSorted( ikey, pmod );
				}
			}
			for( orklut<int,dgmodule*>::iterator it=PendingAndReady.begin(); it!=PendingAndReady.end(); it++ )
			{	dq.QueModule(it->second,0);
			}
			///////////////////////////////////////
		}
		///////////////////////////////////////
		// put into mChildrenTopoSorted
		///////////////////////////////////////
		orklut<int,dgmodule*>& TopoSorted = LockTopoSortedChildrenForWrite(100);
		{	TopoSorted.clear();
			for( size_t ic=0; ic<inumchild; ic++ )
			{	dgmodule* pmod = GetChild(ic);
				int ikey = pmod->Key().mSerial;
				TopoSorted.AddSorted( ikey, pmod );
			}
		}
		SetTopologyDirty( false );
		UnLockTopoSortedChildren();
		///////////////////////////////////////
		///////////////////////////////////////
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
void graph::AddSourcePlug(inplugbase* pin)
{	mMutex.Lock();
	{	mSourcePlugs.push_back(pin);
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
void graph::AddSinkPlug(outplugbase* pout)
{	mMutex.Lock();
	{	mSinkPlugs.push_back(pout);
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
bool graph::IsDirty(void) const
{	OrkAssert( mbTopologyIsDirty == false );
	bool bdirty = false;
	for( orklut<PoolString,Object*>::const_iterator it=mModules.begin(); ((it!=mModules.end())&&(bdirty==false)); it++ )
	{
		dgmodule* module = rtti::autocast(it->second);
		bdirty |= module->IsDirty();
	}
	return bdirty;
}
///////////////////////////////////////////////////////////////////////////////
bool graph::IsPending() const
{	return mbInProgress;
}
///////////////////////////////////////////////////////////////////////////////
void graph::SetPending(bool bv)
{	mbInProgress=bv;
}
///////////////////////////////////////////////////////////////////////////////
bool graph::IsComplete() const
{	bool bcomp = true;
	for( orklut<PoolString,Object*>::const_iterator it=mModules.begin(); it!=mModules.end(); it++ )
	{	dgmodule* module = rtti::autocast(it->second);
		if( 0 == module )
		{	bcomp = false;
		}
	}
	return bcomp;
}
///////////////////////////////////////////////////////////////////////////////
/*void graph::SetModuleDirty( module* pmod )
{
	OrkAssert( mbTopologyIsDirty == false );
	//std::deque<module*>::const_iterator it = std::find( mModuleQueue.begin(), mModuleQueue.end(), pmod );
	//if( it == mModuleQueue.end() ) // NOT IN QUEUE ALREADY, so ADD IT
	//{
	//mModuleQueue.push_back( pmod );
	//}
	//if( mScheduler )
	//{
	//	mScheduler->QueueModule( pmod );
	//}
}*/
///////////////////////////////////////////////////////////////////////////////
bool graph::SerializeConnections(ork::reflect::ISerializer &ser) const
{	for( orklut<ork::PoolString,ork::Object*>::const_iterator it=mModules.begin(); it!=mModules.end(); it++ )
	{	ork::Object* pobj = it->second;
		ork::dataflow::dgmodule* pdgmodule = ork::rtti::autocast(pobj);
		if( pdgmodule )
		{	pdgmodule->SetName( it->first );
		}
	}
	/////////////////////////////////////////////
	int inumlinks = 0;
	for( orklut<ork::PoolString,ork::Object*>::const_iterator it = mModules.begin(); it != mModules.end(); it++ )
	{	dgmodule* pmodule = rtti::autocast( it->second );
		if( pmodule )
		{	int inuminputplugs = pmodule->GetNumInputs();
			for( int ip=0; ip<inuminputplugs; ip++ )
			{	ork::dataflow::inplugbase* pinput = pmodule->GetInput(ip);
				const ork::dataflow::outplugbase* poutput = pinput->GetExternalOutput();
				if( poutput )
				{	module* poutmodule = rtti::autocast(poutput->GetModule());
					inumlinks++;
				}
			}
		}
	}
	ser.Serialize( inumlinks );
	/////////////////////////////////////////////
	for( orklut<ork::PoolString,ork::Object*>::const_iterator it = mModules.begin(); it != mModules.end(); it++ )
	{	dgmodule* pmodule = rtti::autocast( it->second );
		if( pmodule )
		{	int inuminputplugs = pmodule->GetNumInputs();
			for( int ip=0; ip<inuminputplugs; ip++ )
			{	ork::dataflow::inplugbase* pinput = pmodule->GetInput(ip);
				const ork::dataflow::outplugbase* poutput = pinput->GetExternalOutput();
				if( poutput )
				{	module* poutmodule = rtti::autocast(poutput->GetModule());
					ser.Serialize(ork::PieceString(pmodule->GetName().c_str()));
					ser.Serialize(ork::PieceString(pinput->GetName().c_str()));
					ser.Serialize(ork::PieceString(poutput->GetModule()->GetName().c_str()));
					ser.Serialize(ork::PieceString(poutput->GetName().c_str()));
				}
			}
		}
	}
	/////////////////////////////////////////////
	return true;
}
///////////////////////////////////////////////////////////////////////////////
bool graph::DeserializeConnections(ork::reflect::IDeserializer &deser)
{	for( orklut<ork::PoolString,ork::Object*>::const_iterator it=mModules.begin(); it!=mModules.end(); it++ )
	{	ork::Object* pobj = it->second;
		ork::dataflow::dgmodule* pdgmodule = ork::rtti::autocast(pobj);
		if( pdgmodule )
		{	pdgmodule->SetName( it->first );
			pdgmodule->SetParent( this );
		}
	}
	/////////////////////////////////////////////
	// read number of links
	int inumlinks = 0;
	deser.Deserialize( inumlinks );
	/////////////////////////////////////////////
	for( int il=0; il<inumlinks; il++ )
	{	ork::ResizableString inp_mod_name;
		ork::ResizableString inp_plg_name;
		ork::ResizableString out_mod_name;
		ork::ResizableString out_plg_name;
		deser.Deserialize( inp_mod_name );
		deser.Deserialize( inp_plg_name );
		deser.Deserialize( out_mod_name );
		deser.Deserialize( out_plg_name );
		/////////////////////////
		// make the connection
		/////////////////////////
		orklut<ork::PoolString,ork::Object*>::const_iterator it_inp = mModules.find(ork::AddPooledString(inp_mod_name));
		orklut<ork::PoolString,ork::Object*>::const_iterator it_out = mModules.find(ork::AddPooledString(out_mod_name));
		if( it_inp!=mModules.end() && it_out!=mModules.end() )
		{	dgmodule* pinp_mod = rtti::autocast( it_inp->second );
			dgmodule* pout_mod = rtti::autocast( it_out->second );
			ork::dataflow::inplugbase* inp_plug = rtti::autocast(pinp_mod->GetInputNamed( ork::AddPooledString(inp_plg_name) ));
			ork::dataflow::outplugbase* out_plug = rtti::autocast(pout_mod->GetOutputNamed( ork::AddPooledString(out_plg_name) ));
			if( inp_plug != 0 && out_plug != 0 )
			{	inp_plug->SafeConnect( *this, out_plug );
			}
		}
	}
	/////////////////////////////////////////////
	return true;
}
///////////////////////////////////////////////////////////////////////////////
bool graph::PreDeserialize(reflect::IDeserializer &)
{
	LockTopoSortedChildrenForWrite(101);
	Clear();
	mModules.clear();
	mExternal = 0;
	UnLockTopoSortedChildren();
	return true;
}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
