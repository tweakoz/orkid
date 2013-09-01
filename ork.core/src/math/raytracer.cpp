///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2009, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <cmath>
#include <ork/orkconfig.h>
#include <ork/orktypes.h>
#include <ork/math/cfloat.h>
#include <ork/math/spheretree.h>
#include <ork/math/raytracer.h>
#include <ork/math/sphere.h>
#include <ork/math/plane.h>
#include <ork/math/octree.h>
#include <ork/math/collision_test.h>
#include <ork/kernel/Array.h>
#include <ork/kernel/Array.hpp>
#include <ork/kernel/gstack.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.hpp>
#include <queue>
//#include <boost/gil/typedefs.hpp>
//#include <boost/cast.hpp>
//#include <boost/gil/extension/io/png_dynamic_io.hpp>
//#include <IL/il.h>
//#include <IL/ilut.h>
//#include <pthread.h>

#if defined(IX)
#include <unistd.h>
#endif

//#pragma comment( lib, "devil.lib" )
//#pragma comment( lib, "ilu.lib" )

s64 giNumRays = 0;

namespace ork {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static const int kIW = 2048	;
static const int kOS = 0;
static const float kJITTER = 0.5f; // 0.5

static int GetNumCores()
{
	#if defined(IX)
	int numCPUs = sysconf(_SC_NPROCESSORS_ONLN);
	#else
	SYSTEM_INFO sysinfo;
	GetSystemInfo( &sysinfo );
	int numCPUs = sysinfo.dwNumberOfProcessors;
	#endif
	orkprintf( "NumCpus<%d>\n", numCPUs );
	fflush(stdout);
	return numCPUs;
}

void RgmTri::Compute()
{
	mFacePlane.CalcPlaneFromTriangle( mpv0->pos, mpv1->pos, mpv2->pos );
	const CVector3 vn = mFacePlane.n;
	mEdgePlane0.CalcPlaneFromTriangle( mpv0->pos, vn+mpv0->pos, mpv1->pos );
	mEdgePlane1.CalcPlaneFromTriangle( mpv1->pos, vn+mpv1->pos, mpv2->pos );
	mEdgePlane2.CalcPlaneFromTriangle( mpv2->pos, vn+mpv2->pos, mpv0->pos );
	mArea = (mpv0->pos-mpv2->pos).Cross(mpv1->pos-mpv2->pos).Mag() * 0.5f;

}

Jitterer::Jitterer( int ikos, float fradius, const CVector3& dX, const CVector3& dY )
{
	miKos = ikos;

	int idim = (miKos*2)+1;
	miNumSamples = idim*idim;

	int isamp = 0;

	int idiv = (ikos==0) ? 1 : ikos;

	for( int iy=-ikos; iy<=ikos; iy++ )
	{
		float fy = fradius*float(iy)/float(idiv);
		for( int ix=-ikos; ix<=ikos; ix++ )
		{
			float fx = fradius*float(ix)/float(idiv);
			sample[isamp++] = dX*fx+dY*fy;
		}
	}
	OrkAssert(isamp==miNumSamples);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Material::Material()
	: mColor(  0.2f, 0.2f, 0.2f  )
	, m_Refl( 0 )
	, m_Diff( 0.2f )
	, m_Spec( 0.8f )
	, m_RIndex( 1.5f )
	, m_DRefl( 0 )
{
}

///////////////////////////////////////////////////////////////////////////////

void Material::SetParameters( float a_Refl, float a_Refr, const CVector3& a_Col, float a_Diff, float a_Spec )
{
	m_Refl = a_Refl;
	m_Refr = a_Refr;
	mColor = a_Col;
	m_Diff = a_Diff;
	m_Spec = a_Spec;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Primitive::~Primitive()
{
}

	
///////////////////////////////////////////////////////////////////////////////

CVector3 Primitive::GetColor( const CVector3& a_Pos ) const
{
	return mMaterial->GetColor();
}

///////////////////////////////////////////////////////////////////////////////

#define FINDMINMAX( x0, x1, x2, min, max ) \
  min = max = x0; if(x1<min) min=x1; if(x1>max) max=x1; if(x2<min) min=x2; if(x2>max) max=x2;
// X-tests
#define AXISTEST_X01( a, b, fa, fb )											\
	p0 = a * v0[1] - b * v0[2], p2 = a * v2[1] - b * v2[2]; \
    if (p0 < p2) { min = p0; max = p2;} else { min = p2; max = p0; }			\
	rad = fa * a_BoxHalfsize[1] + fb * a_BoxHalfsize[2];				\
	if (min > rad || max < -rad) return 0;
#define AXISTEST_X2( a, b, fa, fb )												\
	p0 = a * v0[1] - b * v0[2], p1 = a * v1[1] - b * v1[2];	\
    if (p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0;}			\
	rad = fa * a_BoxHalfsize[1] + fb * a_BoxHalfsize[2];				\
	if(min>rad || max<-rad) return 0;
// Y-tests
#define AXISTEST_Y02( a, b, fa, fb )											\
	p0 = -a * v0[0] + b * v0[2], p2 = -a * v2[0] + b * v2[2]; \
    if(p0 < p2) { min = p0; max = p2; } else { min = p2; max = p0; }			\
	rad = fa * a_BoxHalfsize[0] + fb * a_BoxHalfsize[2];				\
	if (min > rad || max < -rad) return 0;
#define AXISTEST_Y1( a, b, fa, fb )												\
	p0 = -a * v0[0] + b * v0[2], p1 = -a * v1[0] + b * v1[2]; \
    if (p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0; }			\
	rad = fa * a_BoxHalfsize[0] + fb * a_BoxHalfsize[2];				\
	if (min > rad || max < -rad) return 0;
// Z-tests
#define AXISTEST_Z12( a, b, fa, fb )											\
	p1 = a * v1[0] - b * v1[1], p2 = a * v2[0] - b * v2[1]; \
    if(p2 < p1) { min = p2; max = p1; } else { min = p1; max = p2; }			\
	rad = fa * a_BoxHalfsize[0] + fb * a_BoxHalfsize[1];				\
	if (min > rad || max < -rad) return 0;
#define AXISTEST_Z0( a, b, fa, fb )												\
	p0 = a * v0[0] - b * v0[1], p1 = a * v1[0] - b * v1[1];	\
    if(p0 < p1) { min = p0; max = p1; } else { min = p1; max = p0; }			\
	rad = fa * a_BoxHalfsize[0] + fb * a_BoxHalfsize[1];				\
	if (min > rad || max < -rad) return 0;

///////////////////////////////////////////////////////////////////////////////

bool Primitive::PlaneBoxOverlap( const CVector3& a_Normal, const CVector3& a_Vert, const CVector3& a_MaxBox )
{
	CVector3 vmin, vmax;
	for( int q = 0; q < 3; q++ )
	{
		float v = a_Vert[q];
		if (a_Normal[q] > 0.0f)
		{
			vmin[q] = -a_MaxBox[q] - v;
			vmax[q] =  a_MaxBox[q] - v;
		}
		else
		{
			vmin[q] =  a_MaxBox[q] - v;
			vmax[q] = -a_MaxBox[q] - v;
		}
	}
	if (a_Normal.Dot(vmin) > 0.0f) return false;
	if (a_Normal.Dot(vmax) >= 0.0f) return true;
	return false;
}

///////////////////////////////////////////////////////////////////////////////

bool Primitive::IntersectTriBox( const CVector3& a_BoxCentre, const CVector3& a_BoxHalfsize, const CVector3& a_V0, const CVector3& a_V1, const CVector3& a_V2 )
{
	CVector3 v0, v1, v2, normal, e0, e1, e2;
	float min, max, p0, p1, p2, rad, fex, fey, fez;
	v0 = a_V0 - a_BoxCentre;
	v1 = a_V1 - a_BoxCentre;
	v2 = a_V2 - a_BoxCentre;
	e0 = v1 - v0, e1 = v2 - v1, e2 = v0 - v2;
	fex = fabsf( e0[0] );
	fey = fabsf( e0[1] );
	fez = fabsf( e0[2] );
	AXISTEST_X01( e0[2], e0[1], fez, fey );
	AXISTEST_Y02( e0[2], e0[0], fez, fex );
	AXISTEST_Z12( e0[1], e0[0], fey, fex );
	fex = fabsf( e1[0] );
	fey = fabsf( e1[1] );
	fez = fabsf( e1[2] );
	AXISTEST_X01( e1[2], e1[1], fez, fey );
	AXISTEST_Y02( e1[2], e1[0], fez, fex );
	AXISTEST_Z0 ( e1[1], e1[0], fey, fex );
	fex = fabsf( e2[0] );
	fey = fabsf( e2[1] );
	fez = fabsf( e2[2] );
	AXISTEST_X2 ( e2[2], e2[1], fez, fey );
	AXISTEST_Y1 ( e2[2], e2[0], fez, fex );
	AXISTEST_Z12( e2[1], e2[0], fey, fex );
	FINDMINMAX( v0[0], v1[0], v2[0], min, max );
	if (min > a_BoxHalfsize[0] || max < -a_BoxHalfsize[0]) return false;
	FINDMINMAX( v0[1], v1[1], v2[1], min, max );
	if (min > a_BoxHalfsize[1] || max < -a_BoxHalfsize[1]) return false;
	FINDMINMAX( v0[2], v1[2], v2[2], min, max );
	if (min > a_BoxHalfsize[2] || max < -a_BoxHalfsize[2]) return false;
	normal = e0.Cross( e1 );
	if (!PlaneBoxOverlap( normal, v0, a_BoxHalfsize )) return false;
	return true;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

RaytTriangle::RaytTriangle( const RgmVertex* v1, const RgmVertex* v2, const RgmVertex* v3 )
{
	mMaterial = 0;
	mVertex[0] = v1;
	mVertex[1] = v2;
	mVertex[2] = v3;

	
	//////////////////////////////////////
	// precompute what we can
	//////////////////////////////////////

	CVector3 A = mVertex[0]->pos;
	CVector3 B = mVertex[1]->pos;
	CVector3 C = mVertex[2]->pos;
	CVector3 c = B - A;
	CVector3 b = C - A;
	mN = b.Cross( c );
	int u, v;
	if (std::abs( mN.GetX() ) > std::abs( mN.GetY()))
	{
		if (std::abs( mN.GetX() ) > std::abs( mN.GetZ() )) k = 0; else k = 2;
	}
	else
	{
		if (std::abs( mN.GetY() ) > std::abs( mN.GetZ() )) k = 1; else k = 2;
	}
	u = (k + 1) % 3;
	v = (k + 2) % 3;
	// precomp
	float krec = 1.0f / mN[k];
	nu = mN[u] * krec;
	nv = mN[v] * krec;
	nd = mN.Dot( A ) * krec;
	// first line equation
	float reci = 1.0f / (b[u] * c[v] - b[v] * c[u]);
	bnu = b[u] * reci;
	bnv = -b[v] * reci;
	// second line equation
	cnu = c[v] * reci;
	cnv = -c[u] * reci;
	// finalize normal
	mN.Normalize();
	//mVertex[0]->SetNormal( m_N );
	//mVertex[1]->SetNormal( m_N );
	//mVertex[2]->SetNormal( m_N );

}

///////////////////////////////////////////////////////////////////////////////

AABox RaytTriangle::GetAABox() const
{
	AABox ret;
	ret.BeginGrow();
	ret.Grow( mVertex[0]->pos );
	ret.Grow( mVertex[1]->pos );
	ret.Grow( mVertex[2]->pos );
	ret.EndGrow();
	return ret;
}

///////////////////////////////////////////////////////////////////////////////

int RaytTriangle::Intersect( const Ray3& a_Ray, CVector3& isect, float& a_Dist ) const
{
	const RgmTri* tri = this->mRgmPoly;
	const CVector3& v0 = mVertex[0]->pos;
	const CVector3& v1 = mVertex[1]->pos;
	const CVector3& v2 = mVertex[2]->pos;
	const CPlane& fp = tri->mFacePlane;
	const CPlane& ep0 = tri->mEdgePlane0;
	const CPlane& ep1 = tri->mEdgePlane1;
	const CPlane& ep2 = tri->mEdgePlane2;
	float s,t;
	bool bv = CollisionTester::RayTriangleTest( a_Ray, fp, ep0, ep1, ep2, isect, a_Dist );
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

CVector3 RaytTriangle::GetNormal( const CVector3& a_Pos ) const
{ 
	CVector3 N1 = mVertex[0]->nrm;
	CVector3 N2 = mVertex[1]->nrm;
	CVector3 N3 = mVertex[2]->nrm;
	CVector3 N = N1 + mU * (N2 - N1) + mV * (N3 - N1);
	N.Normalize();
	return N;
}

///////////////////////////////////////////////////////////////////////////////

bool RaytTriangle::IntersectBox( const AABox& a_Box ) const
{
	return IntersectTriBox( a_Box.Min() + a_Box.GetSize() * 0.5f, a_Box.GetSize() * 0.5f, 
							mVertex[0]->pos, mVertex[1]->pos, mVertex[2]->pos );
}

///////////////////////////////////////////////////////////////////////////////

void RaytTriangle::Rasterize( Engine* peng ) const
{
	int iw = peng->GetWidth();
	int ih = peng->GetHeight();

	const BakeShader* bshader = GetBakeShader();
	const CVector2& uv0 = mRgmPoly->mpv0->uv;
	const CVector2& uv1 = mRgmPoly->mpv1->uv;
	const CVector2& uv2 = mRgmPoly->mpv2->uv;

	CVector3 v0( uv0.GetX()*iw, uv0.GetY()*ih, 0.0f );
	CVector3 v1( uv1.GetX()*iw, uv1.GetY()*ih, 0.0f );
	CVector3 v2( uv2.GetX()*iw, uv2.GetY()*ih, 0.0f );

	BakeShadowFragment bv0, bv1, bv2;
	bv0.mPos = mRgmPoly->mpv0->pos;
	bv1.mPos = mRgmPoly->mpv1->pos;
	bv2.mPos = mRgmPoly->mpv2->pos;
	bv0.mNrm = mRgmPoly->mpv0->nrm;
	bv1.mNrm = mRgmPoly->mpv1->nrm;
	bv2.mNrm = mRgmPoly->mpv2->nrm;

	peng->RasterizeTriangle(	*bshader,
								int(v0.GetX()), int(v0.GetY()), bv0, 
								int(v1.GetX()), int(v1.GetY()), bv1, 
								int(v2.GetX()), int(v2.GetY()), bv2);

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

RaytSphere::RaytSphere( CVector3& center, float radius )
{
	mCenter = center;
	mSqRadius = radius * radius;
	mRadius = radius;
	mRRadius = 1.0f / radius;
	//mMaterial = new Material();
	// set vectors for texture mapping
	mVn = CVector3( 0, 1, 0 );
	mVe = CVector3( 1, 0, 0 );
	mVc = mVn.Cross( mVe );
}

///////////////////////////////////////////////////////////////////////////////

bool RaytSphere::IntersectSphereBox( const CVector3& a_Centre, const AABox& a_Box ) const
{
	float dmin = 0;
	CVector3 spos = a_Centre;
	CVector3 bpos = a_Box.Min();
	CVector3 bsize = a_Box.GetSize();
	for ( int i = 0; i < 3; i++ )
	{
		if (spos[i] < bpos[i]) 
		{
			dmin = dmin + (spos[i] - bpos[i]) * (spos[i] - bpos[i]);
		}
		else if (spos[i] > (bpos[i] + bsize[i])) 
		{
			dmin = dmin + (spos[i] - (bpos[i] + bsize[i])) * (spos[i] - (bpos[i] + bsize[i]));
		}
	}
	return (dmin <= mSqRadius);
}

///////////////////////////////////////////////////////////////////////////////

bool RaytSphere::IntersectBox( const AABox& a_Box ) const
{
	return IntersectSphereBox( mCenter, a_Box );
}

///////////////////////////////////////////////////////////////////////////////

void RaytSphere::Rasterize( Engine* peng ) const
{
}

///////////////////////////////////////////////////////////////////////////////

AABox RaytSphere::GetAABox() const
{
	AABox ret;
	return ret;
}

///////////////////////////////////////////////////////////////////////////////

int RaytSphere::Intersect( const Ray3& a_Ray, CVector3& isect, float& a_Dist ) const
{
	CVector3 v = a_Ray.mOrigin - mCenter;
	float b = -v.Dot( a_Ray.mDirection );
	float det = (b * b) - v.Dot( v ) + mSqRadius;
	int retval = MISS;
	if (det > 0)
	{
		det = std::sqrt( det );
		float i1 = b - det;
		float i2 = b + det;
		if (i2 > 0)
		{
			if (i1 < 0) 
			{
				if (i2 < a_Dist) 
				{
					a_Dist = i2;
					retval = INPRIM;
				}
			}
			else
			{
				if (i1 < a_Dist)
				{
					a_Dist = i1;
					retval = HIT;
				}
			}
		}
	}
	return retval;
}

///////////////////////////////////////////////////////////////////////////////

CVector3 RaytSphere::GetNormal( const CVector3& a_Pos ) const
{ 
	return (a_Pos - mCenter) * mRRadius; 
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Scene::Scene()
	: mExtends()
	, mpFixedGrid(0)
{
}

///////////////////////////////////////////////////////////////////////////////

Scene::~Scene()
{
}

///////////////////////////////////////////////////////////////////////////////

void Scene::ExitScene()
{
	if( mpFixedGrid ) delete mpFixedGrid;
	mpFixedGrid = 0;
}

bool Scene::InitScene( const AABox& scene_box )
{
	//Material* mat = new Material();
	//mat->SetParameters( 0.0f, 0.0f, CVector3( 0.4f, 0.3f, 0.3f ), 1.0f, 0.0f );
	//mat = new Material();
	//mat->SetParameters( 0.0f, 0.0f, CVector3( 0.5f, 0.3f, 0.5f ), 0.6f, 0.0f );
	//mat = new Material();
	//mat->SetParameters( 0.0f, 0.0f, CVector3( 0.4f, 0.7f, 0.7f ), 0.5f, 0.0f );
	//mat = new Material();
	//mat->SetParameters( 0.9f, 0, CVector3( 0.9f, 0.9f, 1 ), 0.3f, 0.7f );
	//mat->SetRefrIndex( 1.3f );

	mExtends = scene_box;

	mpFixedGrid = new FixedGrid;

	orkvector<const Primitive*> Prims;

	for( orkmap<std::string,const RgmGeoSet*>::const_iterator it=mGeoSets.begin(); it!=mGeoSets.end(); it++ )
	{
		const std::string& name = it->first;
		const RgmGeoSet* pset = it->second;

		//if( name.find( "caster_" ) != std::string::npos )
		{
			int inump = pset->NumPrimitives();
			for( int ip=0; ip<inump; ip++ )
			{
				const Primitive* prim = pset->GetPrimitive( ip );

				Prims.push_back(prim);
			}

		}
	}


	mpFixedGrid->BuildGrid( scene_box, Prims );

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct RgmLoadAllocator
{	//////////////////////////////
	// per chunk allocation policy
	//////////////////////////////
	void* alloc( const char* pchkname, int ilen )
	{
		void* pmem = 0;
		if( 0 == strcmp( pchkname, "header" ) ) pmem = malloc(ilen);
		else if( 0 == strcmp( pchkname, "modeldata" ) ) pmem = malloc(ilen);

		return pmem;
	}
	//////////////////////////////
	// per chunk deallocation policy
	//////////////////////////////
	void done( const char* pchkname, void* pdata )
	{	
		free( pdata );
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void RgmLightContainer::LoadLitFile( const char* pfilename )
{
	chunkfile::Reader<RgmLoadAllocator> chunkreader( pfilename, "lit" );
	if( chunkreader.IsOk() )
	{	chunkfile::InputStream* HeaderStream = chunkreader.GetStream("header");
		///////////////////////////////////////////////////
		int inumlights = 0;
		HeaderStream->GetItem(inumlights);
		for( int il=0; il<inumlights; il++ )
		{	int iname, itype;
			CMatrix4 mtxW;
			CVector3 clr;
			float intens;
			int   icastsshadows;
			HeaderStream->GetItem(iname);
			HeaderStream->GetItem(mtxW);
			HeaderStream->GetItem(clr);
			HeaderStream->GetItem(intens);
			HeaderStream->GetItem(icastsshadows);
			HeaderStream->GetItem(itype);
			const char* pname = chunkreader.GetString(iname);
			const char* ptype = chunkreader.GetString(itype);

			if( 0 == strcmp( ptype, "PointLight" ) )
			{
				RgmLight rlite( CVector3::Black(), clr );
				HeaderStream->GetItem(rlite.mPos);
				HeaderStream->GetItem(rlite.mFalloff);
				HeaderStream->GetItem(rlite.mRadius);

				rlite.mCastsShadows = bool(icastsshadows);
				mLights[pname] = rlite;
			}
			else // Dir Light
			{
				RgmLight rlite( mtxW.GetTranslation(), clr );

				rlite.mDir = mtxW.GetZNormal();
				rlite.mCastsShadows = bool(icastsshadows);
				mLights[pname] = rlite;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

RgmModel* LoadRgmFile( const char* pfilename, RgmShaderBuilder& shbuilder )
{
	RgmModel* mdl = new RgmModel;

	mdl->mAABox.BeginGrow();
	chunkfile::Reader<RgmLoadAllocator> chunkreader( pfilename, "rgm" );
	if( chunkreader.IsOk() )
	{	chunkfile::InputStream* HeaderStream = chunkreader.GetStream("header");
		chunkfile::InputStream* ModelDataStream = chunkreader.GetStream("modeldata");
		///////////////////////////////////////////////////
		int inumannos = 0;
		HeaderStream->GetItem(inumannos);
		for( int ia=0; ia<inumannos; ia++ )
		{	int ikey, ival;
			HeaderStream->GetItem(ikey);
			HeaderStream->GetItem(ival);
			const char* pkey = chunkreader.GetString(ikey);
			const char* pval = chunkreader.GetString(ival);
			mdl->mAnnos[pkey] = pval;
		}
		///////////////////////////////////////////////////
		HeaderStream->GetItem(mdl->minumsubs);
		mdl->msubmeshes = new RgmSubMesh[mdl->minumsubs];
		for( int is=0; is<mdl->minumsubs; is++ )
		{	RgmSubMesh& sub = mdl->msubmeshes[is];
			sub.mGeoSet = new RgmGeoSet;
			BakeShader* pshader = shbuilder.CreateShader(sub);
			Material* pmaterial = shbuilder.CreateMaterial(sub);
			sub.mpShader = pshader;
			sub.mpMaterial = pmaterial;
			///////////////////////////////////////////////////
			int inumsubannos = 0;
			HeaderStream->GetItem(inumsubannos);
			for( int ia=0; ia<inumsubannos; ia++ )
			{	int ikey, ival;
				HeaderStream->GetItem(ikey);
				HeaderStream->GetItem(ival);
				const char* pkey = chunkreader.GetString(ikey);
				const char* pval = chunkreader.GetString(ival);
				sub.mAnnos[pkey] = pval;
			}
			///////////////////////////////////////////////////
			int inumtotv;
			int iname;
			HeaderStream->GetItem(iname);
			HeaderStream->GetItem(sub.minumverts);
			HeaderStream->GetItem(inumtotv);
			sub.mpVertices = new RgmVertex[sub.minumverts];
			sub.mname = chunkreader.GetString(iname);
			mdl->mSubMeshMap[sub.mname] = & sub;
			for( int iv=0; iv<sub.minumverts; iv++ )
			{	RgmVertex& vtx = sub.mpVertices[iv];	
				ModelDataStream->GetItem(vtx.pos);
				ModelDataStream->GetItem(vtx.nrm);
				float fu = fmod((vtx.pos.GetX())*0.01f,1.0f);
				float fv = fmod((vtx.pos.GetZ())*0.01f,1.0f);
				ModelDataStream->GetItem(vtx.uv);
				vtx.uv.SetX(fu);
				vtx.uv.SetY(fv);
			}
			int inumtotp;
			HeaderStream->GetItem(sub.minumtris);
			HeaderStream->GetItem(inumtotp);
			sub.mtriangles = new RgmTri[sub.minumtris]; 
			for( int ip=0; ip<sub.minumtris; ip++ )
			{	RgmTri& tri = sub.mtriangles[ip];
				int inumv;
				HeaderStream->GetItem(inumv);
				OrkAssert(inumv==3);
				int ivA, ivB, ivC;
				ModelDataStream->GetItem(ivA);
				ModelDataStream->GetItem(ivB);
				ModelDataStream->GetItem(ivC);
				tri.mpv0 = sub.mpVertices+ivA;
				tri.mpv1 = sub.mpVertices+ivB;
				tri.mpv2 = sub.mpVertices+ivC;
				tri.Compute();

			}
			sub.mGeoSet->GetAABox() = AABox();
			sub.mGeoSet->GetAABox().BeginGrow();
			for( int ip=0; ip<sub.minumtris; ip+=4 ) //ip++ )
			{	const RgmTri& tri = sub.mtriangles[ip];
				const RgmVertex* v0 = tri.mpv0;
				const RgmVertex* v1 = tri.mpv1;
				const RgmVertex* v2 = tri.mpv2;
				RaytTriangle* prim = new RaytTriangle( v0, v1, v2 );
				prim->mRgmPoly = & tri;
				prim->SetMaterial( pmaterial );
				prim->SetBakeShader( pshader );
				sub.mGeoSet->AddPrimitive( prim );
				sub.mGeoSet->GetAABox().Grow(v0->pos);
				sub.mGeoSet->GetAABox().Grow(v1->pos);
				sub.mGeoSet->GetAABox().Grow(v2->pos);
				mdl->mAABox.Grow(v0->pos);
				mdl->mAABox.Grow(v1->pos);
				mdl->mAABox.Grow(v2->pos);

			}
			sub.mGeoSet->GetAABox().EndGrow();
		}
	}
	mdl->mAABox.EndGrow();
	return mdl;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Engine::Engine()
{
	mScene = new Scene();
	//KdTree::SetMemoryManager( new MManager() );
	mMod = new int[64];
	mMod = (int*)((((unsigned long)mMod) + 32) & (0xffffffff - 31));
	mMod[0] = 0, mMod[1] = 1, mMod[2] = 2, mMod[3] = 0, mMod[4] = 1;
	//m_Stack = new kdstack[64];
	//m_Stack = (kdstack*)((((unsigned long)m_Stack) + 32) & (0xffffffff - 31));
}

///////////////////////////////////////////////////////////////////////////////

Engine::~Engine()
{
	delete mScene;
}

///////////////////////////////////////////////////////////////////////////////

void Engine::SetTarget( RayPixel* dest, int w, int h )
{
	// set pixel buffer address & size
	mDest = dest;
	miW = w;
	miH = h;
}

///////////////////////////////////////////////////////////////////////////////

static float AreaOfTri( const CVector3& A, const CVector3& B, const CVector3& C )
{
	float farea = (A-C).Cross(B-C).Mag() * 0.5f;
	return farea;
}

///////////////////////////////////////////////////////////////////////////////

const Primitive* Engine::Raytrace( const Ray3& ray, const int irecdepth, const float irindex, CVector3& acc, float& dist )
{
	const Primitive* prim = 0;

	////////////////////////////////////////////////////
	// find the nearest intersection
	////////////////////////////////////////////////////

	acc = CVector3(0.1f,0.1f,0.2f);
	CVector3 P;
	bool bhit = GetScene()->GetFixedGrid()->FindNearest( ray, dist, P, prim );
	if (false==bhit) return 0;

/*	const RgmTri& tri = *prim->mRgmPoly;

	////////////////////////////////////////////////////
	// Compute Barycentric
	////////////////////////////////////////////////////

	const CVector3& NA = tri.mpv0->nrm;
	const CVector3& NB = tri.mpv1->nrm;
	const CVector3& NC = tri.mpv2->nrm;
	const CVector3& A = tri.mpv0->pos;
	const CVector3& B = tri.mpv1->pos;
	const CVector3& C = tri.mpv2->pos;
		
	float PBC = ( AreaOfTri(P,B,C) );
	float PCA = ( AreaOfTri(P,C,A) );
	float PAB = ( AreaOfTri(P,A,B) );
	float ABC = tri.mArea;
	
	float a = PBC/ABC;
	float b = PCA/ABC;
	float c = 1.0f - a - b;

	//////////////////////////
	// Interpolated Normal
	//////////////////////////
	
	CVector3 N = (NA*a+NB*b+NC*c);

	////////////////////////////////////////////////////
	// lighting
	////////////////////////////////////////////////////

	CVector3 LightPos = CVector3(76,-15,-900);
	CVector3 LMP = (LightPos-P);
	CVector3 LightDir = LMP.Normal();

	float fdot = N.Dot(LightDir);

	if( fdot<0.0f ) fdot=0.0f;

	acc = CVector3(fdot,fdot,fdot);

	/////////////////////////////////////////////
	// backfacing to light automatically black
	/////////////////////////////////////////////
	if( fdot <= 0.0f )
	{
		acc = CVector3::Black();
		return prim;
	}
	/////////////////////////////////////////////
	// perform shadowing test
	/////////////////////////////////////////////
	else
	{
		const float shadowbias = 0.1f;
		Ray3 RayToLight( P+LightDir*shadowbias, LightDir );
		const Primitive* primL = 0;

		float shadowdist = 100000.0f;
		CVector3 ShadowPos;
		bool bshadowhit = GetScene()->GetFixedGrid()->FindNearest( RayToLight, shadowdist, ShadowPos, primL );

		if( bshadowhit )
		{
			float disttolight = LMP.Mag();
			if( shadowdist<disttolight )
			{	acc *= 0.5f;
				return primL;
			}
		}
	}*/

	return prim;
}

///////////////////////////////////////////////////////////////////////////////

static inline int round(float a)
{
	int iret = (a>0) ? int(a + .5f) : int(a-.5f);
	return iret;
}

///////////////////////////////////////////////////////////////////////////////

void Engine::DrawSpan(	const BakeShader& shader,
						int x1, int y1, const BakeShadowFragment& d1, 
						int x2, int y2, const BakeShadowFragment& d2,
						SpanCtx& ctx,
						orkset<SpanFragment>* output )
{
	OrkAssert( x2-x1 >= 0 && x2-x1 >= y2-y1 && y2-y1 >= 0);

	bool horizontal = y1==y2;
	bool diagonal = (y2-y1)==(x2-x1);

	float scale = 1.0 / (x2 - x1);

	BakeShadowFragment d = d1;
	BakeShadowFragment d_step = (d2 - d1) * scale * .99999f;

	int y = y1; 
	float yy = y1;
	float y_step = (y2 - y1) * scale;

	for(int x = x1; x <= x2; ++x, d += d_step)
	{
		int X(x);
		int Y(y);
		if(ctx.swap_xy)
		{
			std::swap(X,Y);
		}
		if(ctx.flip_y)
		{
			Y = -Y; 
		}
		if(output)
		{
			output->insert( SpanFragment(X, Y, d) );
		}
		else
		{
			int ipix = (Y*miW)+X;
			mFragments[ipix] = d;
			shader.Compute( X,Y );
		}

		if( ! horizontal )
		{
			y = diagonal ? (y+1) : int(round( yy += y_step ));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void Engine::DrawSpanE( const BakeShader& shader,
						int x1, int y1, BakeShadowFragment c1, 
						int x2, int y2, BakeShadowFragment c2, 
						SpanCtx& ctx,
						orkset<SpanFragment>* output )
{	if(x1 > x2)							
	// force left to right.
	{	std::swap(x1, x2);
		std::swap(y1, y2);
		std::swap(c1, c2);
	}
	if( (ctx.flip_y = y1 > y2) )			
	// force down to up.
	{	y1 = -y1;
		y2 = -y2;
	}
	if( (ctx.swap_xy = y2-y1 > x2-x1) )		
	// force line with <= 45 deg to x-axis.
	{	std::swap(x1, y1);
		std::swap(x2, y2);
	}
	DrawSpan(	shader,
				x1, y1, c1,
				x2, y2, c2,
				ctx, output);
}

///////////////////////////////////////////////////////////////////////////////

void Engine::RasterizeTriangle(	const BakeShader& shader,
								int x1, int y1, const BakeShadowFragment& d1, 
								int x2, int y2, const BakeShadowFragment& d2, 
								int x3, int y3, const BakeShadowFragment& d3 )
  
{	SpanCtx ctx;

	orkset<SpanFragment> scanned_pnts;
	// sorted in y val, and in x val if y val the same.
	DrawSpanE(shader, x1, y1, d1,  x2, y2, d2,  ctx, &scanned_pnts);
	DrawSpanE(shader, x2, y2, d2,  x3, y3, d3,  ctx, &scanned_pnts);
	DrawSpanE(shader, x3, y3, d3,  x1, y1, d1,  ctx, &scanned_pnts);
	int cur_yval = miH / 2;			// initialize to an invalid value.
	orkset<SpanFragment> same_yval;	// of the scanned result.
	for (orkset<SpanFragment>::iterator it = scanned_pnts.begin(); it != scanned_pnts.end(); ++it)
	{	int y = it->y;
		if(y != cur_yval) 
		{	if(same_yval.size())
			{	orkset<SpanFragment>::iterator it1 = same_yval.begin();
				orkset<SpanFragment>::iterator it2 = --same_yval.end();
				DrawSpanE(	shader,
							it1->x, cur_yval, it1->mData, 
							it2->x, cur_yval, it2->mData, 
							ctx, 0);
				same_yval.clear();
			}
			cur_yval = y;
		}
		same_yval.insert(*it);
	}
}

///////////////////////////////////////////////////////////////////////////////

void Engine::InitRender( CVector3& eye, CVector3& target )
{
	///////////////////////////////////////
	// calculate lookat matrix
	///////////////////////////////////////

	CVector3 zdir = (target-eye).Normal();
	CVector3 cross = zdir.Cross(CVector3(0.0f,1.0f,0.0f));
	if( cross.Mag()==0.0f ) cross=zdir.Cross(CVector3(1.0f,0.0f,0.0f));
	if( cross.Mag()==0.0f ) cross=zdir.Cross(CVector3(0.0f,0.0f,1.0f));

	CVector3 up = zdir.Cross(cross);

	CMatrix4 mtxLookat;
	mtxLookat.LookAt( eye, target, up );

	///////////////////////////////////////
	// set projection matrix
	///////////////////////////////////////

	CMatrix4 mtxP;

	float faspect = float(GetWidth())/float(GetHeight());
	mtxP.Perspective( 45, faspect, 1.0f, 10000.0f );
	//mtxP.Ortho( -1, 1, -1, 1, 1, 10000  );

	CVector3 vo(0.0f,0.0f,0.0f);
	CVector3 vpn(0.0f,0.0f,1.0f);
	CVector3 vpf(0.0f,0.0f,10000.0f);

	CVector3 pvo = vo.Transform(mtxP);
	CVector3 pvn = vpn.Transform(mtxP);
	CVector3 pvf = vpf.Transform(mtxP);

	///////////////////////////////////////

	//CMatrix4 mtxViewport;
	//float xs = 2.0f/
	//mtxViewport.Scale(
	///////////////////////////////////////
	
	const CMatrix4 matVP = (mtxLookat*mtxP);
	CMatrix4 matIVP;
	matIVP.GEMSInverse(matVP);

	// set eye and screen plane position
	mEye = eye;
	
	float x1 = 0.0f; //-m_Width;//*0.5f;
	float x2 = miW;
	float y1 = 0.0f; //-m_Height;
	float y2 = miH;

	SRect VP( x1, y1, x2, y2 );

	CVector4 Vxcyc( (x1+x2)*0.5f, (y1+y2)*0.5f, 0.0f, 1.0f );
	CVector4 Vx0y0( x1, y1, 0.0f, 1.0f );
	CVector4 Vx1y0( x2, y1, 0.0f, 1.0f );
	CVector4 Vx1y1( x2, y2, 0.0f, 1.0f );
	CVector4 Vx0y1( x1, y2, 0.0f, 1.0f );

	CVector3 Center, CenterDir;

	CMatrix4::UnProject( Vxcyc, matIVP, VP, Center );
	CMatrix4::UnProject( Vx0y0, matIVP, VP, mCornerTL );
	CMatrix4::UnProject( Vx1y0, matIVP, VP, mCornerTR );
	CMatrix4::UnProject( Vx1y1, matIVP, VP, mCornerBR );
	CMatrix4::UnProject( Vx0y1, matIVP, VP, mCornerBL );

	CenterDir = (Center-eye).Normal();

	// calculate screen plane interpolation vectors
	mDX = (mCornerTR - mCornerTL) * (1.0f / miW);
	mDY = (mCornerBL - mCornerTL) * (1.0f / miH);

}

///////////////////////////////////////////////////////////////////////////////

const Primitive* Engine::RenderRay( CVector3 screen_pos, CVector3& acc )
{
	AABox e = mScene->GetExtends();
	CVector3 dir = (screen_pos - mEye);
	dir.Normalize();
	Ray3 r( mEye, dir );
	float dist = 100000.0f;
	const Primitive* prim = Raytrace( r, 1, 1.0f, acc, dist );
	return prim;
}

///////////////////////////////////////////////////////////////////////////////

struct RenderingJobCtx
{
	Engine*			mpEngine;
	int				miCore;
	int				miNumCores;
	int				miNumRays;
};

///////////////////////////////////////////////////////////////////////////////

void* RenderingJobThread(void *vptr_args)
{
	RenderingJobCtx* mpCtx = (RenderingJobCtx*)	vptr_args;

	mpCtx->miNumRays = 0;
	int ih = mpCtx->mpEngine->GetHeight();
	int iw = mpCtx->mpEngine->GetWidth();

	const CVector3& cTL = mpCtx->mpEngine->CornerTL();
	const CVector3& cTR = mpCtx->mpEngine->CornerTR();
	const CVector3& cBL = mpCtx->mpEngine->CornerBL();
	const CVector3& cBR = mpCtx->mpEngine->CornerBR();

	const CVector3& cQX = (cTR-cTL)*1.0f/float(iw);
	const CVector3& cQY = (cBL-cTL)*1.0f/float(ih);

	const Jitterer my_jitter( kOS, kJITTER, cQX, cQY );


	for ( int y = mpCtx->miCore; y < ih; y+=mpCtx->miNumCores )
	{
		if( y%64 == mpCtx->miCore ) orkprintf( "core<%d> y<%d>\n", mpCtx->miCore, y );
		
		float fy = float(y)/float(ih);
		CVector3 lSC, rSC;
		lSC.Lerp( cTL, cBL, fy );
		rSC.Lerp( cTR, cBR, fy );
		for ( int x = 0; x < iw; x++ )
		{	float fx = float(x)/float(iw);
			CVector3 screen_pos, jittered_pos;
			screen_pos.Lerp( lSC, rSC, fx );
			
			CVector3 acc( 0, 0, 0 );
			for( int isamp=0; isamp<my_jitter.miNumSamples; isamp++ )
			{
				jittered_pos = screen_pos+my_jitter.GetSample(isamp);
							
				CVector3 sample( 0, 0, 0 );

				const Primitive* prim = mpCtx->mpEngine->RenderRay( jittered_pos, sample );
				acc += sample;
				mpCtx->miNumRays++;
			}

			acc = acc*(1.0f/float(my_jitter.miNumSamples));
			int red, green, blue;
			red = (int)(acc.GetX() * 256);
			green = (int)(acc.GetY() * 256);
			blue = (int)(acc.GetZ() * 256);
			if (red > 255) red = 255;
			if (green > 255) green = 255;
			if (blue > 255) blue = 255;
			if (red < 0) red = 0;
			if (green < 0) green = 0;
			if (blue < 0) blue = 0;
			int ipix = (y*iw)+((iw-1)-x);
			RayPixel& rp = mpCtx->mpEngine->RefPixel(ipix);
			rp.r = red;
			rp.g = green;
			rp.b = blue;

		}
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool Engine::Render( const AABox& bbox, const std::string& OutputName )
{		
	RayPixel* pixels = new RayPixel[kIW*kIW];
	memset( (void*) pixels, 0, sizeof(RayPixel)*kIW*kIW );

	CVector3 dim = (bbox.Max()-bbox.Min());
	CVector3 ctr = CVector3(0.0f,275.0f,0.0f)+(bbox.Min()+bbox.Max())*0.5f;
	CVector3 eye = ctr+CVector3(0.0f,30.0f,75.0f);

	SetTarget( pixels, kIW, kIW );

	GetScene()->InitScene(bbox);
	InitRender(eye, ctr);

	/////////////////////////////////////////////////////////////////

	int inumcores = GetNumCores();
	orkvector<RenderingJobCtx*>	JobCtxVect;
#if 0
	orkvector<pthread_t>		ThreadVect;
	f64 ftimeA = CSystem::GetRef().GetSystemRelTime(  );
	for( int ic=0; ic<inumcores; ic++ )
	{
		RenderingJobCtx* ctx = new RenderingJobCtx;
		ctx->mpEngine = this;
		ctx->miNumCores = inumcores;
		ctx->miCore = ic;

		pthread_t job_thread;
		if (pthread_create(&job_thread, NULL, RenderingJobThread, (void*)ctx) != 0)
		{
			OrkAssert(false);
		}

		ThreadVect.push_back(job_thread);
		JobCtxVect.push_back(ctx);
	}
	for( orkvector<pthread_t>::iterator it=ThreadVect.begin(); it!=ThreadVect.end(); it++ )
	{
		pthread_t job = (*it);
		pthread_join(job, NULL);
	}
	int inumrays = 0;
	for( orkvector<RenderingJobCtx*>::iterator it=JobCtxVect.begin(); it!=JobCtxVect.end(); it++ )
	{
		RenderingJobCtx* ctx = *it;
		inumrays += ctx->miNumRays;
	}
	f64 ftimeB = CSystem::GetRef().GetSystemRelTime(  );
	f64 ftime = ftimeB-ftimeA;
	float frayspersec = float(inumrays)/ftime;
	orkprintf( "Rays<%d> Time<%f? RaysPerSec<%f>\n", inumrays, ftime,  frayspersec );

	/////////////////////////////////////////////////////////////////
	GetScene()->ExitScene();
#endif

/*	ilInit();
	ILuint image;
	ilGenImages(1, &image);
	ilBindImage(image);
	if (!ilTexImage(kIW, kIW, 1, 3, IL_RGB, IL_UNSIGNED_BYTE, pixels))
	{
		ILenum error = ilGetError();
		orkprintf("Failed to create image (%04X)\n", error);
	}

	ilEnable(IL_FILE_OVERWRITE);
	if (!ilSaveImage(OutputName.c_str()))
	{
		ILenum error = ilGetError();
		orkprintf("Failed to save to %s (%04X)\n", OutputName.c_str(), error);
	}
*/
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct BakingJobCtx
{
	Engine*			mpEngine;
	int				miCore;
	int				miNumCores;
	int				miNumRays;
};

///////////////////////////////////////////////////////////////////////////////

void* BakingJobThread( void* pval )
{
	BakingJobCtx* mpCtx = (BakingJobCtx*) pval;

	const orkmap<std::string,const RgmGeoSet*>& geosets = mpCtx->mpEngine->GetScene()->GetGeoSets();
	
	for( orkmap<std::string,const RgmGeoSet*>::const_iterator it=geosets.begin(); it!=geosets.end(); it++ )
	{
		const std::string& geoname = it->first;
		if( geoname.find( "raster_" ) != std::string::npos )
		{
			const RgmGeoSet* RasterSet = it->second;

			int inumtri = RasterSet->NumPrimitives();
			int iw = mpCtx->mpEngine->GetWidth();
			int ih = mpCtx->mpEngine->GetHeight();
			int itrictr = inumtri/mpCtx->miNumCores;

			int icntdwn = (inumtri>>8); // 

			if( 0 == icntdwn ) icntdwn = 1;

			for( int ip=mpCtx->miCore; ip<inumtri; ip+=mpCtx->miNumCores )
			{
				if( itrictr%icntdwn == 0 ) orkprintf( "core<%d> numleft<%d> geoname<%s>\n", mpCtx->miCore, itrictr, geoname.c_str() );
				itrictr--;
				const Primitive* prim = RasterSet->GetPrimitive(ip);
				prim->Rasterize( mpCtx->mpEngine );
			}		
		}
	}
	return 0;
}

bool Engine::Bake( const AABox& bbox, const std::string& OutputName )
{		
	int iKKos = kOS;

	int idim = (iKKos*2)+1;
	int iNumSamples = idim*idim;

	int idiv = (kOS==0) ? 1 : kOS;
	int iKIW = idim*kIW;


	RayPixel* pixels = new RayPixel[iKIW*iKIW];
	memset( (void*) pixels, 0, sizeof(RayPixel)*iKIW*iKIW );
	mFragments = new BakeShadowFragment[iKIW*iKIW];
	memset( (void*) mFragments, 0, sizeof(BakeShadowFragment)*kIW*kIW );

	CVector3 dim = (bbox.Max()-bbox.Min());
	CVector3 ctr = (bbox.Min()+bbox.Max())*0.5f;
	CVector3 eye = ctr+CVector3(0.0f,0.0f,75.0f);

	SetTarget( pixels, iKIW, iKIW );

	orkprintf( "Initializing Scene\n" );

	GetScene()->InitScene(bbox);
	InitRender(eye, ctr);

	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////
	//f64 ftimeA = CSystem::GetRef().GetSystemRelTime(  );
	{
		int inumcores = GetNumCores();
		orkvector<BakingJobCtx*>	JobCtxVect;
#if 0
		orkvector<pthread_t>	ThreadVect;
		for( int ic=0; ic<inumcores; ic++ )
		{
			BakingJobCtx* ctx = new BakingJobCtx;
			ctx->mpEngine = this;
			ctx->miNumCores = inumcores;
			ctx->miCore = ic;

			pthread_t job_thread;
			if (pthread_create(&job_thread, NULL, BakingJobThread, (void*)ctx) != 0)
			{
				OrkAssert(false);
			}

			ThreadVect.push_back(job_thread);

			JobCtxVect.push_back(ctx);
		}
		for( orkvector<pthread_t>::iterator it=ThreadVect.begin(); it!=ThreadVect.end(); it++ )
		{
			pthread_t job = (*it);
			pthread_join(job, NULL);
		}
		int inumrays = 0;
		//for( orkvector<BakingJobCtx*>::iterator it=JobCtxVect.begin(); it!=JobCtxVect.end(); it++ )
		//{
		//	RenderingJobCtx* ctx = *it;
		//	inumrays += ctx->miNumRays;
		//}		
		#endif
	}
	//f64 ftimeB = CSystem::GetRef().GetSystemRelTime(  );
	f64 ftime = 1.0; //ftimeB-ftimeA;
	int inrays = int(giNumRays);

	float frayspersec = float(inrays)/ftime;
	orkprintf( "Rays<%d> Time<%f> RaysPerSec<%f>\n", inrays, ftime,  frayspersec );
	giNumRays=0;
	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////
	GetScene()->ExitScene();
	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////
	orkprintf( "growing uv shells...\n" );
	static const int kgrowamt = 7;
	for( int iy=0; iy<iKIW; iy++ )
	{	for( int ix=0; ix<iKIW; ix++ )
		{	int ipix = (iy*iKIW)+ix;
			RayPixel& pixel = pixels[ipix];
			//////////////////////////////
			// we will only ever grow pixels that have not been written to
			if( pixel.a == 0 )
			{	float faccr = 0.0f;
				float faccg = 0.0f;
				float faccb = 0.0f;
				float fnums = 0.0f;
				for( int oy=-kgrowamt; oy<=kgrowamt; oy++ )
				{	int iny = iy+oy;
					if( iny<0 ) continue;
					if( iny>=iKIW ) continue;
					for( int ox=-kgrowamt; ox<=kgrowamt; ox++ )
					{	int inx = ix+ox;
						if( ox==0 && oy==0 ) continue;
						if( inx<0 ) continue;
						if( inx>=iKIW ) continue;
						int ipix2 = (iny*kIW)+inx;
						const RayPixel& opix = pixels[ipix2];
						if( opix.a != 0 )
						{	faccr += float(opix.r);
							faccg += float(opix.g);
							faccb += float(opix.b);
							fnums += 1.0f;
						}
					}
				}
				if( fnums != 0.0f )
				{	pixel.r = u8(faccr/fnums);
					pixel.g = u8(faccg/fnums);
					pixel.b = u8(faccb/fnums);
				}
			}
		}	
	}
	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////

/*	boost::gil::rgb8_image_t img(iKIW,iKIW);
	boost::gil::rgb8_view_t myview = boost::gil::view(img);
 
	for( int iy=0; iy<iKIW; iy++ )
	{
		for( int ix=0; ix<iKIW; ix++ )
		{
			int ipix = (iy*iKIW)+ix;
			const RayPixel& pixel = pixels[ipix];
			boost::gil::rgb8_pixel_t mypix( pixel.r, pixel.g, pixel.b );
			myview(ix,iy)=mypix;
		}
	}

	orkprintf( "blurring image...\n" );

	orkprintf( "saving image...\n" );

	//boost::gil::png_write_view(OutputName.c_str(), boost::gil::view(img));
	
	delete[] pixels;
	delete[] mFragments;
	delete GetScene()->GetFixedGrid();
	*/

	return true;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Scene::AddGeoSet( const std::string& name, RgmGeoSet* pset )
{
	mGeoSets[name] = pset;
}
void Scene::RemoveGeoSet( const std::string& name )
{
	orkmap<std::string,const RgmGeoSet*>::iterator it=mGeoSets.find(name);
	if( it != mGeoSets.end() )
	{
		const RgmGeoSet* rval = it->second;
		mGeoSets.erase(it);
		if( rval )
		{
			delete rval;
		}
	}
}

const RgmGeoSet* Scene::FindGeoSet( const std::string& name ) const
{
	const RgmGeoSet* rval = 0;
	orkmap<std::string,const RgmGeoSet*>::const_iterator it=mGeoSets.find(name);
	if( it != mGeoSets.end() ) rval=it->second;
	return rval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}
