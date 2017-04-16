////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/kernel/core/singleton.h>
#include <ork/util/md5.h>
#include <ork/math/cvector4.h>
#include <ork/file/path.h>
#include <ork/kernel/kernel.h>

namespace ork { namespace lev2 {

class GfxTarget;

///////////////////////////////////////////////////////////////////////////////

class CTextureMipData
{
	public:

	void*	mpImageData;
	void*	mpClutData;

	int		miWidth;
	int		miHeight;
	int		miImageBytesPerPixel;
	int		miClutBytesPerPixel;
	int		miClutSize;
	int		miImageDataSize;
	int		miUniqueColors;

	CTextureMipData()
		: mpImageData( 0 )
		, mpClutData( 0 )
		, miClutSize( 0 )
		, miWidth( 0 )
		, miHeight( 0 )
		, miImageBytesPerPixel( 0 )
		, miClutBytesPerPixel( 0 )
		, miImageDataSize( 0 )
		, miUniqueColors( 0 )
	{

	}
};

//////////////////////////////////////////////////////////////////////////

struct TextureSamplingModeData
{
	TextureSamplingModeData()
		: mTexAddrModeU(ETEXADDR_WRAP)
		, mTexAddrModeV(ETEXADDR_WRAP)
		, mTexFiltModeMin(ETEXFILT_POINT)
		, mTexFiltModeMag(ETEXFILT_POINT)
		, mTexFiltModeMip(ETEXFILT_POINT)
	{}

	ETextureAddressMode GetAddrModeU() const { return mTexAddrModeU; }
	ETextureAddressMode GetAddrModeV() const { return mTexAddrModeV; }
	ETextureFilterMode	GetFiltModeMin() const { return mTexFiltModeMin; }
	ETextureFilterMode	GetFiltModeMag() const { return mTexFiltModeMag; }
	ETextureFilterMode	GetFiltModeMip() const { return mTexFiltModeMip; }

	ETextureAddressMode mTexAddrModeU;
	ETextureAddressMode mTexAddrModeV;
	ETextureFilterMode	mTexFiltModeMin;
	ETextureFilterMode	mTexFiltModeMag;
	ETextureFilterMode	mTexFiltModeMip;

	void PresetPointAndClamp();
	void PresetTrilinearWrap();

};

//////////////////////////////////////////////////////////////////////////

class TextureAnimationInst;
class Texture;
class TextureInterface;
    

class TextureAnimationBase
{
public:

	virtual void UpdateTexture( TextureInterface* txi, Texture* ptex, TextureAnimationInst* ptexanim ) = 0;
	virtual float GetLengthOfTime( void ) const = 0;
	virtual ~TextureAnimationBase() {}
	
private:

};

//////////////////////////////////////////////////////////////////////////

class TextureAnimationInst
{
public:
	TextureAnimationInst(TextureAnimationBase*panim=0) : mfCurrentTime(0.0f), mpAnim(panim) {}
	float GetCurrentTime() const { return mfCurrentTime; }
	void SetCurrentTime( float fv ) { mfCurrentTime=fv; }
	TextureAnimationBase* GetAnim() const { return mpAnim; }
private:
	float mfCurrentTime;
	TextureAnimationBase* mpAnim;
};

//////////////////////////////////////////////////////////////////////////

class Texture
{
public:

	enum ETexClass
	{
		ETEXCLASS_STATIC = 0,		// standard static texture
		ETEXCLASS_RENDERTARGET ,	// rendered texture
		ETEXCLASS_PAINTABLE ,		// paintable texture (like a rendertarget, but doesnt clear every frame)
		ETEXCLASS_DATA ,			// RAW DATA (non renderable)
		ETEXCLASS_END,
	};

	//////////////////////////////////////////////////////

	int	GetWidth( void ) const { return miWidth; }
	int	GetHeight( void ) const { return miHeight; }
	int	GetDepth( void ) const { return miDepth; }
	int	GetBytesPerPixel( void ) const { return miBPP; }
	U32 GetFlags( void ) const { return mFlags; }

	bool IsVolumeTexture( void ) const { return (miDepth>1); }
	bool IsDirty( void ) const { return mbDirty; }

	void* GetTexIH( void ) const { return mInternalHandle; }

	ETexClass GetTexClass( void ) const { return meTexClass; }
	ETextureType GetTexType( void ) const { return meTexType; }
	ETextureDest GetTexDest( void ) const { return meTexDest; }

	void* GetTexData( void ) const { return mpData; }

