////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/lev2/gfx/builtin_frameeffects.h>
#include <ork/dataflow/dataflow.h>

namespace ork { namespace ent {

class CompositingComponentInst;
class CompositingGroup;
class CompositingSceneItem;
struct CMCIdrawdata;
struct CompositingContext;

///////////////////////////////////////////////////////////////////////////////

enum EOutputTimeStep
{
	EOutputTimeStep_RealTime = 0,
	EOutputTimeStep_15fps,
	EOutputTimeStep_24fps,
	EOutputTimeStep_30fps,
	EOutputTimeStep_48fps,
	EOutputTimeStep_60fps,
	EOutputTimeStep_72fps,
	EOutputTimeStep_96fps,
	EOutputTimeStep_120fps,
	EOutputTimeStep_240fps,
};

enum EOutputRes
{
	EOutputRes_640x480 = 0,
	EOutputRes_960x640,
	EOutputRes_1024x1024,
	EOutputRes_1280x720,
	EOutputRes_1600x1200,
	EOutputRes_1920x1080,
};

enum EOutputResMult
{
	EOutputResMult_Quarter = 0,
	EOutputResMult_Half,
	EOutputResMult_Full,
	EOutputResMult_Double,
	EOutputResMult_Quadruple,
};

///////////////////////////////////////////////////////////////////////////////

enum ECOMPOSITEBlend
{
	BoverAplusC = 0,
	AplusBplusC,
	AlerpBwithC,
	Asolo,
	Bsolo,
	Csolo,
};

///////////////////////////////////////////////////////////////////////////////

class CompositingMaterial : public lev2::GfxMaterial
{
public:

	CompositingMaterial();
	~CompositingMaterial();
	/////////////////////////////////////////////////
	virtual void Update( void ) {}
	virtual void Init( lev2::GfxTarget *pTarg );
	virtual bool BeginPass( lev2::GfxTarget* pTARG, int iPass=0 );
	virtual void EndPass( lev2::GfxTarget* pTARG );
	virtual int BeginBlock( lev2::GfxTarget* pTARG, const lev2::RenderContextInstData &MatCtx );
	virtual void EndBlock( lev2::GfxTarget* pTARG );
	/////////////////////////////////////////////////
	void SetTextureA( lev2::Texture* ptex ) { mCurrentTextureA=ptex; }
	void SetTextureB( lev2::Texture* ptex ) { mCurrentTextureB=ptex; }
	void SetTextureC( lev2::Texture* ptex ) { mCurrentTextureC=ptex; }
	void SetLevelA( const CVector4& la ) { mLevelA=la; }
	void SetLevelB( const CVector4& lb ) { mLevelB=lb; }
	void SetLevelC( const CVector4& lc ) { mLevelC=lc; }
	void SetBiasA( const CVector4& ba ) { mBiasA=ba; }
	void SetBiasB( const CVector4& bb ) { mBiasB=bb; }
	void SetBiasC( const CVector4& bc ) { mBiasC=bc; }
	void SetTechnique( const std::string& tek );
	/////////////////////////////////////////////////
	lev2::Texture* mCurrentTextureA;
	lev2::Texture* mCurrentTextureB;
	lev2::Texture* mCurrentTextureC;
	CVector4 mLevelA;
	CVector4 mLevelB;
	CVector4 mLevelC;
	CVector4 mBiasA;
	CVector4 mBiasB;
	CVector4 mBiasC;

	const lev2::FxShaderTechnique*	hTekOp2AmulB;
	const lev2::FxShaderTechnique*	hTekOp2AdivB;

	const lev2::FxShaderTechnique*	hTekBoverAplusC;
	const lev2::FxShaderTechnique*	hTekAplusBplusC;
	const lev2::FxShaderTechnique*	hTekAlerpBwithC;
	const lev2::FxShaderTechnique*	hTekAsolo;
	const lev2::FxShaderTechnique*	hTekBsolo;
	const lev2::FxShaderTechnique*	hTekCsolo;

	const lev2::FxShaderTechnique*	hTekCurrent;
	
