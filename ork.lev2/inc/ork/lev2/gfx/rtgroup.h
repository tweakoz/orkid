#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/any.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

class Texture;
class GfxTarget;
class GfxMaterial;
class RtGroup;
class RtBuffer;

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// RtGroup (Multiple Render Target Group)
/// collection of buffers that can be rendered to in parallel
/// on Geforce 6800 and lower, blend modes are common to active on all MRT sub buffers
/// on 7xxx and higher this restriction is removed
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class RtBuffer //: public GfxBuffer
{
	public:

	RtBuffer(	RtGroup* pgroup,
				ETargetType etype,
				EBufferFormat efmt,
				int iW, int iH );

	Texture* GetTexture() const { return mTexture; }
	GfxMaterial* GetMaterial() const { return mMaterial; }
	EBufferFormat format() const { return mFormat; }

	void SetSizeDirty( bool sd ) { mSizeDirty=sd; }
	void SetTexture( Texture* ptex ) { mTexture=ptex; }
	void SetMaterial( GfxMaterial* pmtl ) { mMaterial=pmtl; }

	RtGroup* mParentGroup;
	int miW, miH;
	Texture* mTexture;
	GfxMaterial* mMaterial;
	ETargetType   mType;
	EBufferFormat mFormat;
	svarp_t mPlatformHandle;
	bool mSizeDirty;
	bool mComputeMips;
  std::string _debugName;
};

class RtGroup
{
public:
	/////////////////////////////////////////
	RtGroup( GfxTarget* partarg,
			 int iW, int iH, int iSamples=1 );

	~RtGroup();
	/////////////////////////////////////////
	RtBuffer* GetMrt( int idx ) const
	{
		OrkAssert( (idx>=0) && (idx<kmaxmrts) );
		return mMrt[ idx ];
	}
	/////////////////////////////////////////
	void	SetMrt( int idx, RtBuffer* buffer );
	int		GetNumTargets( void ) const { return mNumMrts; }
	void	SetInternalHandle( void*h ) { mInternalHandle=h; }
	void*	GetInternalHandle( void ) const { return mInternalHandle; }
	void	Resize( int iw, int ih );
	void	SetSizeDirty( bool bv ) { mbSizeDirty=bv; }
	bool	IsSizeDirty() const { return mbSizeDirty; }
	GfxTarget* ParentTarget() const { return mParentTarget; }
	/////////////////////////////////////////
	int		GetW() const { return miW; }
	int		GetH() const { return miH; }
	int		GetSamples() const { return miSamples; }
	/////////////////////////////////////////
	static const int	kmaxmrts = 4;

	GfxTarget* 			mParentTarget;
	RtBuffer*			mMrt[kmaxmrts];
	GfxBuffer*			mDepth;
  Texture* _depthTexture = nullptr;
	int					mNumMrts;
	int					miW;
	int					miH;
	int					miSamples;
	bool				mbSizeDirty;
	void*				mInternalHandle;
};

///////////////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