	void SetWidth( int iw ) { miWidth=iw; }
	void SetHeight( int ih ) { miHeight=ih; }
	void SetDepth( int id ) { miDepth=id; }
	void SetBytesPerPixel( int ib ) { miBPP=ib; }
	void SetTexClass( ETexClass eclass ) { meTexClass=eclass; }
	void SetTexType( ETextureType etype ) { meTexType=etype; }
	void SetTexDest( ETextureDest edest ) { meTexDest=edest; }
	void SetTexIH( void* hIH ) const { mInternalHandle=hIH; }
	void SetDirty( bool bv ) const { mbDirty=bv; }
	void SetFlags( u32 uf ) { mFlags = uf; }
	void SetTexData( void *pd ) { mpData=reinterpret_cast<u8*>( pd ); }

	//////////////////////////////////////////////////////

	void Clear( const CColor4 & color );
	void SetTexel( const CColor4 & color, const CVector2 & ST );

	Md5Sum GetMd5Sum( void ) const { return mMd5Sum; }
	void SetMd5Sum( Md5Sum sum ) { mMd5Sum=sum; }

    const TextureSamplingModeData& TexSamplingMode() const { return mTexSampleMode; }
    TextureSamplingModeData& TexSamplingMode() { return mTexSampleMode; }

	//////////////////////////////////////////////////////

	void AddMip( const CTextureMipData & Mip ) { mMipImages.push_back( Mip ); }
	const CTextureMipData & GetMip( int imip ) const { return mMipImages[ imip ]; }

	int GetNumMipMaps( void ) const { return miNumMipMaps; }
	void SetNumMipMaps( int imips ) { miNumMipMaps=imips; }

	int GetMipMaxUniqueColors( void ) const { return miMaxMipUniqueColors; }
	int GetMipTotalUniqueColors( void ) const { return miTotalUniqueColors; }

	void SetMipMaxUniqueColors( int ival ) { miMaxMipUniqueColors=ival; }
	void SetMipTotalUniqueColors( int ival ) { miTotalUniqueColors=ival; }

	//////////////////////////////////////////////////////

	int mMaxMip;

	Texture()
		: meTexDest(ETEXDEST_END)
		, meTexType(ETEXTYPE_END)
		, meTexClass(ETEXCLASS_END)
		, meTexFormat(EBUFFMT_END)
		, miWidth(0)
		, miHeight(0)
		, miDepth(0)
		, muvW(0)
		, muvH(0)
		, miBPP(0)
		, mpImageData(0)
		, mFlags(0)
		, mInternalHandle(0)
		, mbDirty(true)
		, miMaxMipUniqueColors( 0 )
		, miTotalUniqueColors( 0 )
		, mpData(0)
		, mpTexAnim(0)
		, mMaxMip(0)
	{}

	~Texture();

	static Texture *LoadUnManaged( const AssetPath& fname );
	static Texture *CreateBlank( int iw, int ih, EBufferFormat efmt );

	//////////////////////////////////////////////////////////

	void SetProperty( const std::string & texname, const std::string &value ); // { mTextureProperties.AddSorted(texname,value); }

	const std::string & GetProperty( const std::string & texname ) const
	{
		orkmap<std::string,std::string>::const_iterator it=mTextureProperties.find( texname );
		return (it==mTextureProperties.end()) ? gstring_noval : (*it).second;
	}

	//////////////////////////////////////////////////////////

	TextureAnimationBase* GetTexAnim() const { return mpTexAnim; }
	void SetTexAnim( TextureAnimationBase* ptexanim ) { mpTexAnim=ptexanim; }
	//////////////////////////////////////////////////////////

	void* GetImageData() const { return mpImageData; }
	void SetImageData( void* pid ) { mpImageData = pid; }

	//////////////////////////////////////////////////////////

	static void RegisterLoaders( void );

	private:

	Md5Sum							mMd5Sum;	// for dirty checking (mipgen/palettegen)
	orkvector< CTextureMipData >	mMipImages;
	int								miTotalUniqueColors;
	int								miMaxMipUniqueColors;

	ETextureDest					meTexDest;
	ETextureType					meTexType;
	ETexClass						meTexClass;
	EBufferFormat					meTexFormat;

	int								miWidth, miHeight, miDepth;
	int								miNumMipMaps;
	int								muvW, muvH;

	U32								miBPP;
	void*							mpImageData;

	U32								mFlags;

	mutable void*					mInternalHandle;

	mutable bool					mbDirty;
	void*							mpData;
	TextureAnimationBase*			mpTexAnim;

	orkmap<std::string,std::string>	mTextureProperties;

    TextureSamplingModeData			mTexSampleMode;

};

///////////////////////////////////////////////////////////////////////////////

} }