	const lev2::FxShaderParam*		hMapA;
	const lev2::FxShaderParam*		hMapB;
	const lev2::FxShaderParam*		hLevelA;
	const lev2::FxShaderParam*		hLevelB;
	const lev2::FxShaderParam*		hLevelC;
	const lev2::FxShaderParam*		hBiasA;
	const lev2::FxShaderParam*		hBiasB;
	const lev2::FxShaderParam*		hBiasC;
	const lev2::FxShaderParam*		hMapC;
	const lev2::FxShaderParam*		hMatMVP;
	lev2::FxShader*					hModFX;

};

///////////////////////////////////////////////////////////////////////////////

class CompositingTechnique : public ork::Object
{
	RttiDeclareAbstract(CompositingTechnique, ork::Object);
public:
	virtual void Init( lev2::GfxTarget* pTARG, int w, int h ) = 0;
	virtual void Draw(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI) = 0;
	virtual void CompositeToScreen( ork::lev2::GfxTarget* pT, CompositingComponentInst* pCCI, CompositingContext& cctx ) = 0;
};

///////////////////////////////////////////////////////////////////////////////

class Fx3CompositingTechnique : public CompositingTechnique
{
	RttiDeclareConcrete(Fx3CompositingTechnique, CompositingTechnique );

public:
	Fx3CompositingTechnique();
	~Fx3CompositingTechnique();

	void CompositeLayerToScreen(	lev2::GfxTarget* pT,
									CompositingContext& cctx,
									ECOMPOSITEBlend eblend,
									lev2::RtGroup* psrcgroupA,
									lev2::RtGroup* psrcgroupB,
									lev2::RtGroup* psrcgroupC,
									float levA, float levB, float levC );

	const PoolString&	GetGroupA() const { return mGroupA; }
	const PoolString&	GetGroupB() const { return mGroupB; }
	const PoolString&	GetGroupC() const { return mGroupC; }

	float GetLevelA() const { return mfLevelA; }
	float GetLevelB() const { return mfLevelB; }
	float GetLevelC() const { return mfLevelC; }

	ECOMPOSITEBlend GetBlendMode() const { return meBlendMode; }


	lev2::BuiltinFrameTechniques*	mpBuiltinFrameTekA;
	lev2::BuiltinFrameTechniques*	mpBuiltinFrameTekB;
	lev2::BuiltinFrameTechniques*	mpBuiltinFrameTekC;
	ECOMPOSITEBlend					meBlendMode;
	PoolString						mGroupA;
	PoolString						mGroupB;
	PoolString						mGroupC;
	float							mfLevelA;
	float							mfLevelB;
	float							mfLevelC;
	CompositingMaterial				mCompositingMaterial;
	
private:

	void Init( lev2::GfxTarget* pTARG, int w, int h ); // virtual
	void Draw(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI); // virtual
	void CompositeToScreen( ork::lev2::GfxTarget* pT, CompositingComponentInst* pCCI, CompositingContext& cctx ); // virtual
};

///////////////////////////////////////////////////////////////////////////////
class CompositingNode : public ork::Object
{
	RttiDeclareAbstract(CompositingNode, ork::Object);
public:
	CompositingNode();
	~CompositingNode();
	void Init( lev2::GfxTarget* pTARG, int w, int h );
	void Render(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI);
	virtual lev2::RtGroup* GetOutput() const { return nullptr; }

private:
	virtual void DoInit( lev2::GfxTarget* pTARG, int w, int h ) = 0;
	virtual void DoRender(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI) = 0;

};
///////////////////////////////////////////////////////////////////////////////
class CompositingBuffer : public ork::Object
{
	int							miWidth;
	int							miHeight;
	ork::lev2::EBufferFormat	meBufferFormat;
	
	CompositingBuffer();
	~CompositingBuffer();
};
///////////////////////////////////////////////////////////////////////////////
class PassThroughCompositingNode : public CompositingNode
{
	RttiDeclareConcrete(PassThroughCompositingNode, CompositingNode);
public:
	PassThroughCompositingNode();
	~PassThroughCompositingNode();
private:
	void DoInit( lev2::GfxTarget* pTARG, int w, int h ); // virtual
	void DoRender(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI); // virtual

	void GetGroup(ork::rtti::ICastable*& val) const;
	void SetGroup( ork::rtti::ICastable* const & val);
	lev2::RtGroup* GetOutput() const override;

