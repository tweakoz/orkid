////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _SURFACE_BAKER_H
#define _SURFACE_BAKER_H

#include <QtCore/QThread>
#include <orktool/qtui/qtprocessview.h>
#include <orktool/qtui/qtapp.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

enum EBAKEMAP_TYPE
{
	EBMT_AMBOCC = 0 ,
	EBMT_DIFFUSE ,
	EBMT_USER ,
};

class LightMapperArchetype;

class BakeOps : public tool::ged::IOpsDelegate
{	public:
	static void LinkMe() {}
	LightMapperArchetype*	mpARCH;
	bool					mbFlip;
	BakeOps() : mbFlip(false) {}
	private:
	RttiDeclareConcrete( BakeOps, tool::ged::IOpsDelegate );
	virtual void Execute( ork::Object* ptarget );
};

class AtlasMapperOps : public tool::ged::IOpsDelegate
{	public:
	static void LinkMe() {}
	static float gProgress;
	LightMapperArchetype*	mpARCH;
	bool					mbIMT;
	AtlasMapperOps() : mbIMT(false) {}
	private:
	RttiDeclareConcrete( AtlasMapperOps, tool::ged::IOpsDelegate );
	virtual void Execute( ork::Object* ptarget );
};

class ImtMapperOps : public tool::ged::IOpsDelegate
{	public:
	static void LinkMe() {}
	static float gProgress;
	LightMapperArchetype*	mpARCH;
	private:
	RttiDeclareConcrete( ImtMapperOps, tool::ged::IOpsDelegate );
	virtual void Execute( ork::Object* ptarget );
};

///////////////////////////////////////////////////////////////////////////////

