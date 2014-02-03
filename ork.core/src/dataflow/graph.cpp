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
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::graph_data,"dflow/graphdata");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::graph_inst,"dflow/graph");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace dataflow {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void graph_data::Describe()
{
	ork::reflect::RegisterMapProperty("Modules", &graph_data::mModules);
	ork::reflect::AnnotatePropertyForEditor< graph_data >("Modules", "editor.factorylistbase", "dflow/dgmodule" );
}
graph_data::graph_data()
	: mbTopologyIsDirty( true )
	, mMutex( "GraphMutex" )
{
}
graph_data::graph_data(const graph_data& oth)
	: mbTopologyIsDirty( true )
	, mMutex( "GraphMutex" )
{
}
graph_data::~graph_data()
{
}
///////////////////////////////////////////////////////////////////////////////
void graph_data::AddChild( const PoolString& named, dgmodule* pchild )
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
void graph_data::AddChild( const char* named, dgmodule* pchild )
{
	AddChild( AddPooledString(named), pchild );
}
///////////////////////////////////////////////////////////////////////////////
void graph_data::RemoveChild( dgmodule* pchild )
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
dgmodule* graph_data::GetChild( const PoolString& named ) const
{	dgmodule *pret = 0;
	orklut<PoolString,Object*>::const_iterator it = mModules.find( named );
	if( it != mModules.end() )
	{
		pret = rtti::autocast( it->second );
	}
	return pret;
}
///////////////////////////////////////////////////////////////////////////////
dgmodule* graph_data::GetChild( size_t indexed ) const
{	dgmodule* pret = rtti::autocast(mModules.GetItemAtIndex(indexed).second);
	return pret;
}
///////////////////////////////////////////////////////////////////////////////
bool graph_data::CanConnect( const inplugbase* pin, const outplugbase* pout ) const
{	return ((&pin->GetDataTypeId()) == (&pout->GetDataTypeId()));
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void graph_inst::Describe()
{
	ork::reflect::RegisterProperty( "zzz_connections", & graph_inst::SerializeConnections, & graph_inst::DeserializeConnections );
	ork::reflect::AnnotatePropertyForEditor< graph_inst >("zzz_connections", "editor.visible", "false" );

	ork::reflect::AnnotateClassForEditor< graph_inst >("editor.object.ops", ConstString("dfgraph:dflowgraphedit import:dflowgraphimport export:dflowgraphexport") );
}
///////////////////////////////////////////////////////////////////////////////
graph_inst::graph_inst()
	: mScheduler( 0 )
	, mbAccumulateWork( false )
	, mbInProgress(false)
	, mExternal(0)
{
	mChildrenTopoSorted.LockForWrite().SetKeyPolicy(ork::EKEYPOLICY_MULTILUT);
	mChildrenTopoSorted.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
graph_inst::~graph_inst()
{	SetScheduler( 0 );

	for( orklut<ork::PoolString,ork::Object*>::const_iterator it=mModules.begin(); it!=mModules.end(); it++ )
	{
		ork::Object* pobj = it->second;
		delete pobj;
	}

}
///////////////////////////////////////////////////////////////////////////////
graph_inst::graph_inst( const graph_inst& oth )
	: mScheduler( 0 )
	, mbAccumulateWork( false )
	, mbInProgress(false)
	, mExternal(0)
{
}
///////////////////////////////////////////////////////////////////////////////
bool graph_inst::DoNotify(const ork::event::Event *event)
{	if( const ItemRemovalEvent* pev = rtti::autocast(event) )
	{	if( pev->mProperty == graph_inst::GetClassStatic()->Description().FindProperty("Modules") )
		{	ork::PoolString ps = pev->mKey.Get<ork::PoolString>();
			ork::Object* pobj = pev->mOldValue.Get<ork::Object*>();
			delete pobj;
			return true;
		}
	}
	else if( const MapItemCreationEvent* pev = rtti::autocast(event) )
	{	if( pev->mProperty == graph_inst::GetClassStatic()->Description().FindProperty("Modules") )
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
void graph_inst::Clear()
{	mSourcePlugs.clear();
	mSinkPlugs.clear();
	while( false==mModuleQueue.empty() ) mModuleQueue.pop();
	mbTopologyIsDirty = true;
	mScheduler = 0;
	mbAccumulateWork = false;
	mbInProgress = false;
}
///////////////////////////////////////////////////////////////////////////////
void graph_inst::ReInit()
{
	for( orklut<ork::PoolString,ork::Object*>::const_iterator it=mModules.begin(); it!=mModules.end(); it++ )
	{
		ork::Object* pobj = it->second;
		delete pobj;
	}
	Clear();
}
///////////////////////////////////////////////////////////////////////////////
const orklut<int,dgmodule*>& graph_inst::LockTopoSortedChildrenForRead(int lid) const
{	const orklut<int,dgmodule*>& topos = mChildrenTopoSorted.LockForRead(lid);
	OrkAssert( IsTopologyDirty() == false );
	return topos;
}
///////////////////////////////////////////////////////////////////////////////
orklut<int, dgmodule*>& graph_inst::LockTopoSortedChildrenForWrite(int lid)
{	orklut<int,dgmodule*>& topos = mChildrenTopoSorted.LockForWrite(lid);
	return topos;
}
///////////////////////////////////////////////////////////////////////////////
void graph_inst::UnLockTopoSortedChildren() const
{	mChildrenTopoSorted.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
void graph_inst::SetScheduler( scheduler* psch )
{	if( psch )
	{	psch->AddGraph( this );
	}
	else if( mScheduler )
	{	mScheduler->RemoveGraph( this );
	}
	mScheduler = psch;
}
///////////////////////////////////////////////////////////////////////////////
void graph_inst::UnBindExternal()
{
	mMutex.Lock();
	{
		mExternal = 0;
		mbTopologyIsDirty = true;
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
void graph_inst::BindExternal( dyn_external* pext )
{
	mMutex.Lock();
	{
		mExternal = pext;
		mbTopologyIsDirty = true;
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
dyn_external* graph_inst::GetExternal() const
{
	return mExternal;
}
///////////////////////////////////////////////////////////////////////////////
void graph_inst::RefreshTopology(dgcontext& ctx )
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
void graph_inst::AddSourcePlug(inplugbase* pin)
{	mMutex.Lock();
	{	mSourcePlugs.push_back(pin);
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
void graph_inst::AddSinkPlug(outplugbase* pout)
{	mMutex.Lock();
	{	mSinkPlugs.push_back(pout);
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
bool graph_inst::IsDirty(void) const
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
bool graph_inst::IsPending() const
{	return mbInProgress;
}
///////////////////////////////////////////////////////////////////////////////
void graph_inst::SetPending(bool bv)
{	mbInProgress=bv;
}
///////////////////////////////////////////////////////////////////////////////
bool graph_inst::IsComplete() const
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
bool graph_inst::SerializeConnections(ork::reflect::ISerializer &ser) const
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
bool graph_inst::DeserializeConnections(ork::reflect::IDeserializer &deser)
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
bool graph_inst::PreDeserialize(reflect::IDeserializer &)
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