	CompositingMaterial				mCompositingMaterial;
	CompositingGroup*				mGroup;
	lev2::BuiltinFrameTechniques*	mFTEK;
};
///////////////////////////////////////////////////////////////////////////////
class SeriesCompositingNode : public CompositingNode
{
	RttiDeclareConcrete(SeriesCompositingNode, CompositingNode);
public:
	SeriesCompositingNode();
	~SeriesCompositingNode();
private:
	void DoInit( lev2::GfxTarget* pTARG, int w, int h ); // virtual
	void DoRender(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI); // virtual

	void GetNode(ork::rtti::ICastable*& val) const;
	void SetNode( ork::rtti::ICastable* const & val);

	lev2::RtGroup* GetOutput() const override;

	CompositingMaterial				mCompositingMaterial;
	CompositingNode*				mNode;
	lev2::RtGroup*					mOutput;
	lev2::BuiltinFrameTechniques*	mFTEK;
};
///////////////////////////////////////////////////////////////////////////////
enum EOp2CompositeMode
{
	Op2AsumB = 0,
	Op2AmulB,
	Op2AdivB,
	Op2BoverA,
	Op2AoverB,
	Op2Asolo,
	Op2Bsolo,
};
class Op2CompositingNode : public CompositingNode
{
	RttiDeclareConcrete(Op2CompositingNode, CompositingNode);
public:
	Op2CompositingNode();
	~Op2CompositingNode();
private:
	void DoInit( lev2::GfxTarget* pTARG, int w, int h ); // virtual
	void DoRender(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI); // virtual
	void GetNodeA(ork::rtti::ICastable*& val) const;
	void SetNodeA( ork::rtti::ICastable* const & val);
	void GetNodeB(ork::rtti::ICastable*& val) const;
	void SetNodeB( ork::rtti::ICastable* const & val);
	lev2::RtGroup* GetOutput() const override { return mOutput; }

	CompositingNode* mSubA;
	CompositingNode* mSubB;
	CompositingMaterial				mCompositingMaterial;
	lev2::RtGroup*					mOutput;
	EOp2CompositeMode				mMode;
	CVector4						mLevelA;
	CVector4						mLevelB;
	CVector4						mBiasA;
	CVector4						mBiasB;
};
///////////////////////////////////////////////////////////////////////////////
class NodeCompositingTechnique : public CompositingTechnique
{
	RttiDeclareConcrete(NodeCompositingTechnique, CompositingTechnique );

public:
	NodeCompositingTechnique();
	~NodeCompositingTechnique();

private:

	void Init( lev2::GfxTarget* pTARG, int w, int h ); // virtual
	void Draw(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI); // virtual
	void CompositeToScreen( ork::lev2::GfxTarget* pT, CompositingComponentInst* pCCI, CompositingContext& cctx ); // virtual
	//
	void GetRoot(ork::rtti::ICastable*& val) const;
	void SetRoot( ork::rtti::ICastable* const & val);

	ork::ObjectMap			mBufferMap;
	CompositingNode*		mpRootNode;
	CompositingMaterial		mCompositingMaterial;

	
};
///////////////////////////////////////////////////////////////////////////////

struct CompositingContext
{
	int									miWidth;
	int									miHeight;
	lev2::GfxMaterial3DSolid			mUtilMaterial;
	CompositingTechnique*				mCTEK;
	
	CompositingContext();
	~CompositingContext();
	void Init( lev2::GfxTarget* pTARG );
	void Draw( lev2::GfxTarget* pTARG, CMCIdrawdata& drawdata, CompositingComponentInst* pCCI );
	void CompositeToScreen( ork::lev2::GfxTarget* pT, CompositingComponentInst* pCCI );
	void Resize( int iW, int iH );
	void SetTechnique(CompositingTechnique*ptek) { mCTEK=ptek; }
};

///////////////////////////////////////////////////////////////////////////////

class CompositingManagerComponentData : public ork::ent::SceneComponentData
{
	RttiDeclareConcrete(CompositingManagerComponentData, ork::ent::SceneComponentData);
public:
	///////////////////////////////////////////////////////
	CompositingManagerComponentData();
	ork::ent::SceneComponentInst* CreateComponentInst(ork::ent::SceneInst *pinst) const; // virtual 
	///////////////////////////////////////////////////////
	CompositingContext& GetCompositingContext() const { return  mContext; }
private:
	mutable CompositingContext mContext;
};

///////////////////////////////////////////////////////////////////////////

struct CompositingPassData
{
	const CompositingGroup*			mpGroup;
	lev2::BuiltinFrameTechniques*	mpFrameTek;
	bool							mbDrawSource;
	const PoolString*				mpCameraName;
	const PoolString*				mpLayerName;
	
