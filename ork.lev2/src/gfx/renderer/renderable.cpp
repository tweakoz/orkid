////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::IRenderable, "IRenderable" )
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::IRenderableDag, "IRenderableDag" )
INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::ModelRenderable, "ModelRenderable" );

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

void IRenderable::Describe() {}
void IRenderableDag::Describe() {}
void ModelRenderable::Describe() {}

IRenderable::IRenderable()
{
}

IRenderableDag::IRenderableDag()
	: IRenderable()
	, mpObject(0)
	, mModColor( fcolor4::White() )
	, mDrwDataA(nullptr)
	, mDrwDataB(nullptr)
{
}

///////////////////////////////////////////////////////////////////////////////

CallbackRenderable::CallbackRenderable(Renderer *renderer)
	: IRenderableDag()
	, mSortKey( 0 )
	, mMaterialIndex( 0 )
	, mMaterialPassIndex( 0 )
	, mUserData0()
	, mUserData1()
	, mRenderCallback( 0 )
{
}

void CallbackRenderable::Render(const Renderer *renderer) const
{
	renderer->RenderCallback( *this );
}


///////////////////////////////////////////////////////////////////////////////

ModelRenderable::ModelRenderable(Renderer *renderer)
	: IRenderableDag()
	, mModelInst( 0 )
	, mSortKey( 0 )
	, mMaterialIndex( 0 )
	, mMaterialPassIndex( 0 )
	, mScale(1.0f)
	, mEdgeColor(-1)
///////////////////////////
	, mMesh(0)
	, mSubMesh(0)
	, mCluster(0)
	, mRotate(0.0f,0.0f,0.0f)
	, mOffset(0.0f,0.0f,0.0f)
{
	for(int i = 0; i < kMaxEngineParamFloats; i++)
		mEngineParamFloats[i] = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void ModelRenderable::SetEngineParamFloat(int idx, float fv)
{
	OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

	mEngineParamFloats[idx] = fv;
}

///////////////////////////////////////////////////////////////////////////////

float ModelRenderable::GetEngineParamFloat(int idx) const
{
	OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

	return mEngineParamFloats[idx];
}

///////////////////////////////////////////////////////////////////////////////

void ModelRenderable::Render(const Renderer *renderer) const
{
	renderer->RenderModel( *this );
}

///////////////////////////////////////////////////////////////////////////////

bool ModelRenderable::CanGroup( const IRenderable* oth ) const
{
	const ModelRenderable* pren = ork::rtti::autocast(oth);
	if( pren )
	{
		const lev2::XgmSubMesh* submesh = pren->GetSubMesh();
		const GfxMaterial* mtl = submesh->GetMaterial();
		const GfxMaterial* mtl2 = GetSubMesh()->GetMaterial();
		return (mtl==mtl2);
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

} } // namespace ork
