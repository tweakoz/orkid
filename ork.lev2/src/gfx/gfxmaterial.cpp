////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/camera/cameraman.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/application/application.h>

namespace ork {
static const std::string TexDestStrings[lev2::ETEXDEST_END+2] = 
{
	"ETEXDEST_AMBIENT",
	"ETEXDEST_DIFFUSE",
	"ETEXDEST_SPECULAR",
	"ETEXDEST_BUMP",
	"ETEXDEST_END",
	""
};
template<> const EPropType PropType<lev2::ETextureDest>::meType = EPROPTYPE_ENUM;
template<> const char * PropType<lev2::ETextureDest>::mstrTypeName = "GfxEnv::ETextureDest";
template<> lev2::ETextureDest PropType<lev2::ETextureDest>::FromString(const PropTypeString& String)
{
	return PropType::FindValFromStrings<lev2::ETextureDest>( String.c_str(), TexDestStrings, lev2::ETEXDEST_END );
}
template<> void PropType<lev2::ETextureDest>::ToString( const lev2::ETextureDest & e, PropTypeString& tstr )
{
	tstr.set( TexDestStrings[ int(e) ].c_str() );
}
template<> void PropType<lev2::ETextureDest>::GetValueSet( const std::string * & ValueStrings, int & NumStrings )
{	
	NumStrings = lev2::ETEXDEST_END+1;
	ValueStrings = TexDestStrings;
}
}

/////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterial, "GfxMaterial" )
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::MaterialInstApplicator, "MaterialInstApplicator" )
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::MaterialInstItem, "MaterialInstItem" )
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::MaterialInstItemMatrix, "MaterialInstItemMatrix" )
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::MaterialInstItemMatrixBlock, "MaterialInstItemMatrixBlock" )

namespace ork { namespace lev2 {

/////////////////////////////////////////////////////////////////////////

void MaterialInstApplicator::Describe() {}
void MaterialInstItem::Describe() {}
void MaterialInstItemMatrix::Describe() {}
void MaterialInstItemMatrixBlock::Describe() {}

void GfxMaterial::Describe()
{

}


/////////////////////////////////////////////////////////////////////////

RenderQueueSortingData::RenderQueueSortingData()
	: miSortingPass( 4 )
	, miSortingOffset( 0 )
	, mbTransparency( false )
{

}

/////////////////////////////////////////////////////////////////////////

TextureContext::TextureContext( const Texture *ptex, float repU, float repV )
	: mpTexture( ptex )
	, mfRepeatU( repU )
	, mfRepeatV( repV )
{
}

/////////////////////////////////////////////////////////////////////////

GfxMaterial::GfxMaterial() 
	: miNumPasses(0)
	, mRenderContexInstData( 0 )
	, mMaterialName( AddPooledString("DefaultMaterial") )
{
	PushDebug(false);
}

GfxMaterial::~GfxMaterial()
{
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial::PushDebug(bool bdbg)
{
	mDebug.push(bdbg);
}
void GfxMaterial::PopDebug()
{
	mDebug.pop();
}
bool GfxMaterial::IsDebug()
{
	return mDebug.top();
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial::SetTexture( ETextureDest edest, const TextureContext & tex )
{
	mTextureMap[edest] = tex;
}

const TextureContext & GfxMaterial::GetTexture( ETextureDest edest ) const
{
	return mTextureMap[edest];
}

TextureContext & GfxMaterial::GetTexture( ETextureDest edest ) 
{
	return mTextureMap[edest];
}


} }


/////////////////////////////////////////////////////////////////////////
