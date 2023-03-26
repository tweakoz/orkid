////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/prop.h>
#include <ork/rtti/downcast.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/any.h>

namespace ork { namespace lev2 { struct DisplayBuffer; } }

namespace ork { namespace dataflow {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// parallel dataflow
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// technique - method by which to compute
///////////////////////////////////////////////////////////////////////////////

struct technique
{
	
};

///////////////////////////////////////////////////////////////////////////////
// work unit for a module to compute
///////////////////////////////////////////////////////////////////////////////

class workunit
{
	Affinity			mAffinity;
	technique			mTechnique;
	dgmoduleinst_ptr_t			mpModule;
	int					mModuleWuIndex;
	cluster*			mCluster;
	anyp				mContextData;
	
public:

	workunit( dgmoduleinst_ptr_t pmod, cluster* pclus, int imwuidx=-1 );

	void			SetAffinity( const Affinity& afn ) { mAffinity=afn; }
	const Affinity&	GetAffinity() const { return mAffinity; }

	int				GetModuleWuIndex() const { return mModuleWuIndex; }
	void			SetModuleWuIndex( int idx ) { mModuleWuIndex=idx; }

	dgmoduleinst_ptr_t		GetModule() const { return mpModule; }

	void			SetContextData( anyp pd ) { mContextData=pd; }
	const anyp&		GetContextData() const { return mContextData; }

	cluster*		GetCluster() const { return mCluster; }
};

///////////////////////////////////////////////////////////////////////////////
// cluster of workunits to be processed in ANY ORDER
//  => no inner dependencies
///////////////////////////////////////////////////////////////////////////////

class cluster
{
	int						miNumWorkUnitsAssigned;
	int						miNumWorkUnitsDone;
	int						miSerialNumber;
	int						miNumWorkUnits;

	/////////////////////////////////////
	LockedResource< orkset<dgmoduleinst_ptr_t> >				mModules;
	LockedResource< orkvector<workunit*> >			mWorkUnits;
	/////////////////////////////////////

public:

	cluster( /*sequence* pseq*/ ); 

	workunit*		GetPendingWorkUnit();
	int				GetNumPendingWorkUnits() const;
	int				GetNumAssignedWorkUnits() const;
	int				GetNumCompletedWorkUnits() const;
	int				GetNumWorkUnits() const;
	int				GetSerialNumber() const { return miSerialNumber; }
	void			AddWorkUnit(workunit*wu); 
	void			NotifyWorkUnitFinished(workunit*wu);
	void			AddModule( dgmoduleinst_ptr_t pmod);
	void			SetSerialNumber( int isn ) { miSerialNumber=isn; }

	const orkset<dgmoduleinst_ptr_t>		LockModulesForRead();
	void						UnLockModules() const;

	LockedResource< orkvector<workunit*> >& GetWorkUnits() { return mWorkUnits; }
	const LockedResource< orkvector<workunit*> >& GetWorkUnits() const { return mWorkUnits; }

	//const orkvector<workunit*>& LockWorkUnitsForRead() const; 

};

///////////////////////////////////////////////////////////////////////////////
// processor is a parallel execution unit which processess workunit
//  they are heterogenous, a gpu is a processor, as is a cpu (duh....)
///////////////////////////////////////////////////////////////////////////////

struct gpgpucontext
{
	orkmap< std::string, ork::lev2::DisplayBuffer* >	mDisplayBuffers;
};

///////////////////////////////////////////////////////////////////////////////

class processor
{
	Affinity					mMyAffinity;
	bool						mbTerminate;
	LockedResource< workunit* >	mCurrentWorkUnit;
	int							miNumProcessed;
	PoolString					mName;
	bool						mBusy;

public:
	
	//////////////////////////
	// Scheduler Thread

	void AssignWorkUnit( workunit* wu );
	void terminate();

	//////////////////////////
	// Any Thread

	int GetNumProcessed() const { return miNumProcessed; }
	bool IsBusy() const;
	float AffinityScore( const Affinity& ain ) const;

	void SetAffinity( const Affinity& afn ) { mMyAffinity=afn; }
	const Affinity& GetAffinity() const { return mMyAffinity; }

	//////////////////////////
	// Main thread

	processor( const char* name );

	//////////////////////////
	// Processor Thread

	void ProcessorThreadMain();			// routine called by other thread

	//////////////////////////
};

///////////////////////////////////////////////////////////////////////////////
// scheduler takes a graph, and distributes it to processors
///////////////////////////////////////////////////////////////////////////////

class scheduler
{
	typedef orkset<graphinst_ptr_t> graph_set_t;
	typedef orkset<processor*> proc_set_t;
	/////////////////////////////////////////////
	LockedResource< graph_set_t >	mGraphSet;
	LockedResource< proc_set_t >	mProcessors;
	/////////////////////////////////////////////
	//std::priority_queue<module*>mModuleQueue;

public:

	static const Affinity CpuAffinity;
	static const Affinity GpuAffinity;

	scheduler();

	//////////////////////////
	// ProcessThread
	//////////////////////////

	void Process();							// BLOCKING
	void AssignWorkUnit( workunit* wu );	// BLOCKING

	//////////////////////////
	// Main Thread
	//////////////////////////

	void terminate();

	void QueueModule( moduleinst_ptr_t pmod );

	void AddProcessor( processor* proc );
	LockedResource< proc_set_t >& GetProcessors() { return mProcessors; }

	//////////////////////////
	// ANY Thread

	void AddGraph( graphinst_ptr_t graf );
	void RemoveGraph( graphinst_ptr_t graf );

	int GetNumProcessors( const Affinity& affin ) const;

	void WaitForIdle();								// BLOCKING

	LockedResource< graph_set_t >& Graphset() { return mGraphSet; }

	//////////////////////////

};

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