class BakingGroup : public ork::Object
{
	RttiDeclareConcrete(BakingGroup, ork::Object);
public:
	BakingGroup();
	const ork::file::Path& GetUserShader() const { return mUserShaderName; }
	const ork::file::Path& GetUserSdbShader() const { return mUserSdbShaderName; }
	int GetResolution() const { return miResolution; }
	int GetDiceSize() const { return miDiceSize; }
	int GetNumSamples() const { return miNumSamples; }
	float GetShadowBias() const { return mfShadowBias; }
	float GetMaxPixelDist() const { return mfMaxPixelDist; }
	float GetMaxError() const { return mfMaxError; }
	float GetAdaptive() const { return mfAdaptive; }
	float GetMaxHitDist() const { return mfMaxHitDist; }
	float GetFilterWidth() const { return mfFilterWidth; }
	float GetAtlasStretching() const { return mfAtlasStretching; }
	float GetAtlasUnification() const { return mfAtlasUnification; }
	bool ComputeShadows() const { return mbComputeShadows; }
	bool ComputeAmbOcc() const { return mbComputeAmbOcc; }
	bool UseVertexColors() const { return mbUseVertexColors; }
	const ork::PoolString& MatchItem() const { return mMatchItem; }
	const ork::PoolString& GetLights() const { return mMatchLights; }
	const ork::PoolString& GetShadowCasters() const { return mShadowCasters; }
	EBAKEMAP_TYPE GetBakeType() const { return meBakeMapType; }
private:
	ork::file::Path		mUserShaderName;
	ork::file::Path		mUserSdbShaderName;
	EBAKEMAP_TYPE		meBakeMapType;
	int					miResolution;
	int					miDiceSize;
	int					miNumSamples;
	float				mfMaxPixelDist;
	float				mfMaxError;
	float				mfShadowBias;
	float				mfAdaptive;
	float				mfMaxHitDist;
	float				mfFilterWidth;
	float				mfAtlasStretching;
	float				mfAtlasUnification;
	bool				mbComputeShadows;
	bool				mbComputeAmbOcc;
	bool				mbUseVertexColors;
	ork::PoolString		mMatchItem;
	ork::PoolString		mMatchLights;
	ork::PoolString		mShadowCasters;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FarmNode : public ork::Object
{
	RttiDeclareConcrete(FarmNode, ork::Object);
public:
	FarmNode() : mbExclusiveConnection(false) , mSSHPort(22) {}
	static void LinkMe() {}
	const PoolString& GetHostName() const { return mHostName; }
	int GetSshPort() const { return mSSHPort; }
	bool IsExclusiveConnection() const { return mbExclusiveConnection; }
private:
	PoolString						mHostName;
	int								mSSHPort;
	bool							mbExclusiveConnection;
};

///////////////////////////////////////////////////////////////////////////////

class FarmJob : public ork::Object
{
	RttiDeclareConcrete(FarmJob, ork::Object);
public:
	static void LinkMe() {}
	//const ork::file::Path& GetRgmName() const { return mRgmInputName; }
	const ork::file::Path& GetDaeName() const { return mDaeInputName; }
	const PoolString& GetBakeGroupMatch() const { return mBakeGroupMatch; }
private:
	//ork::file::Path					mRgmInputName;
	ork::file::Path					mDaeInputName;
	PoolString						mBakeGroupMatch;
};

///////////////////////////////////////////////////////////////////////////////

class FarmJobSet : public ork::Object
{
	RttiDeclareConcrete(FarmJobSet, ork::Object);
public:
	static void LinkMe() {}
	const orklut<PoolString,FarmJob*>& GetJobs() const { return mFarmJobs; }
private:
	orklut<PoolString,FarmJob*>	mFarmJobs;
};

///////////////////////////////////////////////////////////////////////////////

class FarmNodeGroup : public ork::Object
{
	RttiDeclareConcrete(FarmNodeGroup, ork::Object);
public:
	static void LinkMe() {}
	const orklut<PoolString,FarmNode*>& GetNodes() const { return mFarmNodes; }
private:
	orklut<PoolString,FarmNode*>	mFarmNodes;
};

///////////////////////////////////////////////////////////////////////////////

struct UiFarmNode
{
	FarmNode				mFarmNode;
	int						miNumJobsProcessed;
	ork::tool::ProcessView*	mProcessViewer;
	UiFarmNode() : mProcessViewer(0), miNumJobsProcessed(0) {}
};

///////////////////////////////////////////////////////////////////////////////

struct FarmNodePool
{
	ork::pool<UiFarmNode>	mPool;
	FarmNodePool() : mPool() {}
};

///////////////////////////////////////////////////////////////////////////////

struct BakeContext
{
	LockedResource< bool >				mSlowConnectionLock;
	LockedResource< FarmNodePool >		mLockedFarmNodePool;
	LockedResource< orkset<FarmJob*> >	mJobSet;
	LockedResource< orkset<FarmJob*> >	mActiveJobSet;
	ork::file::Path						mLitPath;
};

///////////////////////////////////////////////////////////////////////////////

class BakeProcessor;

class BakeJobThread : public QThread 
{
	DeclareMoc(BakeJobThread,QThread);
public:
	BakeJobThread(BakeContext*bc,UiFarmNode*fn);
	BakeContext*	GetBakeContext() const { return bkctx; }
	UiFarmNode*		GetFarmNode() const { return pfarmnode; }
	FarmJob*		GetActiveJob() const { return pactivejob; }
	void DoCleanupJob();
	void SigAppendString( QString str );
	const std::string& GetOutputFolder() const { return mOutputFolder; }
	void SetOutputFolder( const std::string& of ) { mOutputFolder=of; }

private:


	BakeContext*	bkctx;
	UiFarmNode*		pfarmnode;
	FarmJob*		pqueuejob;
	FarmJob*		pactivejob;
	QTimer			mIdleTimer;
	BakeProcessor*	mProcessor;
	std::string		mCurrentRgmFile;
	std::string		mOutputFolder;

