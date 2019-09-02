////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include "lev3_test.h"
#include <math.h>
#include <ork/kernel/orkpool.h>
#include <ork/kernel/Array.hpp>
#include <ork/math/collision_test.h>
#include <ork/math/raytracer.h>
#include <ork/math/sphere.h>
#include <ork/math/plane.hpp>
#include "render_graph.h"
#include "shader_funcs.h"
//#include <CL/cl.h>
//#include <CL/clext.h>

///////////////////////////////////////////////////////////////////////////////

ShaderBuilder::ShaderBuilder(ork::Engine* tracer,const RenderData*prdata) 
	: mpbakeshader(new MyBakeShader(*tracer,prdata))
	, mpmaterial(new ork::Material)
{
	rend_shader* prendshader = new Shader1();//prdata->mClEngine);
	//rend_shader* prendshader = new Shader2();
	prendshader->mRenderData = prdata;
	mpbakeshader->mPlatformShader.Set<rend_shader*>(prendshader);
}

///////////////////////////////////////////////////////////////////////////////

ork::BakeShader* ShaderBuilder::CreateShader(const ork::RgmSubMesh& sub) const // virtual
{
	return mpbakeshader;
}

///////////////////////////////////////////////////////////////////////////////

ork::Material* ShaderBuilder::CreateMaterial(const ork::RgmSubMesh& sub) const // virtual
{
	return mpmaterial;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

MyBakeShader::MyBakeShader(ork::Engine& eng,const RenderData*prdata)
	: ork::BakeShader(eng)
{
}

///////////////////////////////////////////////////////////////////////////////

void MyBakeShader::Compute( int ix, int iy ) const // virtual 
{
}

///////////////////////////////////////////////////////////////////////////////

Shader2::Shader2()
{
	mTexture1.Load( "data/tex/whirlmetalseamless.tga" );
	mSphMapTexture.Load( "data/tex/whirl1teralpha.tga" );
}

///////////////////////////////////////////////////////////////////////////////
Shader1::Shader1(/*const CLengine& eng*/)
{
//	SourceBuffer srcbuf;
//	srcbuf.Load( "data/clshader/testshader.cl" );
	//srcbuf.CreateCLprogram( eng.GetDevice(), mCLKernelmProgram, mKernel, "testshader" );
//	mCLKernel.Compile( eng.GetDevice(), srcbuf, "testshader" );

	mTexture1.Load( "data/tex/colornoise.tga" );

	mTexture1.Init( /*eng.GetDevice()*/ );

	this->mpVolumeShader = new test_volume_shader;

}

ork::fvec4 test_volume_shader::ShadeVolume( const ork::fvec3& entrywpos, const ork::fvec3& exitwpos ) const // virtual 
{
	float fdist = (exitwpos-entrywpos).Mag();
	float fsd = 1.5f*fdist/10.0f;
	float falpha = std::pow(fsd,2.0f);
	return ork::fvec4(1.7f,0.2f,0.2f,falpha);
}

///////////////////////////////////////////////////////////////////////////////

void Shader1::Shade( const rend_prefragment& prefrag, rend_fragment* pdstfrag )  const // virtual
{
	const rend_ivtx* srcvtxR = prefrag.srcvtxR;
	const rend_ivtx* srcvtxS = prefrag.srcvtxS;
	const rend_ivtx* srcvtxT = prefrag.srcvtxT;
	const ork::fvec3& onrmR = srcvtxR->mObjSpaceNrm;
	const ork::fvec3& onrmS = srcvtxS->mObjSpaceNrm;
	const ork::fvec3& onrmT = srcvtxT->mObjSpaceNrm;
	float r = prefrag.mfR;
	float s = prefrag.mfS;
	float t = prefrag.mfT;
	float onx = onrmR.GetX()*r+onrmS.GetX()*s+onrmT.GetX()*t;
	float ony = onrmR.GetY()*r+onrmS.GetY()*s+onrmT.GetY()*t;
	float onz = onrmR.GetZ()*r+onrmS.GetZ()*s+onrmT.GetZ()*t;
	ork::fvec3 c( onx, ony, onz );
	pdstfrag->mRGBA = ork::fvec4( c, 1.0f );
	pdstfrag->mZ = prefrag.mfZ;
}

///////////////////////////////////////////////////////////////////////////////

size_t shrRoundUp(int group_size, int global_size) 
{
    int r = global_size % group_size;
    if(r == 0) 
    {
        return global_size;
    } else 
    {
        return global_size + group_size - r;
    }
}

///////////////////////////////////////////////////////////////////////////////

#if 0
void Shader1::ShadeBlock( AABuffer& aabuf, int ifragbase, int icount, int inumtri ) const
{	
	if( 0 == icount ) return;
	rend_prefrags& PFRAGS = aabuf.mPreFrags;
	//////////////////////////////////////////////////////
	int imaxwgsize = mCLKernel.GetMaxWorkgroupSize();
	size_t ilocsize = imaxwgsize; 
	size_t iglbsizw = shrRoundUp(ilocsize,icount);
	//////////////////////////////////////////////////////
	int ifraginpsize = icount*5*sizeof(float);
	int ifragoutsize = icount*11*sizeof(float);
	int ifraginpsizePAD = iglbsizw*5*sizeof(float); // because we might execute a few extra to fill all warps 
	int ifragoutsizePAD = iglbsizw*11*sizeof(float); // because we might execute a few extra to fill all warps
	//////////////////////////////////////////////////////
	OrkAssert( ifraginpsizePAD < aabuf.mFragInpClBuffer->GetSize() );
	OrkAssert( ifragoutsizePAD < aabuf.mFragOutClBuffer->GetSize() );
	//////////////////////////////////////////////////////
	// copy prefrag data to cl buffer
	//////////////////////////////////////////////////////
	float* pprefragbuffer = (float*) aabuf.mFragInpClBuffer->GetBufferRegion();
	int ifragidx = ifragbase;
	for( int i=0; i<icount; i++ )
	{	const rend_prefragment& pfrag = PFRAGS.mPreFrags[ifragidx++];
		int idx = i*5;
		pprefragbuffer[idx+0] = pfrag.mfR;
		pprefragbuffer[idx+1] = pfrag.mfS;
		pprefragbuffer[idx+2] = pfrag.mfT;
		pprefragbuffer[idx+3] = pfrag.mfZ;
		pprefragbuffer[idx+4] = float(pfrag.miTri);
	}
	//////////////////////////////////////////////////////
	mRenderData->mClEngine.GetDevice()->Lock();
	//////////////////////////////////////////////////////
	{	//mTexture1.Init( mRenderData->mClEngine.GetDevice() );

		aabuf.mTriangleClBuffer->TransferAndBlock( mRenderData->mClEngine.GetDevice(), inumtri*36*sizeof(float) );
		aabuf.mFragInpClBuffer->TransferAndBlock( mRenderData->mClEngine.GetDevice(), ifraginpsize );
		//////////////////////////////////////////////
		const ork::fvec3& veye = mRenderData->mEye;
		const float* peye = veye.GetArray();
		//////////////////////////////////////////////
		// set arguments
		int iarg = 0;
		aabuf.mTriangleClBuffer->SetArg( mCLKernel.GetKernel(), iarg++ );
		aabuf.mFragInpClBuffer->SetArg( mCLKernel.GetKernel(), iarg++ );
		aabuf.mFragOutClBuffer->SetArg( mCLKernel.GetKernel(), iarg++ );
		clSetKernelArg(mCLKernel.GetKernel(), iarg++, sizeof(cl_mem), &mTexture1.mCLhandle );
		clSetKernelArg(mCLKernel.GetKernel(), iarg++, sizeof(float), peye+0);
		clSetKernelArg(mCLKernel.GetKernel(), iarg++, sizeof(float), peye+1);
		clSetKernelArg(mCLKernel.GetKernel(), iarg++, sizeof(float), peye+2);
		clSetKernelArg(mCLKernel.GetKernel(), iarg++, sizeof(int), &inumtri);
		clSetKernelArg(mCLKernel.GetKernel(), iarg++, sizeof(int), &icount);
		//////////////////////////////////////////////
		// enqueue kernel
		mCLKernel.Enqueue( mRenderData->mClEngine.GetDevice(), 1, &iglbsizw, &ilocsize );
		//ierr2a = clFinish( mRenderData->mClEngine.GetDevice()->GetCmdQueue() );
		//////////////////////////////////////////////
		aabuf.mFragOutClBuffer->Transfer( mRenderData->mClEngine.GetDevice(), ifragoutsize, true );
		//////////////////////////////////////////////
	}
	//////////////////////////////////////////////////////
	mRenderData->mClEngine.GetDevice()->UnLock();
	//////////////////////////////////////////////////////
	// READ data from clbuffer to fragments
	//////////////////////////////////////////////////////
	const float* ppostfragbuffer = (const float*) aabuf.mFragOutClBuffer->GetBufferRegion();
	for( int i=0; i<icount; i++ )
	{	rend_fragment* frag = aabuf.mpFragments[i];
		//const rend_prefragment& pfrag = PFRAGS.mPreFrags[ifragbase+i];
		frag->mRGBA.SetXYZ( ppostfragbuffer[2], ppostfragbuffer[1], ppostfragbuffer[0] );
		frag->mRGBA.SetW( ppostfragbuffer[3] );
		frag->mZ = ppostfragbuffer[4];
		frag->mWorldPos.SetXYZ( ppostfragbuffer[5], ppostfragbuffer[6], ppostfragbuffer[7] );
		frag->mWldSpaceNrm.SetXYZ( ppostfragbuffer[8], ppostfragbuffer[9], ppostfragbuffer[10] );
		frag->mpShader = this;
		ppostfragbuffer += 11;
	}
	//////////////////////////////////////////////////////
}
#else
void Shader1::ShadeBlock( AABuffer& aabuf, int ifragbase, int icount, int inumtri ) const
{	
	rend_prefrags& PFRAGS = aabuf.mPreFrags;
	if( 0 == icount ) return;
	//////////////////////////////////////////////////////
	int ifraginpsize = icount*4*sizeof(float);
	int ifragoutsize = icount*5*sizeof(float);
	//////////////////////////////////////////////////////
	float* pprefragbuffer = (float*) aabuf.mFragInpClBuffer->GetBufferRegion();
	int ifragidx = ifragbase;
	for( int i=0; i<icount; i++ )
	{	const rend_prefragment& pfrag = PFRAGS.mPreFrags[ifragidx++];
		rend_fragment* frag = aabuf.mpFragments[i];
//		const ork::fvec3 nrm = (frag->mWldSpaceNrm*0.5f)+ork::fvec3(0.5f,0.5f,0.5f);
		frag->mRGBA.SetXYZ( pfrag.mfR, pfrag.mfS, pfrag.mfT );
		//frag->mRGBA.SetXYZ( nrm.GetX(), nrm.GetY(), nrm.GetZ() );
		frag->mRGBA.SetW( 0.5f );
		frag->mZ = pfrag.mfZ;
	}
	//////////////////////////////////////////////////////
}
#endif
///////////////////////////////////////////////////////////////////////////////

void rend_shader::ShadeBlock( AABuffer& aabuf, int ifragbase, int icount, int inumtri ) const
{	
	rend_prefrags& PFRAGS = aabuf.mPreFrags;
	int ifragctr = ifragbase;
	for( int i=0; i<icount; i++ )
	{	const rend_prefragment& pfrag = PFRAGS.mPreFrags[ifragctr++];
		rend_fragment* frag = aabuf.mpFragments[i];
		//frag->mRGBA.SetABGRU32( rand()+(rand()<<16) );
		Shade( pfrag, frag );
	}
}

