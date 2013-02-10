////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////



inline ork::CVector4 SphMap( const ork::CVector3& N, const ork::CVector3& EyeToPointDir, const rend_texture2D& tex ) 
{
	ork::CVector3 ref = EyeToPointDir-N*(N.Dot(EyeToPointDir)*2.0f);
	float p = ::sqrtf( ref.GetX()*ref.GetX()+ref.GetY()*ref.GetY()+::powf(ref.GetZ()+1.0f,2.0f) );
	float reflectS = ref.GetX()/(2.0f*p)+0.5f;
	float reflectT = ref.GetY()/(2.0f*p)+0.5f;
	return tex.sample_point( reflectS, reflectT, true, true );
}

inline ork::CVector4 OctaveTex( int inumoctaves, float fu, float fv, float texscale, float texamp, float texscalemodifier, float texampmodifier, const rend_texture2D& tex )
{
	ork::CVector4 tex0;
	for( int i=0; i<inumoctaves; i++ )
	{
		tex0 += tex.sample_point( std::abs(fu*texscale), std::abs(fv*texscale), true, true )*texamp;
		texscale *= texscalemodifier;
		texamp *= texampmodifier;
	}
	return tex0;
}

///////////////////////////////////////////////////////////////////////////////
struct test_volume_shader : public rend_volume_shader
{
	ork::CVector4 ShadeVolume( const ork::CVector3& entrywpos, const ork::CVector3& exitwpos ) const; // virtual 
};
///////////////////////////////////////////////////////////////////////////////
struct Shader1 : public rend_shader
{
	ork::CPerlin2D						mPerlin2D;
	rend_texture2D mTexture1;
	//cl_program							mProgram;
	//cl_kernel							mKernel;
	//CLKernel							mCLKernel;

	eType GetType() const { return EShaderTypeSurface; } // virtual

	void Shade( const rend_prefragment& prefrag, rend_fragment* pdstfrag )  const; 
	void ShadeBlock( AABuffer& aabuf, int ifragbase, int icount, int inumtri ) const; // virtual

	Shader1(/*const CLengine& eng*/);
};

///////////////////////////////////////////////////////////////////////////////

struct Shader2 : public rend_shader
{
	rend_texture2D mTexture1;
	rend_texture2D mSphMapTexture;

	Shader2();

	eType GetType() const { return EShaderTypeSurface; } // virtual

	void Shade( const rend_prefragment& prefrag, rend_fragment* pdstfrag ) const  // virtual
	{
		const rend_ivtx* srcvtxR = prefrag.srcvtxR;
		const rend_ivtx* srcvtxS = prefrag.srcvtxS;
		const rend_ivtx* srcvtxT = prefrag.srcvtxT;
		const ork::CVector3& wposR = srcvtxR->mWldSpacePos;
		const ork::CVector3& wposS = srcvtxS->mWldSpacePos;
		const ork::CVector3& wposT = srcvtxT->mWldSpacePos;
		const ork::CVector3& wnrmR = srcvtxR->mWldSpaceNrm;
		const ork::CVector3& wnrmS = srcvtxS->mWldSpaceNrm;
		const ork::CVector3& wnrmT = srcvtxT->mWldSpaceNrm;
		float r = prefrag.mfR;
		float s = prefrag.mfS;
		float t = prefrag.mfT;
		float z = prefrag.mfZ;
		float wnx = wnrmR.GetX()*r+wnrmS.GetX()*s+wnrmT.GetX()*t;
		float wny = wnrmR.GetY()*r+wnrmS.GetY()*s+wnrmT.GetY()*t;
		float wnz = wnrmR.GetZ()*r+wnrmS.GetZ()*s+wnrmT.GetZ()*t;
		float wx = wposR.GetX()*r+wposS.GetX()*s+wposT.GetX()*t;
		float wy = wposR.GetY()*r+wposS.GetY()*s+wposT.GetY()*t;
		float wz = wposR.GetZ()*r+wposS.GetZ()*s+wposT.GetZ()*t;
		/////////////////////////////////////////////////////////////////
		float area = prefrag.mpSrcPrimitive->mfArea;
		float areaintens = (area==0.0f) ? 0.0f : ::powf( 100.0f / area, 0.7f );
		/////////////////////////////////////////////////////////////////
		float wxm = ::fmod( ::abs(wx)/20.0f, 1.0f )<0.1f;
		float wym = ::fmod( ::abs(wy)/20.0f, 1.0f )<0.1f;
		float wzm = ::fmod( ::abs(wz)/20.0f, 1.0f )<0.1f;
		ork::CVector3 vN = (ork::CVector3(wx,wy,wz)-mRenderData->mEye).Normal();
		/////////////////////////////////////////////////////////////////
		ork::CVector4 tex0 = OctaveTex( 4, wy, wy, 0.01f, 0.407f, 2.0f, 0.6f, mTexture1 );
		ork::CVector4 tex1 = OctaveTex( 4, wx, wz, 0.01f, 0.507f, 2.0f, 0.5f, mTexture1 );
		ork::CVector4 texout = tex0*tex1;
		/////////////////////////////////////////////////////////////////
		float fgrid = (wxm+wym+wzm)!=0.0f;
		///////////////////////////////////////////////
		float fZblend = 64.0f*::pow( z, 5.0f );
		///////////////////////////////////////////////
		ork::CVector4 sphtexout = SphMap( ork::CVector3(wnx,wny,wnz), vN, mSphMapTexture );
		///////////////////////////////////////////////
		ork::CVector3 c0( fgrid, fgrid, fgrid );
		ork::CVector3 c( wnx, wny, wnz ); c=c*0.6f+c0*0.1f+texout*::powf(areaintens,0.1f)*0.7f+sphtexout*0.5f;
		pdstfrag->mRGBA.Set(c.GetX(), c.GetY(), c.GetZ(), fZblend ); // cunc
		pdstfrag->mZ = z;
		pdstfrag->mpPrimitive = prefrag.mpSrcPrimitive;
		pdstfrag->mpShader = this;
		pdstfrag->mWldSpaceNrm.SetXYZ( wnx, wny, wnz );
		pdstfrag->mWorldPos.SetXYZ( wx, wy, wz );
		///////////////////////////////////////////////
	}
	//void ShadeBlock( AABuffer& aabuf, const rend_prefragsubgroup* pfgsubgrp, int ifragbase, int icount ) = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct MyBakeShader : public ork::BakeShader
{
	MyBakeShader(ork::Engine& eng,const RenderData*prdata);
	void Compute( int ix, int iy ) const; // virtual 
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderBuilder : public ork::RgmShaderBuilder
{
	MyBakeShader*	mpbakeshader;
	ork::Material*		mpmaterial;

	ShaderBuilder(ork::Engine* tracer,const RenderData*prdata);
	ork::BakeShader* CreateShader(const ork::RgmSubMesh& sub) const; // virtual
	ork::Material* CreateMaterial(const ork::RgmSubMesh& sub) const; // virtual
};

///////////////////////////////////////////////////////////////////////////////