	CompositingPassData() 
		: mpGroup(0)
		, mpFrameTek(0)
		, mbDrawSource(true)
		, mpCameraName(0)
		, mpLayerName(0)
	{
	}
};

///////////////////////////////////////////////////////////////////////////

struct CMCIdrawdata
{
	lev2::FrameRenderer&		mFrameRenderer;
	orkstack<CompositingPassData>	mCompositingGroupStack;
		
	CMCIdrawdata( lev2::FrameRenderer& renderer ) : mFrameRenderer(renderer) {}
};

///////////////////////////////////////////////////////////////////////////

class CompositingManagerComponentInst : public ork::ent::SceneComponentInst
{
	RttiDeclareAbstract(CompositingManagerComponentInst, ork::ent::ComponentInst);
public:
	CompositingManagerComponentInst( const CompositingManagerComponentData &data, ork::ent::SceneInst *pinst );

	CompositingComponentInst* GetCompositingComponentInst( int icidx ) const;
	
	void Draw(CMCIdrawdata& drawdata);
	void ComposeToScreen( lev2::GfxTarget* pT );
	
	const CompositingManagerComponentData& GetCMCD() const { return	mCMCD; }
	
	void AddCCI( CompositingComponentInst* cci );

	bool IsEnabled() const;

	EOutputTimeStep GetCurrentFrameRateEnum() const;
	float GetCurrentFrameRate() const;

private:

	orkvector<CompositingComponentInst*>	mCCIs;
	const CompositingManagerComponentData&	mCMCD;
	//DrawableBufferLock						mDbLock;
};

///////////////////////////////////////////////////////////////////////////////

class CompositingGroupEffect : public ork::Object
{
	RttiDeclareConcrete(CompositingGroupEffect, ork::Object);

public:
	///////////////////////////////////////////////////////
	CompositingGroupEffect();

	lev2::EFrameEffect GetFrameEffect() const { return meFrameEffect; }
	float GetEffectAmount() const { return mfEffectAmount; }
	float GetFeedbackAmount() const { return mfFeedbackAmount; }
	float GetFinalRezScale() const { return mfFinalResMult; }
	float GetFxRezScale() const { return mfFxResMult; }
	const char* GetEffectName() const;
    ork::lev2::Texture*	GetFbUvMap() const;
	bool IsPostFxFeedback() const { return mbPostFxFeedback; }

private:

    void SetTextureAccessor( ork::rtti::ICastable* const & tex);
    void GetTextureAccessor( ork::rtti::ICastable* & tex) const;

    lev2::EFrameEffect	        meFrameEffect;
    ork::lev2::TextureAsset*	mTexture;
	float	        		    mfEffectAmount;
	float			            mfFeedbackAmount;
	float						mfFxResMult;
	float						mfFinalResMult;
	bool						mbPostFxFeedback;

};

///////////////////////////////////////////////////////////////////////////////

class CompositingGroup : public ork::Object
{
	RttiDeclareConcrete(CompositingGroup, ork::Object);

public:
	CompositingGroup();

	const PoolString&				GetCameraName() const { return mCameraName; }
	const PoolString&				GetLayers() const { return mLayers; }
	const CompositingGroupEffect&	GetEffect() const { return mEffect; }

private:

	PoolString				mCameraName;
	PoolString				mLayers;
	CompositingGroupEffect	mEffect;

	ork::Object* EffectAccessor() { return & mEffect; }
};

///////////////////////////////////////////////////////////////////////////////

class CompositingSceneItem : public ork::Object
{
	RttiDeclareConcrete(CompositingSceneItem, ork::Object);

public:
	CompositingSceneItem();

	CompositingTechnique* GetTechnique() const { return mpTechnique; }

private:

	void GetTech(ork::rtti::ICastable*& val) const;
	void SetTech( ork::rtti::ICastable* const & val);

