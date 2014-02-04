///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2004, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/prop.h>
#include <ork/rtti/downcast.h>
#include <ork/kernel/mutex.h>
#include <ork/util/crc64.h>

#include <ork/config/config.h>
#include <ork/math/multicurve.h>
#include <ork/kernel/orkpool.h>
#include <ork/event/Event.h>

namespace ork { 

namespace dataflow {

///////////////////////////////////////////////////////////////////////////////

typedef PoolString MorphKey;
typedef PoolString MorphGroup;

enum EMorphEventType
{
	EMET_WRITE = 0,
	EMET_MORPH,
	EMET_END
};

class morph_event : public event::Event
{
	RttiDeclareConcrete(morph_event, event::Event);
public:
	EMorphEventType		meType;
	float				mfMorphValue;
	MorphGroup			mMorphGroup;
	
	morph_event() : meType(EMET_END), mfMorphValue(0.0f) {}
};

struct imorphtarget
{
	virtual void Apply() = 0;
};
struct float_morphtarget : public imorphtarget
{
	float	mfValue;
};

struct morphitem
{
	MorphKey	mKey;
	float		mWeight;
	
	morphitem() : mWeight(0.0f) {}
};

struct morphable
{
	void HandleMorphEvent( const morph_event* me );
	virtual void WriteMorphTarget( MorphKey name, float flerpval ) = 0;
	virtual void RecallMorphTarget( MorphKey name ) = 0;
	virtual void Morph1D( const morph_event* pevent ) = 0;

	static const int kmaxweights = 2;
	morphitem mMorphItems[kmaxweights];
};

///////////////////////////////////////////////////////////////////////////////

struct nodekey
{
	int	mSerial;
	int	mDepth;
	int	mModifier;

	nodekey() : mSerial(-1), mDepth(-1), mModifier(-1) {}
};
	
///////////////////////////////////////////////////////////////////////////////

template <typename vartype> class plug;
template <typename vartype> class inplug;
template <typename vartype> class outplug;
class module;
class graph_data;
class graph_inst;
class inplugbase;
class outplugbase;
typedef int Affinity;

///////////////////////////////////////////////////////////////////////////////

class workunit;
class scheduler;
class cluster;
struct dgregister;

///////////////////////////////////////////////////////////////////////////////

class node_hash
{
public:
	node_hash()
	{
		boost::crc64_init(mValue);
	}

	template <typename T> void Hash( const T& val )
	{
		crc64_compute( mValue, (const void*) & val, sizeof(val));
		boost::crc64_fin(mValue);
	}

	bool operator == ( const node_hash& oth )
	{
		return mValue==oth.mValue;
	}
	bool operator != ( const node_hash& oth )
	{
		return mValue!=oth.mValue;
	}

private:

	boost::Crc64 mValue;

};

///////////////////////////////////////////////////////////////////////////////

enum EPlugDir
{
	EPD_INPUT = 0,
	EPD_OUTPUT,
	EPD_BOTH,
	EPD_NONE,
};

enum EPlugRate
{
	EPR_EVENT = 0,	// plug will not change during the entire duration of an event
	EPR_UNIFORM,	// plug will not change during the entire duration of a single compute call
	EPR_VARYING1,	// plug may change during the entire duration of a single compute call (once per item)
	EPR_VARYING2,	// plug may change more frequently than EPR_VARYING1 (multiple times per item)
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> int MaxFanout(void);

///////////////////////////////////////////////////////////////////////////////

class  plugroot : public ork::Object
{
	RttiDeclareAbstract(plugroot,ork::Object);

public:

	plugroot( module*pmod, EPlugDir edir, EPlugRate epr, const std::type_info& tid, const char* pname );
	EPlugDir GetPlugDirection() const { return mePlugDir; }
	module* GetModule() const { return mModule; }
	bool IsDirty() const { return mbDirty; }
	void SetDirty( bool bv );
	const ork::PoolString& GetName() const { return mPlugName; }
	void SetName(const ork::PoolString& newname) { mPlugName=newname; }
	void SetModule(module*newmod) { mModule=newmod; }
	void SetRate(EPlugRate newrate) { mePlugRate=newrate; }
	const std::type_info& GetDataTypeId() const { return mTypeId; }

	EPlugRate GetPlugRate() const { return mePlugRate; }

private:

	EPlugDir				mePlugDir;
	EPlugRate				mePlugRate;
	module*					mModule;
	bool					mbDirty;
	const std::type_info&	mTypeId;
	ork::PoolString			mPlugName;

	virtual void DoSetDirty( bool bv ) {}
};

///////////////////////////////////////////////////////////////////////////////

class  inplugbase : public plugroot
{
	friend class module;
	RttiDeclareAbstract(inplugbase,plugroot);

public:

	bool IsConnected() const { return (mExternalOutput!=0); }
	bool IsMorphable() const { return (mpMorphable!=0); }

	inplugbase(	module*pmod, EPlugRate epr, const std::type_info& tid, const char* pname );
	~inplugbase();
	outplugbase* GetExternalOutput() const { return mExternalOutput; }
	void SafeConnect( graph_data& gr, outplugbase* vt );
	void Disconnect();
	void SetMorphable( morphable* pmorph ) { mpMorphable=pmorph; }
	morphable* GetMorphable() const { return mpMorphable; }
	
protected:
	outplugbase*					mExternalOutput;			// which EXTERNAL output plug are we connected to
	orkvector<outplugbase*>			mInternalOutputConnections;	// which output plugs IN THE SAME MODULE are connected to me ?
	morphable*						mpMorphable;
	
	void ConnectInternal( outplugbase* vt );
	void ConnectExternal( outplugbase* vt );

private:

	void DoSetDirty( bool bv ); // virtual
};

///////////////////////////////////////////////////////////////////////////////

class  outplugbase : public plugroot
{
	friend class inplugbase;
	RttiDeclareAbstract(outplugbase,plugroot);

public:

	dataflow::node_hash& RefHash() { return mOutputHash; }

	outplugbase( module*pmod, EPlugRate epr, const std::type_info& tid, const char* pname );
	~outplugbase();

	virtual int MaxFanOut() const { return 0; }

	bool IsConnected() const { return (GetNumExternalOutputConnections()!=0); }

	size_t GetNumExternalOutputConnections() const { return mExternalInputConnections.size(); }
	inplugbase* GetExternalOutputConnection( size_t idx ) const { return mExternalInputConnections[idx]; }

	dgregister* GetRegister() const { return mpRegister; }
	void SetRegister(dgregister*preg) { mpRegister=preg; }
	void Disconnect( inplugbase* pinplug );

protected:

	mutable orkvector<inplugbase*>	mExternalInputConnections;

private:

	void DoSetDirty( bool bv ); // virtual

	dataflow::node_hash		mOutputHash;
	dgregister*				mpRegister;

};

///////////////////////////////////////////////////////////////////////////////

template <typename vartype> class  outplug : public outplugbase
{
	DECLARE_TRANSPARENT_TEMPLATE_RTTI(outplug<vartype>,outplugbase);
	
public:

	void operator = ( const outplug<vartype>& oth )
	{
		new(this) outplug<vartype>(oth);
	}
	outplug( const outplug<vartype>& oth )
		: outplugbase(oth.GetModule(), oth.GetPlugRate(), oth.GetDataTypeId(), oth.GetName().c_str() )
		, mOutputData(oth.mOutputData)
	{
	}

	outplug( )
		: outplugbase(0,EPR_EVENT,typeid(vartype),0)
		, mOutputData(0)
	{
	}

	outplug( module*pmod, EPlugRate epr, const vartype* def, const char* pname ) 
		: outplugbase(pmod,epr,typeid(vartype),pname)
		, mOutputData(def)
	{
	}
	outplug( module*pmod, EPlugRate epr, const vartype* def, const std::type_info& tinfo, const char* pname ) 
		: outplugbase(pmod,epr,tinfo,pname)
		, mOutputData(def)
	{
	}
	virtual int MaxFanOut() const { return MaxFanout<vartype>(); }
	///////////////////////////////////////////////////////////////
	void ConnectData( const vartype*pd ) { mOutputData=pd; }
	///////////////////////////////////////////////////////////////
	// Internal value access (will not ever give effective value)
	const vartype& GetInternalData() const;
	///////////////////////////////////////////////////////////////
	const vartype& GetValue() const; // virtual
	///////////////////////////////////////////////////////////////

private:

	const vartype* mOutputData;
};

///////////////////////////////////////////////////////////////////////////////

template <typename vartype> class  inplug : public inplugbase
{
	DECLARE_TRANSPARENT_TEMPLATE_ABSTRACT_RTTI(inplug<vartype>,inplugbase);

public:

	explicit inplug( module*pmod, EPlugRate epr, vartype& def, const char* pname ) 
		: inplugbase(pmod,epr,typeid(vartype),pname)
		, mDefault( def )
	{
	}

	void SetDefault( const vartype& val ) { mDefault=val; }
	const vartype& GetDefault() const { return mDefault; }

	////////////////////////////////////////////

	void Connect( outplug<vartype>* vt ) { ConnectExternal(vt); }
	void Connect( outplug<vartype>& vt ) { ConnectExternal(&vt); }

	///////////////////////////////////////////////////////////////

	template <typename T> void GetTypedInput( outplug<T>* & oval )
	{	outplugbase* oplug = mExternalOutput;
		oval = rtti::downcast< outplug<T>* >( oplug );
	}

	///////////////////////////////////////////////////////////////

	inline const vartype& GetValue() // virtual
	{	outplug<vartype>* connected = 0;
		GetTypedInput(connected);
		return (connected!=0) ? (connected->GetValue()) : mDefault;
	}

	///////////////////////////////////////////////////////////////

protected:

	vartype&			mDefault;	// its a reference to prevent large plugs taking up memory
									// in the unconnected state it can connect to a global dummy

};

///////////////////////////////////////////////////////////////////////////////

class  floatinplug : public inplug<float>
{
	RttiDeclareAbstract(floatinplug,inplug<float>);

public:

	floatinplug( module*pmod, EPlugRate epr, float& def, const char* pname ) : inplug<float>(pmod,epr,def,pname) {}

private:

	void SetValAccessor( float const & val) { mDefault=val; }
	void GetValAccessor( float & val) const { val=mDefault; } 
};

///////////////////////////////////////////////////////////////////////////////

class  vect3inplug : public inplug<CVector3>
{
	RttiDeclareAbstract(vect3inplug,inplug<CVector3>);

public:

	vect3inplug( module*pmod, EPlugRate epr, CVector3& def, const char* pname ) : inplug<CVector3>(pmod,epr,def,pname) {}

private:

	void SetValAccessor( CVector3 const & val) { mDefault=val; }
	void GetValAccessor( CVector3 & val) const { val=mDefault; } 
};

///////////////////////////////////////////////////////////////////////////////

template <typename xf> class  floatinplugxf : public floatinplug
{
	DECLARE_TRANSPARENT_TEMPLATE_ABSTRACT_RTTI(floatinplugxf<xf>,floatinplug);

public:

	explicit floatinplugxf( module*pmod, EPlugRate epr, float& def, const char* pname ) 
		: floatinplug(pmod,epr,def,pname)
		, mtransform()
	{
	}

	///////////////////////////////////////////////////////////////

	xf& GetTransform() { return mtransform; }

	///////////////////////////////////////////////////////////////

	inline const float& GetValue() // virtual
	{	outplug<float>* connected = 0;
		GetTypedInput(connected);
		mtransformed = mtransform.transform((connected!=0) ? (connected->GetValue()) : mDefault);
		return mtransformed;
	}

private:

	xf mtransform;
	mutable float mtransformed;
	ork::Object* XfAccessor() { return & mtransform; }
};

///////////////////////////////////////////////////////////////////////////////

template <typename xf> class  vect3inplugxf : public vect3inplug
{
	DECLARE_TRANSPARENT_TEMPLATE_ABSTRACT_RTTI(vect3inplugxf<xf>,vect3inplug);

public:

	explicit vect3inplugxf( module*pmod, EPlugRate epr, CVector3& def, const char* pname ) 
		: vect3inplug(pmod,epr,def,pname)
		, mtransform()
	{
	}

	///////////////////////////////////////////////////////////////

	xf& GetTransform() { return mtransform; }

	///////////////////////////////////////////////////////////////

	inline const CVector3& GetValue() // virtual
	{	outplug<CVector3>* connected = 0;
		GetTypedInput(connected);
		mtransformed = mtransform.transform((connected!=0) ? (connected->GetValue()) : mDefault);
		return mtransformed;
	}

private:

	xf mtransform;
	mutable CVector3 mtransformed;
	ork::Object* XfAccessor() { return & mtransform; }
};

///////////////////////////////////////////////////////////////////////////////
class modscabias : public ork::Object
{
	RttiDeclareConcrete(modscabias,ork::Object);

public:

	float GetMod() const { return mfMod; }
	float GetScale() const { return mfScale; }
	float GetBias() const { return mfBias; }
	void SetMod(float val) { mfMod=val; }
	void SetScale(float val) { mfScale=val; }
	void SetBias(float val) { mfBias=val; }

	modscabias()
		: mfMod( 1.0f )
		, mfScale( 1.0f )
		, mfBias( 0.0f )
	{
	}
	
private:

	float				mfMod;
	float				mfScale;
	float				mfBias;

	
};

///////////////////////////////////////////////////////////////////////////////

class  floatxfitembase : public ork::Object
{
	RttiDeclareAbstract(floatxfitembase,ork::Object);

public:

	virtual float transform( float inp ) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

class  floatxfmsbcurve : public floatxfitembase
{
	RttiDeclareConcrete(floatxfmsbcurve,floatxfitembase);

public:

	float GetMod() const { return mModScaleBias.GetMod(); }
	float GetScale() const { return mModScaleBias.GetScale(); }
	float GetBias() const { return mModScaleBias.GetBias(); }

	void SetMod(float val) { mModScaleBias.SetMod(val); }
	void SetScale(float val) { mModScaleBias.SetScale(val); }
	void SetBias(float val) { mModScaleBias.SetBias(val); }

	ork::Object* CurveAccessor() { return & mMultiCurve1d; }
	ork::Object* ModScaleBiasAccessor() { return & mModScaleBias; }

	floatxfmsbcurve()
		: mbDoModScaBia( false )
		, mbDoCurve( false )
	{
	}

	float transform( float input ) const; // virtual

private:

	ork::MultiCurve1D	mMultiCurve1d;
	modscabias			mModScaleBias;
	bool				mbDoCurve;
	bool				mbDoModScaBia;
};

///////////////////////////////////////////////////////////////////////////////

class  floatxfmodstep : public floatxfitembase
{
	RttiDeclareConcrete(floatxfmodstep,floatxfitembase);
	
public:

	floatxfmodstep()
		: mMod( 1.0f )
		, miSteps( 4 )
		, mOutputScale(1.0f)
		, mOutputBias(1.0f)
	{
	}

	float transform( float input ) const; // virtual

private:

	float				mMod;
	int					miSteps;
	float				mOutputScale;
	float				mOutputBias;
};

///////////////////////////////////////////////////////////////////////////////

class floatxf : public ork::Object
{
	RttiDeclareAbstract(floatxf,ork::Object);
	
public:
	
	float transform( float inp ) const;
	floatxf();
	~floatxf();

private:

	orklut<ork::PoolString,ork::Object*> mTransforms;
	int										miTest;
};

///////////////////////////////////////////////////////////////////////////////

class  vect3xf : public ork::Object
{
	RttiDeclareConcrete(vect3xf,ork::Object);

public:

	const floatxf&	GetTransformX() const { return mTransformX; }
	const floatxf&	GetTransformY() const { return mTransformY; }
	const floatxf&	GetTransformZ() const { return mTransformZ; }

	CVector3 transform( const CVector3& input ) const;

private:

	ork::Object* TransformXAccessor() { return & mTransformX; }
	ork::Object* TransformYAccessor() { return & mTransformY; }
	ork::Object* TransformZAccessor() { return & mTransformZ; }

	floatxf	mTransformX;
	floatxf	mTransformY;
	floatxf	mTransformZ;
};

typedef floatinplugxf< floatxf > floatxfinplug;
typedef vect3inplugxf< vect3xf > vect3xfinplug;

///////////////////////////////////////////////////////////////////////////////

class  module : public ork::Object
{
	RttiDeclareAbstract(module,ork::Object);

public:

	////////////////////////////////////////////
	const dataflow::node_hash& GetModuleHash() { return mModuleHash; }
	virtual void UpdateHash() { mModuleHash=dataflow::node_hash(); }
	////////////////////////////////////////////
	const PoolString& GetName() const { return mName; }
	void SetName( const PoolString& val ) { mName = val; }
	virtual void DoSetInputDirty( inplugbase* plg ) {}
	virtual void DoSetOutputDirty( outplugbase* plg ) {}
	void SetInputDirty( inplugbase* plg );
	void SetOutputDirty( outplugbase* plg );
	virtual bool IsDirty(void);
	////////////////////////////////////////////
	virtual int GetNumInputs() const { return 0; }
	virtual int GetNumOutputs() const { return 0; }
	virtual inplugbase* GetInput(int idx) { return 0; }
	virtual outplugbase* GetOutput(int idx) { return 0; }
	inplugbase* GetInputNamed( const PoolString& named ) ;
	outplugbase* GetOutputNamed( const PoolString& named );
	////////////////////////////////////////////
	virtual int GetNumChildren() const { return 0; }
	virtual module* GetChild( int idx ) const { return 0; }
	module* GetChildNamed( const ork::PoolString& named ) const;
	////////////////////////////////////////////
	template <typename T> void GetTypedInput(int idx, inplug<T>* & oval )
	{	inplugbase* oplug = GetInput(idx);
		oval = rtti::downcast< inplug<T>* >( oplug );
	}
	////////////////////////////////////////////
	virtual void OnTopologyUpdate(void) {}
	virtual void OnStart() {}
	////////////////////////////////////////////
	template <typename T> void GetTypedOutput(int idx, outplug<T>* & oval )
	{	outplugbase* oplug = GetOutput(idx);
		oval = rtti::downcast< outplug<T>* >( oplug );
	}
	////////////////////////////////////////////
	void SetMorphable( morphable* pmorph ) { mpMorphable=pmorph; }
	morphable* GetMorphable() const { return mpMorphable; }
	bool IsMorphable() const { return (mpMorphable!=0); }
	////////////////////////////////////////////

protected:

	PoolString				mName;
	dataflow::node_hash		mModuleHash;
	morphable*				mpMorphable;

	void AddDependency( outplugbase& pout, inplugbase& pin ); 
};

///////////////////////////////////////////////////////////////////////////////

class  dgmodule : public module
{
	RttiDeclareAbstract(dgmodule,module);

public:
	dgmodule();
	////////////////////////////////////////////
	void SetParent( graph_data* par ) { mParent=par; }
	nodekey& Key() { return mKey; }
	const nodekey& Key() const { return mKey; }
	graph_data* GetParent() const { return mParent; }
	////////////////////////////////////////////
	//bool IsOutputDirty( const outplugbase* pplug ) const;
	////////////////////////////////////////////
	virtual void Compute( workunit* wu ) = 0;
	////////////////////////////////////////////
	void SetAffinity(Affinity ia) { mAffinity=ia; }
	const Affinity& GetAffinity() const { return mAffinity; }
	////////////////////////////////////////////
	void DivideWork( const scheduler& sch, cluster* clus );
	virtual void CombineWork( const cluster* clus ) = 0;
	////////////////////////////////////////////
	virtual void ReleaseWorkUnit( workunit* wu );
	////////////////////////////////////////////
	virtual graph_data* GetChildGraph() const { return 0; }
	bool IsGroup() const { return GetChildGraph()!=0; }
	////////////////////////////////////////////
	const CVector2& GetGVPos() const { return mgvpos; }
	void SetGVPos(const CVector2&p) { mgvpos=p; }

protected:

	virtual void DoDivideWork( const scheduler& sch, cluster* clus );

private:

	Affinity		mAffinity;
	graph_data*		mParent;
	nodekey			mKey;
	CVector2		mgvpos;
};

class dyn_external
{
public:

	struct FloatBinding
	{
		const float*						mpSource;
		orklut<PoolString,float>::iterator	mIterator;
	};
	struct Vect3Binding
	{
		const CVector3*							mpSource;
		orklut<PoolString,CVector3>::iterator	mIterator;
	};

	const orklut<PoolString,FloatBinding>& GetFloatBindings() const { return mFloatBindings; }
	orklut<PoolString,FloatBinding>& GetFloatBindings() { return mFloatBindings; }

	const orklut<PoolString,Vect3Binding>& GetVect3Bindings() const { return mVect3Bindings; }
	orklut<PoolString,Vect3Binding>& GetVect3Bindings() { return mVect3Bindings; }

private:

	orklut<PoolString,FloatBinding>			mFloatBindings;
	orklut<PoolString,Vect3Binding>			mVect3Bindings;

};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// dgqueue (dependency graph queue)
//  this will topologically sort and queue modules in a graph so that:
//  1. all modules are computed
//	2. no module is computed before its inputs
//  3. modules are computed soon after their parents
//  4. minimal temp registers are used
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// a dgregister is an abstraction of the concept machine register
//  where the dgcontext is a thread and the dggraph is the program
// It knows the dependent clients downstream of it self (for managing the
//  lifetime of given data attached to the register

struct dgregister
{	int					mIndex;
	std::set<dgmodule*>	mChildren;
	dgmodule*			mpOwner;
	//////////////////////////////////
	void SetModule( dgmodule*pmod );
	//////////////////////////////////
	dgregister( dgmodule*pmod = 0, int idx=-1 );
	//////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

// a dgregisterblock is a pool of registers for a given machine

class dgregisterblock
{
public:

	dgregisterblock(int isize);

	dgregister* Alloc();
	void Free( dgregister* preg );
	const orkset<dgregister*>& Allocated() const { return mAllocated; }
	void Clear();
private:
	ork::pool<dgregister>		mBlock;
	orkset<dgregister*>			mAllocated;
};

///////////////////////////////////////////////////////////////////////////////

class dgcontext
{
public:
	void SetRegisters( const std::type_info*pinfo,dgregisterblock*);
	dgregisterblock* GetRegisters(const std::type_info*pinfo);
	void Clear(); 
	template <typename T> dgregisterblock* GetRegisters() { return GetRegisters( & typeid(T) ); }
	template <typename T> void SetRegisters(dgregisterblock*pregs) { SetRegisters( & typeid(T), pregs ); }
	void Prune( dgmodule* mod );
	void Alloc(outplugbase* poutplug);
	void SetProbeModule(dgmodule*pmod) { mpProbeModule=pmod; }
private:
	orkmap<const std::type_info*, dgregisterblock*>	mRegisterSets;
	dgmodule*	mpProbeModule;
};

///////////////////////////////////////////////////////////////////////////////

struct dgqueue
{	std::set<dgmodule*>		pending;
	int						mSerial;
	std::stack<dgmodule*>	mModStack;
	dgcontext&				mCompCtx;
	//////////////////////////////////////////////////////////
	bool IsPending( dgmodule* mod);
	size_t NumPending() { return pending.size(); }
	int NumDownstream( dgmodule* mod );
	int NumPendingDownstream( dgmodule* mod );
	void AddModule( dgmodule* mod );
	void PruneRegisters(dgmodule* pmod );
	void QueModule( dgmodule* pmod, int irecd );
	bool HasPendingInputs( dgmodule* mod );
	//////////////////////////////////////////////////////////
	dgqueue( const graph_inst* pg, dgcontext& ctx );
	//////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class graph_data : public ork::Object
{
	RttiDeclareAbstract(graph_data,ork::Object);	

public:
	graph_data();
	~graph_data();
	graph_data( const graph_data& oth );

	virtual bool CanConnect( const inplugbase* pin, const outplugbase* pout ) const;
	bool IsComplete() const;
	bool IsTopologyDirty() const { return mbTopologyIsDirty; }
	dgmodule* GetChild( const PoolString& named ) const;
	dgmodule* GetChild( size_t indexed ) const;
	const orklut<ork::PoolString,ork::Object*>& Modules() { return mModules; }
	size_t GetNumChildren() const { return mModules.size(); }

	void AddChild( const PoolString& named, dgmodule* pchild );
	void AddChild( const char* named, dgmodule* pchild );
	void RemoveChild( dgmodule* pchild );
	void SetTopologyDirty( bool bv ) { mbTopologyIsDirty=bv; }
	recursive_mutex& GetMutex() { return mMutex; }

	const orklut<int,dgmodule*>& LockTopoSortedChildrenForRead(int lid) const;
	orklut<int,dgmodule*>& LockTopoSortedChildrenForWrite(int lid);
	void UnLockTopoSortedChildren() const;
	virtual void Clear() = 0;

protected:

	LockedResource<	orklut<int,dgmodule*> >	mChildrenTopoSorted;
	orklut<ork::PoolString,ork::Object*> mModules;
	bool mbTopologyIsDirty;
	recursive_mutex	mMutex;

	bool SerializeConnections(ork::reflect::ISerializer &ser) const;
	bool DeserializeConnections(ork::reflect::IDeserializer &deser);
	bool PreDeserialize(reflect::IDeserializer &); // virtual

};

class graph_inst : public graph_data
{
	RttiDeclareAbstract(graph_inst,graph_data);	

public:
	const std::set<int>& OutputRegisters() const { return mOutputRegisters; }
	////////////////////////////////////////////
	graph_inst();
	~graph_inst();
	graph_inst( const graph_inst& oth );
	////////////////////////////////////////////
	void BindExternal( dyn_external* pexternal );
	void UnBindExternal();
	dyn_external* GetExternal() const;
	void Clear() override;
	////////////////////////////////////////////
	bool IsPending() const;
	bool IsDirty(void) const;
	////////////////////////////////////////////
	void SetPending(bool bv);
	////////////////////////////////////////////
	void RefreshTopology( dgcontext& ctx );
	////////////////////////////////////////////
	void SetScheduler( scheduler* psch );
	scheduler* GetScheduler() const { return mScheduler; }
	////////////////////////////////////////////

protected:

	dyn_external*									mExternal;
	scheduler*										mScheduler;
	
	bool											mbInProgress;

	std::priority_queue<dgmodule*>					mModuleQueue;

	std::set<int>									mOutputRegisters;

	bool DoNotify(const ork::event::Event *event); // virtual

};

///////////////////////////////////////////////////////////////////////////////

#define OutPlugName( name ) mPlugOut##name
#define InpPlugName( name ) mPlugInp##name
#define OutDataName( name ) mOutData##name
#define ConstructOutPlug( name, epr ) OutPlugName(name)( this, epr, &mOutData##name, #name )
#define ConstructOutTypPlug( name, epr, typ ) OutPlugName(name)( this, epr, &mOutData##name, typ, #name )
#define ConstructInpPlug( name, epr, def ) InpPlugName(name)( this, epr, def, #name )

#define DeclareFloatXfPlug( name )\
	float mf##name;\
	ork::dataflow::floatxfinplug	InpPlugName(name);\
	ork::Object* InpAccessor##name() { return & InpPlugName(name); }

#define DeclareVect3XfPlug( name )\
	ork::CVector3 mv##name;\
	ork::dataflow::vect3xfinplug	InpPlugName(name);\
	ork::Object* InpAccessor##name() { return & InpPlugName(name); }

#define RegisterFloatXfPlug( cls, name, mmin, mmax, deleg )\
	ork::reflect::RegisterProperty( #name, & cls::InpAccessor##name );\
	ork::reflect::AnnotatePropertyForEditor< cls >( #name, "editor.class", "ged.factory.plug" );\
	ork::reflect::AnnotatePropertyForEditor< cls >( #name, "ged.plug.delegate", #deleg );\
	ork::reflect::AnnotatePropertyForEditor< cls >( #name, "editor.range.min", #mmin );\
	ork::reflect::AnnotatePropertyForEditor< cls >( #name, "editor.range.max", #mmax );

#define RegisterVect3XfPlug( cls, name, mmin, mmax, deleg )\
	ork::reflect::RegisterProperty( #name, & cls::InpAccessor##name );\
	ork::reflect::AnnotatePropertyForEditor< cls >( #name, "editor.class", "ged.factory.plug" );\
	ork::reflect::AnnotatePropertyForEditor< cls >( #name, "ged.plug.delegate", #deleg );\
	ork::reflect::AnnotatePropertyForEditor< cls >( #name, "editor.range.min", #mmin );\
	ork::reflect::AnnotatePropertyForEditor< cls >( #name, "editor.range.max", #mmax );

#define DeclareFloatOutPlug( name )\
	float OutDataName(name);\
	ork::dataflow::outplug<float> OutPlugName(name);\
	ork::Object* PlgAccessor##name() { return & OutPlugName(name); }

#define DeclareVect3OutPlug( name )\
	ork::CVector3 OutDataName(name);\
	ork::dataflow::outplug<ork::CVector3> OutPlugName(name);\
	ork::Object* PlgAccessor##name() { return & OutPlugName(name); }

#define RegisterObjInpPlug( cls, name )\
	ork::reflect::RegisterProperty( #name, & cls::InpAccessor##name );\
	ork::reflect::AnnotatePropertyForEditor< cls >( #name, "editor.class", "ged.factory.plug" );\
	ork::reflect::AnnotatePropertyForEditor< cls >( #name, "ged.plug.delegate", "ged::OutPlugChoiceDelegate" );\

#define RegisterObjOutPlug( cls, name )\
	ork::reflect::RegisterProperty( #name, & cls::OutAccessor##name );\
	ork::reflect::AnnotatePropertyForEditor< cls >(#name, "editor.visible", "false" );

} }

///////////////////////////////////////////////////////////////////////////////

