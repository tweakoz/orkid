////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_GFX_MATERIAL_H_
#define _ORK_GFX_MATERIAL_H_

#include <ork/lev2/gfx/renderable.h>
#include <ork/lev2/gfx/gfxenv_enum.h> // For ETextureDest
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/lev2renderer.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/gfxrasterstate.h>

namespace ork { namespace lev2 {

class Texture;

/////////////////////////////////////////////////////////////////////////

#define MAX_MATERIAL_TEXTURES 4

///////////////////////////////////////////////////////////////////////////////

struct TextureContext
{
	TextureContext( const Texture* ptex=0, float repU=1.0f, float repV=1.0f );

	const Texture*	mpTexture;
	float		mfRepeatU;
	float		mfRepeatV;

};

class MaterialInstApplicator : public ork::Object
{
	RttiDeclareAbstract(MaterialInstApplicator,ork::Object);
public:
	virtual void ApplyToTarget(GfxTarget *pTARG) = 0;
};

class MaterialInstItem : public ork::Object
{
	RttiDeclareAbstract(MaterialInstItem,ork::Object);

public:
	PoolString					mObjectName;
	PoolString					mChannelName;
	const GfxMaterial*			mMaterial;
	MaterialInstApplicator*	mApplicator;

	MaterialInstItem() : mMaterial(0), mApplicator(0) {}

	virtual void Set() = 0;
	void SetApplicator( MaterialInstApplicator* papplicator ) { mApplicator=papplicator; }
};

///////////////////////////////////////////////////////////////////////////////

class MaterialInstItemMatrix : public MaterialInstItem
{
	RttiDeclareAbstract(MaterialInstItemMatrix,MaterialInstItem);

	CMatrix4			mMatrix;

public:

	MaterialInstItemMatrix() {}

	void				SetMatrix( const CMatrix4& pmat ) { mMatrix=pmat; }
	const CMatrix4&		GetMatrix() const { return mMatrix; }

	virtual void Set() {}

};

///////////////////////////////////////////////////////////////////////////////

class MaterialInstItemMatrixBlock : public MaterialInstItem
{
	RttiDeclareAbstract(MaterialInstItemMatrixBlock,MaterialInstItem);

	size_t				miNumMatrices;
	const CMatrix4*		mpMatrices;

public:

	MaterialInstItemMatrixBlock() : miNumMatrices(0), mpMatrices(0) {}

	void				SetNumMatrices(size_t i) { miNumMatrices=i; }
	void				SetMatrixBlock( const CMatrix4* pmat ) { mpMatrices=pmat; }

	size_t				GetNumMatrices() const { return miNumMatrices; }
	const CMatrix4*		GetMatrices() const { return mpMatrices; }

	virtual void Set() {}

};

///////////////////////////////////////////////////////////////////////////////

class GfxMaterial : public ork::Object
{
	RttiDeclareAbstract(GfxMaterial,ork::Object);
	//////////////////////////////////////////////////////////////////////////////

	public:
	f32 mfParticleSize; //todo this does not belong here

	GfxMaterial();
	virtual ~GfxMaterial();

	virtual int GetNumPasses( void ) { return int( miNumPasses ); }

	virtual void Update( void ) = 0;

	virtual void Init( GfxTarget *pTarg ) = 0;

	virtual bool BeginPass( GfxTarget* pTARG, int iPass=0 ) = 0;
	virtual void EndPass( GfxTarget* pTARG ) = 0;
	virtual int  BeginBlock( GfxTarget* pTARG, const RenderContextInstData &MatCtx = RenderContextInstData::Default ) = 0;
	virtual void EndBlock( GfxTarget* pTARG ) = 0;

	void SetTexture( ETextureDest edest, const TextureContext & htex );
	const TextureContext & GetTexture( ETextureDest edest ) const;
	TextureContext & GetTexture( ETextureDest edest );

	void SetName( const PoolString & nam ) { mMaterialName = nam; }
	const PoolString & GetName( void ) const { return mMaterialName; }

	void SetFogStart( F32 fstart ) { mfFogStart=CReal(fstart); };
	void SetFogRange( F32 frange ) { mfFogRange=CReal(frange); };

	virtual void UpdateMVPMatrix( GfxTarget *pTARG ) {}

	virtual void BindMaterialInstItem( MaterialInstItem* pitem ) const {}
	virtual void UnBindMaterialInstItem( MaterialInstItem* pitem ) const {}

	const RenderQueueSortingData& GetRenderQueueSortingData() const { return mSortingData; }
	RenderQueueSortingData& GetRenderQueueSortingData() { return mSortingData; }
	
	virtual void SetMaterialProperty( const char* prop, const char* val ) {}

	void PushDebug(bool bdbg);
	void PopDebug();
	bool IsDebug();

	//////////////////////////////////////////////////////////////////////////////

	SRasterState							mRasterState;

	protected:

	int										miNumPasses;		///< Number Of Render Passes in this Material (platform specific)
	PoolString								mMaterialName;
	TextureContext							mTextureMap[ETEXDEST_END];
	CReal mfFogStart;
	CReal mfFogRange;
	RenderQueueSortingData					mSortingData;
	const RenderContextInstData*			mRenderContexInstData;
	std::stack<bool>						mDebug;
};

///////////////////////////////////////////////////////////////////////////////

typedef orkmap<std::string,GfxMaterial*> MaterialMap;
bool LoadMaterialMap( const ork::file::Path& pth, MaterialMap& mmap );

} }

#endif