	CompositingTechnique*	mpTechnique;

};

///////////////////////////////////////////////////////////////////////////////

class CompositingScene : public ork::Object
{
	RttiDeclareConcrete(CompositingScene, ork::Object);

public:
	CompositingScene();

	const orklut<PoolString,ork::Object*>& GetItems() const { return mItems; }
	
private:

	orklut<PoolString,ork::Object*> mItems;

};

///////////////////////////////////////////////////////////////////////////////

class CompositingComponentData : public ork::ent::ComponentData
{
	RttiDeclareConcrete(CompositingComponentData, ork::ent::ComponentData);

public:
	///////////////////////////////////////////////////////
	CompositingComponentData();
	virtual ork::ent::ComponentInst *CreateComponent(ork::ent::Entity *pent) const;
	///////////////////////////////////////////////////////
	void DoRegisterWithScene( ork::ent::SceneComposer& sc );

	const orklut<PoolString,ork::Object*>& GetGroups() const { return mCompositingGroups; }
	const orklut<PoolString,ork::Object*>& GetScenes() const { return mScenes; }

	PoolString& GetActiveScene() const { return mActiveScene; }
	PoolString& GetActiveItem() const { return mActiveItem; }

	bool IsEnabled() const { return mbEnable&&mToggle; }
	bool IsOutputFramesEnabled() const { return mbOutputFrames; }
	EOutputTimeStep OutputFrameRate() const { return mOutputFrameRate; }

	void Toggle() const { mToggle=!mToggle; }

private:

	orklut<PoolString,ork::Object*> mCompositingGroups;
	orklut<PoolString,ork::Object*> mScenes;
	mutable PoolString mActiveScene;
	mutable PoolString mActiveItem;
	mutable bool mToggle;
	bool mbEnable;
	bool mbOutputFrames;
	EOutputRes mOutputBaseResolution;
	EOutputResMult mOutputResMult;
	EOutputTimeStep mOutputFrameRate;

	const char* GetShortSelector() const { return "com"; } // virtual

};

struct CompositingMorphable : public dataflow::morphable
{
	void WriteMorphTarget( dataflow::MorphKey name, float flerpval ); // virtual
	void RecallMorphTarget( dataflow::MorphKey name ); // virtual 
	void Morph1D( const dataflow::morph_event* pevent ); // virtual
};

///////////////////////////////////////////////////////////////////////////////

class CompositingComponentInst : public ork::ent::ComponentInst
{
	RttiDeclareAbstract(CompositingComponentInst, ork::ent::ComponentInst);

public:

	CompositingComponentInst( const CompositingComponentData &data, ork::ent::Entity *pent );
	~CompositingComponentInst();
	
	const CompositingComponentData& GetCompositingData() const { return mCompositingData; }

	const CompositingContext& GetCCtx() const;
	CompositingContext& GetCCtx();

	const CompositingSceneItem* GetCompositingItem(int isceneidx,int itemidx) const;
	
	const CompositingGroup* GetGroup(const PoolString& grpname) const;

//	const CompositingGroup* GetActiveGroupA() const;
//	const CompositingGroup* GetActiveGroupB() const;
//	const CompositingGroup* GetActiveGroupC() const;
	
private:
	
	const CompositingComponentData& mCompositingData;
	bool DoLink(ork::ent::SceneInst *psi);
	void DoUnLink(SceneInst *psi);
	void DoUpdate(SceneInst *inst);

	float	mfTimeAccum;
	float	mfLastTime;
	
	CompositingManagerComponentInst*	mpCMCI;
	CompositingMorphable				mMorphable;

	int miActiveSceneItem;
	bool DoNotify(const ork::event::Event *event);
	
};

///////////////////////////////////////////////////////////////////////////////

class CompositorArchetype : public Archetype
{
	RttiDeclareConcrete(CompositorArchetype, Archetype);
public:
	CompositorArchetype();
private:
	void DoCompose(ArchComposer& composer); // virtual
	void DoStartEntity(SceneInst*, const CMatrix4& mtx, Entity* pent ) const {}
	//void DoRegisterWithScene( ork::ent::SceneComposer& sc );
};

///////////////////////////////////////////////////////////////////////////////
}} // namespace { ork { namespace ent {