	void OnTimer();
	void ActivateJob();
	void run(); // virtual
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct BakingGroupMatchItem
{
	PoolString		mMatchName;
	BakingGroup*	mMatchGroup;
	BakingGroupMatchItem() : mMatchGroup(0) {}
};

struct LightingGroupMatchItem
{
	orkset<LightingComponentInst*>	mLights;
	LightingGroupMatchItem() {}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class BakerSettings : public ork::Object
{
	RttiDeclareConcrete(BakerSettings, ork::Object);
public:
	BakerSettings() : mDebugPyg(false), mDebugPygEmbedGeom(false)
	{}

	const ork::file::Path& GetDaeInput() const { return	mDaeInputName; }
	const orklut<PoolString,BakingGroup*>& BakingGroupMap() const { return mBakingGroupMap; }
	const orklut<PoolString,FarmJobSet*>& JobSetMap() const { return mFarmJobSets; }
	bool IsDebugPyg() const { return mDebugPyg; }
	bool IsDebugPygEmbedGeom() const { return mDebugPygEmbedGeom; }
	BakingGroupMatchItem Match( const std::string& TestName ) const;
	LightingGroupMatchItem LightMatch( const std::string& TestName ) const;
	const FarmJobSet* GetCurrentJobSet() const;
	
private:
	
	ork::file::Path						mDaeInputName;
	orklut<PoolString,BakingGroup*>		mBakingGroupMap;
	bool								mDebugPyg;
	bool								mDebugPygEmbedGeom;
	orklut<PoolString,FarmJobSet*>		mFarmJobSets;
	PoolString							mCurrentJobSet;							

};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class LightMapperArchetype : public Archetype
{
	RttiDeclareConcrete(LightMapperArchetype, Archetype);
public:
	LightMapperArchetype() : mCurrentSetting()	{}

	const BakerSettings* GetCurrentSetting() const;
	const FarmNodeGroup* GetCurrentFarmNodeGroup() const;
	const orklut<PoolString,BakerSettings*>& GetBakerSettingsMap() const { return mSettingsMap; }
	const orklut<PoolString,FarmNodeGroup*>& GetFarmNodeGroupMap() const { return mFarmNodeGroups; }

private:
	void DoCompose(ArchComposer& composer){}
	void DoStartEntity(SceneInst*, const CMatrix4& mtx, Entity* pent ) const {}

	orklut<PoolString,BakerSettings*>	mSettingsMap;
	PoolString							mCurrentSetting;							
	orklut<PoolString,FarmNodeGroup*>	mFarmNodeGroups;
	PoolString							mCurrentNodeGroup;							

};

///////////////////////////////////////////////////////////////////////////////

class BakersChoiceDelegate : public ork::IUserChoiceDelegate
{	RttiDeclareConcrete( BakersChoiceDelegate, ork::IUserChoiceDelegate );
public:
	BakersChoiceDelegate() : IUserChoiceDelegate() , mpARCH( 0 ) {} //, mUserData(0) {}
private:
	virtual void EnumerateChoices( orkmap<ork::PoolString,ValueType>& Choices );
	void SetObject( ork::Object* pobj, Object* puserdata ) { mpARCH = ork::rtti::autocast( pobj ); }
	LightMapperArchetype* mpARCH;
};

///////////////////////////////////////////////////////////////////////////////

class FarmGroupChoiceDelegate : public ork::IUserChoiceDelegate
{	RttiDeclareConcrete( FarmGroupChoiceDelegate, ork::IUserChoiceDelegate );
public:
	FarmGroupChoiceDelegate() : IUserChoiceDelegate() , mpARCH( 0 ) {} //, mUserData(0) {}
private:
	virtual void EnumerateChoices( orkmap<ork::PoolString,ValueType>& Choices );
	void SetObject( ork::Object* pobj, Object* puserdata ) { mpARCH = ork::rtti::autocast( pobj );	}
	LightMapperArchetype* mpARCH;
};

///////////////////////////////////////////////////////////////////////////////

class JobSetChoiceDelegate : public ork::IUserChoiceDelegate
{	RttiDeclareConcrete( JobSetChoiceDelegate, ork::IUserChoiceDelegate );
public:
	JobSetChoiceDelegate() : IUserChoiceDelegate() , mpSETTINGS( 0 ) {} 
private:
	virtual void EnumerateChoices( orkmap<ork::PoolString,ValueType>& Choices );
	void SetObject( ork::Object* pobj, Object* puserdata ) { mpSETTINGS = ork::rtti::autocast( pobj );	}
	BakerSettings* mpSETTINGS;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool PerformAtlas(AtlasMapperOps* pOPS,const BakerSettings*settings);
bool PerformBake(BakeOps* pOPS,const BakerSettings*settings,const FarmNodeGroup*pfarmgroup);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}}
///////////////////////////////////////////////////////////////////////////////
#endif
