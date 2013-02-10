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
namespace ork { namespace dataflow {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
dgregisterblock::dgregisterblock(int isize)
	: mBlock( isize )
{
	for( int io=0; io<isize; io++ )
	{
		mBlock.direct_access(io).mIndex = (isize-1)-io;
		mBlock.direct_access(io).mChildren.clear();
	}
}
///////////////////////////////////////////////////////////////////////////////
dgregister* dgregisterblock::Alloc()
{
	dgregister* reg = mBlock.allocate();
	OrkAssert( reg != 0 );
	mAllocated.insert(reg);
	return reg;
}
///////////////////////////////////////////////////////////////////////////////
void dgregisterblock::Free( dgregister* preg )
{
	preg->mChildren.clear();
	mBlock.deallocate( preg );
	mAllocated.erase(preg);
}
///////////////////////////////////////////////////////////////////////////////
void dgregisterblock::Clear()
{
	orkvector<dgregister*> deallocvec;
	for( orkset<dgregister*>::const_iterator it=mAllocated.begin(); it!=mAllocated.end(); it++ )
	{	dgregister* reg = (*it);
		deallocvec.push_back(reg);
	}
	for( orkvector<dgregister*>::iterator it=deallocvec.begin(); it!=deallocvec.end(); it++ )
	{	dgregister* reg = *it;
		Free(reg);
	}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void dgregister::SetModule( dgmodule*pmod )
{	
	if( pmod )
	{
		mpOwner = pmod;

		/////////////////////////////////////////
		// for each output,
		//  find any modules connected to this module
		//  and mark it as a dependency
		/////////////////////////////////////////
		
		int inumouts = pmod->GetNumOutputs();
		for( int io=0; io<inumouts; io++ )
		{	
			const outplugbase* poutplug = pmod->GetOutput(io);
			size_t inumcon = poutplug->GetNumExternalOutputConnections(); 
			
			for( size_t ic=0; ic<inumcon; ic++ )
			{	
				inplugbase* pinp = poutplug->GetExternalOutputConnection(ic);

				if( pinp && pinp->GetModule()!=pmod ) // it is dependent on pmod
				{
					dgmodule* childmod = rtti::autocast( pinp->GetModule() );
					mChildren.insert( childmod );
				}
			}
		}
	}
}
//////////////////////////////////
dgregister::dgregister( dgmodule*pmod, int idx ) 
	: mIndex(idx)
	, mpOwner(0)
{
	SetModule( pmod );
}
///////////////////////////////////////////////////////////////////////////////
// prune no longer needed registers
///////////////////////////////////////////////////////////////////////////////
void dgcontext::Prune(dgmodule* pmod) // we are done with pmod, prune registers associated with it
{
	// check all register sets
	for( auto itc=mRegisterSets.begin(); itc!=mRegisterSets.end(); itc++ )
	{
		dgregisterblock* regs = itc->second;
		const orkset<dgregister*>& allocated = regs->Allocated();
		orkvector<dgregister*> deallocvec;

		// check all allocated registers
		for( orkset<dgregister*>::const_iterator it=allocated.begin(); it!=allocated.end(); it++ )
		{	dgregister* reg = (*it);	
			
			std::set<dgmodule*>::iterator itfind = reg->mChildren.find(pmod);
			
			// were any allocated registers feeding this module?
			// is it also not a probed module ?
			//  if so, they can be pruned!!!
			bool b_didfeed_pmod = (itfind != reg->mChildren.end());
			bool b_is_probed = (reg->mpOwner==mpProbeModule);

			if( b_didfeed_pmod && (false==b_is_probed) )
			{	
				reg->mChildren.erase(itfind);
			}
			if( 0 == reg->mChildren.size() )
			{	
				deallocvec.push_back(reg);
			}
		}
		for( orkvector<dgregister*>::iterator it=deallocvec.begin(); it!=deallocvec.end(); it++ )
		{	dgregister* reg = *it;
			regs->Free(reg);
		}
	}
}
//////////////////////////////////////////////////////////
void dgcontext::Alloc(outplugbase* poutplug)
{	const std::type_info* tinfo = & poutplug->GetDataTypeId();
	orkmap<const std::type_info*, dgregisterblock*>::iterator itc=mRegisterSets.find( tinfo );
	if( itc != mRegisterSets.end() )
	{	dgregisterblock* regs = itc->second;
		dgregister* preg = regs->Alloc();
		preg->mpOwner = ork::rtti::autocast(poutplug->GetModule());
		poutplug->SetRegister(preg);
	}
}
//////////////////////////////////////////////////////////
void dgcontext::SetRegisters( const std::type_info*pinfo,dgregisterblock* pregs )
{	mRegisterSets[ pinfo ] =  pregs;
}
//////////////////////////////////////////////////////////
dgregisterblock* dgcontext::GetRegisters(const std::type_info*pinfo)
{	orkmap<const std::type_info*, dgregisterblock*>::const_iterator it=mRegisterSets.find(pinfo);
	return (it==mRegisterSets.end()) ? 0 : it->second;
}
//////////////////////////////////////////////////////////
void dgcontext::Clear()
{	for( orkmap<const std::type_info*, dgregisterblock*>::const_iterator	it =  mRegisterSets.begin();
																			it != mRegisterSets.end();
																			it ++ )
	{	dgregisterblock* pregs = it->second;
		pregs->Clear();
	}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool dgqueue::IsPending( dgmodule* mod)
{	return (pending.find(mod)!=pending.end());
}
//////////////////////////////////////////////////////////
int dgqueue::NumDownstream( dgmodule* mod )
{	int inumoutcon = 0;
	int inumouts = mod->GetNumOutputs();
	for( int io=0; io<inumouts; io++ )
	{	const outplugbase* poutplug = mod->GetOutput(io);
		inumoutcon+=(int)poutplug->GetNumExternalOutputConnections();
	}
	return inumoutcon;		
}
//////////////////////////////////////////////////////////
int dgqueue::NumPendingDownstream( dgmodule* mod )
{	int inumoutcon = 0;
	int inumouts = mod->GetNumOutputs();
	for( int io=0; io<inumouts; io++ )
	{	const outplugbase* poutplug = mod->GetOutput(io);
		size_t inumcon = poutplug->GetNumExternalOutputConnections();
		for( size_t ic=0; ic<inumcon; ic++ )
		{	inplugbase* pinplug = poutplug->GetExternalOutputConnection(ic);
			dgmodule* pconmod = rtti::autocast(pinplug->GetModule());
			inumoutcon += int(IsPending(pconmod));
		}
	}
	return inumoutcon;		
}
//////////////////////////////////////////////////////////
void dgqueue::AddModule( dgmodule* mod )
{	mod->Key().mDepth=0;
	mod->Key().mSerial = -1;
	int inumo = mod->GetNumOutputs();
	mod->Key().mModifier = s8(-inumo);
	for( int io=0; io<inumo; io++ )
	{
		mod->GetOutput(io)->SetRegister(0);
	}
	pending.insert(mod);
}
//////////////////////////////////////////////////////////
void dgqueue::PruneRegisters(dgmodule* pmod )
{	mCompCtx.Prune(pmod);
}
//////////////////////////////////////////////////////////
void dgqueue::QueModule( dgmodule* pmod, int irecd )
{	
	if( pending.find(pmod) != pending.end() ) // is pmod pending ?
	{	
		if( mModStack.size() )
		{	// check the top of stack for registers to prune
			dgmodule* prev = mModStack.top();
			PruneRegisters(prev);
		}
		mModStack.push(pmod);
		///////////////////////////////////
		pmod->Key().mSerial = s8(mSerial++);
		pending.erase(pmod);
		///////////////////////////////////
		int inuminps = pmod->GetNumInputs();
		int inumouts = pmod->GetNumOutputs();
		///////////////////////////////////
		// assign new registers
		///////////////////////////////////
		int inumincon = 0;
		for( int ii=0; ii<inuminps; ii++ )
		{	inplugbase* pinpplug = pmod->GetInput(ii);
			inumincon += int( pinpplug->IsConnected() );
		}
		for( int io=0; io<inumouts; io++ )
		{	outplugbase* poutplug = pmod->GetOutput(io);
			if( poutplug->IsConnected() || (inumincon!=0) ) // if it has input or output connections
			{	
				mCompCtx.Alloc(poutplug);
			}
		}
		///////////////////////////////////
		// add dependants to register 
		///////////////////////////////////
		for( int io=0; io<inumouts; io++ )
		{	outplugbase* poutplug = pmod->GetOutput(io);
			dgregister* preg = poutplug->GetRegister();
			if( preg )
			{	size_t inumcon = poutplug->GetNumExternalOutputConnections(); 
				for( size_t ic=0; ic<inumcon; ic++ )
				{	inplugbase* pinp = poutplug->GetExternalOutputConnection(ic);
					if( pinp && pinp->GetModule()!=pmod )
					{	dgmodule* dmod = rtti::autocast( pinp->GetModule() );
						preg->mChildren.insert(dmod);
					}
				}
			}
		}
		///////////////////////////////////
		// completed "pmod"
		//  add connected with no other pending deps
		///////////////////////////////////
		for( int io=0; io<inumouts; io++ )
		{	const outplugbase* poutplug = pmod->GetOutput(io);
			if( poutplug->GetRegister() )
			{	size_t inumcon = poutplug->GetNumExternalOutputConnections(); 
				for( size_t ic=0; ic<inumcon; ic++ )
				{	inplugbase* pinp = poutplug->GetExternalOutputConnection(ic);
					if( pinp && pinp->GetModule()!=pmod )
					{	dgmodule* dmod = rtti::autocast( pinp->GetModule() );
						if( false == HasPendingInputs( dmod ) )
						{	QueModule( dmod, irecd+1 );
						}
					}
				}
			}
		}
		if( pending.size() != 0 )
		{
			PruneRegisters(pmod);
		}
		///////////////////////////////////
		mModStack.pop();
	}
}
//////////////////////////////////////////////////////////
bool dgqueue::HasPendingInputs( dgmodule* mod )
{	bool bhaspending = false;
	int inumins = mod->GetNumInputs();
	for( int ip=0; ip<inumins; ip++ )
	{	const inplugbase* pinplug = mod->GetInput(ip);
		if( pinplug->GetExternalOutput() )
		{	const outplugbase* pout = pinplug->GetExternalOutput();
			dgmodule* pconcon = rtti::autocast(pout->GetModule());
			std::set<dgmodule*>::iterator it = pending.find(pconcon);
			if( pconcon == mod && typeid(float)==pinplug->GetDataTypeId() ) // connected to self and a float plug, must be an internal loop rate plug
			{	//pending.erase(it);
				//it = pending.end();
			}
			else if( it != pending.end() )
			{	bhaspending = true;
			}
		}
	}
	return bhaspending;
}
//////////////////////////////////////////////////////////
dgqueue::dgqueue( const graph* pg, dgcontext& ctx )
	: mSerial(0)
	, mCompCtx( ctx )
{	/////////////////////////////////////////
	// add all modules
	/////////////////////////////////////////
	size_t inumchild = pg->GetNumChildren();
	for( size_t ic=0; ic<inumchild; ic++ )
	{	dgmodule* pmod = pg->GetChild((int)ic);
		AddModule( pmod );	
	}
	/////////////////////////////////////////
	// compute depths iteratively 
	/////////////////////////////////////////
	int inumchg = -1;
	while( inumchg != 0 )
	{	inumchg = 0;
		for( size_t ic=0; ic<inumchild; ic++ )
		{	dgmodule* pmod = pg->GetChild(ic);
			int inumouts = pmod->GetNumOutputs();
			for( int op=0; op<inumouts; op++ )
			{	const outplugbase* poutplug = pmod->GetOutput(op);
				size_t inumcon = poutplug->GetNumExternalOutputConnections();
				int ilo = 0;
				for( size_t ic=0; ic<inumcon; ic++ )
				{	inplugbase* pin = poutplug->GetExternalOutputConnection(ic);
					dgmodule* pcon = rtti::autocast(pin->GetModule());
					int itd = pcon->Key().mDepth-1;
					if( itd < ilo ) ilo = itd;
				}
				if( pmod->Key().mDepth > ilo && ilo!=0)
				{	pmod->Key().mDepth = s8(ilo);
					inumchg++;
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
