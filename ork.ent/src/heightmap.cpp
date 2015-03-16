///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/heightmap.h>
#include <ork/file/file.h>
#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

 sheightmap sheightmap::gdefhm(3,3);

///////////////////////////////////////////////////////////////////////////////
sheightmap::sheightmap( int isx, int isz ) 
	: miGridSizeX(0)
	, miGridSizeZ(0)
	, mHeightData()
	, mMin( CFloat::TypeMax() )
	, mMax( -CFloat::TypeMax() )
	, mRange(0.0f)
	, mMutex( "shmMutex" )
	, mfWorldSizeX( 1.0f )
	, mfWorldSizeZ( 1.0f )
	, mIndexToUnitX(1.0f)
	, mIndexToUnitZ(1.0f)
	, mWorldHeight(1.0f)

{
	SetGridSize(isx,isz);
}
void sheightmap::ResetMinMax()
{
	mMin = CFloat::TypeMax();
	mMax = -CFloat::TypeMax();
	mRange = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////
void sheightmap::SetGridSize( int iw, int ih )
{
	mMutex.Lock();
	{
		ResetMinMax();
		miGridSizeX = iw;
		miGridSizeZ = ih;
		mHeightData.clear();
		mHeightData.resize(iw*ih);
		float fiiw( 1.0f/ float(iw) );
		float fiih( 1.0f/ float(ih) );
		mIndexToUnitX = fiiw;
		mIndexToUnitZ = fiih;
	}
	mMutex.UnLock();
}
///////////////////////////////////////////////////////////////////////////////
sheightmap::~sheightmap()
{
}
///////////////////////////////////////////////////////////////////////////////
void sheightmap::SetHeight( int ix, int iz, float fh )
{
	int iaddr = CalcAddress(ix,iz);
	mHeightData[ iaddr ] = fh;
	if( fh > mMax ) mMax=fh;
	if( fh < mMin ) mMin=fh;
	mRange = (mMax-mMin);
}
///////////////////////////////////////////////////////////////////////////////
float sheightmap::GetHeight( int ix, int iz ) const
{
	int iaddr = CalcAddress(ix,iz);
	return mHeightData[ iaddr ];
}

///////////////////////////////////////////////////////////////////////////////

CVector3 sheightmap::XYZ( int iX, int iZ ) const
{
	int isx = GetGridSizeX();
	int isz = GetGridSizeZ();

	//float fminy = GetMinHeight();
	//float frngy = GetHeightRange();

	int ixd2 = isx>>1;
	int izd2 = isz>>1;

	float fx = (float(iX-ixd2)*mIndexToUnitX)*mfWorldSizeX;
	float fz = (float(iZ-izd2)*mIndexToUnitZ)*mfWorldSizeZ;

	float fy = GetHeight(iX,iZ)*mWorldHeight;
	auto rval =  CVector3( fx, fy, fz );
	//printf( "XYZ<%d %d> - <%f %f %f>\n", iX, iZ, fx, fy, fz );
	return rval;
}

CVector3 sheightmap::ComputeNormal( int ix1, int iz1 ) const
{
	int isx = GetGridSizeX();
	int isz = GetGridSizeZ();

	int ix0 = (ix1-1); if( ix0<0 ) ix0=0;
	int iz0 = (iz1-1); if( iz0<0 ) iz0=0;
	
	int ix2 = (ix1+1); if( ix2>(isx-1) ) ix2=isx-1;
	int iz2 = (iz1+1); if( iz2>(isz-1) ) iz2=isz-1;

	//CVector3 hscale(1.0f,mWorldHeight,1.0f);

	CVector3 VC = XYZ(ix1,iz1);

	CVector3 d0 = (XYZ(ix0,iz0)-VC).Normal();
	CVector3 d1 = (XYZ(ix1,iz0)-VC).Normal();
	CVector3 d2 = (XYZ(ix2,iz0)-VC).Normal();
	CVector3 d3 = (XYZ(ix2,iz1)-VC).Normal();
	CVector3 d4 = (XYZ(ix2,iz2)-VC).Normal();
	CVector3 d5 = (XYZ(ix1,iz2)-VC).Normal();
	CVector3 d6 = (XYZ(ix0,iz2)-VC).Normal();
	CVector3 d7 = (XYZ(ix0,iz1)-VC).Normal();

	CVector3 c0 = d0.Cross(d1);
	CVector3 c1 = d1.Cross(d2);
	CVector3 c2 = d2.Cross(d3);
	CVector3 c3 = d3.Cross(d4);
	CVector3 c4 = d4.Cross(d5);
	CVector3 c5 = d5.Cross(d6);
	CVector3 c6 = d6.Cross(d7);
	CVector3 c7 = d7.Cross(d0);

	CVector3 vdx = -(c0+c1+c2+c3+c4+c5+c6+c7).Normal();

	return vdx;
}

///////////////////////////////////////////////////////////////////////////////

bool sheightmap::CalcClosestAddress( const CVector3& to, float& outx, float& outz ) const
{
	bool bOK = false;

	/*float isx = GetGridSizeX();
	float isz = GetGridSizeZ();

	CVector3 vnorm = (to-Min());

	float fX = (vnorm.GetX()/Range().GetX());
	float fZ = (vnorm.GetZ()/Range().GetZ());

	float itX = fX*float(isx);
	float itZ = fZ*float(isz);

	if( itX>=0.0f && itX<isx )
	{
		outx = itX; //+0.5f;
		if( itZ>=0.0f && itZ<isz )
		{
			outz = itZ; //+0.5f;

			bOK = true;
		}
	}

	if( false == bOK )
	{
		if( itX<0.0f ) outx = -1.0f;
		if( itX>=isx ) outx = -2.0f;
		if( itZ<0.0f ) outz = -1.0f;
		if( itZ>=isz ) outz = -2.0f;
	}*/

	return bOK;
}

///////////////////////////////////////////////////////////////////////////////

CVector3 sheightmap::Min() const 
{
	CVector3 ret( mfWorldSizeX*-0.5f, mMin, mfWorldSizeZ*-0.5f );
	return ret;
}
CVector3 sheightmap::Max() const 
{
	CVector3 ret( mfWorldSizeX*0.5f, mMax, mfWorldSizeZ*0.5f );
	return ret;
}
CVector3 sheightmap::Range() const 
{
	CVector3 ret( mfWorldSizeX, mRange, mfWorldSizeZ );
	return ret;
}

void sheightmap::ReadSurface(  bool bfilter, const CVector3& xyz, CVector3& pos, CVector3& nrm ) const
{/*
	float fiterX, fiterZ;
	bool bOK = CalcClosestAddress( xyz, fiterX, fiterZ );
	int iterX = int(fiterX);
	int iterZ = int(fiterZ);

	int igsizX = GetGridSizeX()-2;
	int igsizZ = GetGridSizeZ()-2;

	//////////////////////////////////////////
	nrm = CVector3(0.0f,1.0f,0.0f);
	pos = CVector3(xyz.GetX(),xyz.GetY()+50.0f,xyz.GetZ());
	//////////////////////////////////////////
	if( false == bOK )
	{
		//nrm = CVector3(0.0f,1.0f,0.0f);
		//pos = CVector3(xyz.GetX(),xyz.GetY()+50.0f,xyz.GetZ());
		if( fiterX==-1.0f )
		{
			nrm = CVector3(1.0f,0.0f,0.0f);
		}
		else if( fiterX==-2.0f)
		{
			nrm = CVector3(-1.0f,0.0f,0.0f);
		}
		//////////////////////////////////////////
		if( fiterZ==-1.0f )
		{
			nrm = CVector3(0.0f,0.0f,1.0f);
		}
		else if( fiterZ==-2.0f)
		{
			nrm = CVector3(0.0f,0.0f,-1.0f);
		}
	}
	//////////////////////////////////////////
	else if( iterX >= 2 && iterZ >= 2 && iterX < igsizX && iterZ < igsizZ )
	//////////////////////////////////////////
	{
		if( bfilter )
		{
			CVector3 terp_xyz[4];
			CVector3 terp_nrm[4];
			for( int is=0; is<4; is++ )
			{
				int isx = is&1;
				int isz = is>>1;
				terp_xyz[is] = XYZ( iterX+isx, iterZ+isz );
				terp_nrm[is] = ComputeNormal( iterX+isx, iterZ+isz );
			}
			
			float flerpx = fiterX-float(iterX);
			float flerpz = fiterZ-float(iterZ);

			CVector3 pza;	pza.Lerp( terp_xyz[0],terp_xyz[2], flerpz );
			CVector3 pzb;	pzb.Lerp( terp_xyz[1],terp_xyz[3], flerpz );

			pos.Lerp( pza, pzb, flerpx );

			CVector3 nra;	nra.Lerp( terp_nrm[0],terp_nrm[2], flerpz );
			CVector3 nrb;	nrb.Lerp( terp_nrm[1],terp_nrm[3], flerpz );

			nrm.Lerp( nra, nrb, flerpx );

			nrm.Normalize();
		}
		else
		{
			//int iX = int( float(iterX)+0.5f );
			//int iZ = int( float(iterZ)+0.5f );
			pos = XYZ(iterX,iterZ);

			//printf( "ReadSurfacenofilt to_x<%f> to_z<%f> iterX<%d> iterZ<%d> pos<%f %f %f> bOK<%d>\n", 
			//xyz.GetX(), xyz.GetZ(),
			//iterX, iterZ,
			//pos.GetX(), pos.GetY(), pos.GetZ(),
			//int(bOK) );

			nrm = ComputeNormal(iterX,iterZ);
		}
	}
*/
}

bool sheightmap::Load( const ork::file::Path& pth )
{
	auto abs_path = pth.ToAbsolute();

	bool bexists = CFileEnv::DoesFileExist( abs_path );

	if( bexists )
	{
		ImageInput* oiio_img = ImageInput::create(abs_path.c_str());
		if (! oiio_img)
			return false;
		ImageSpec spec;
		oiio_img->open (abs_path.c_str(), spec);
		int xres = spec.width;
		int yres = spec.height;
		int num_channels = spec.nchannels;
		auto pu16 = new uint16_t [xres*yres*num_channels];

		printf( "xres<%d> yres<%d> numch<%d>\n", xres, yres, num_channels );

		bool read_ok = oiio_img->read_image (TypeDesc::UINT16, pu16);
		oiio_img->close ();
		delete oiio_img;
		
		OrkAssert( read_ok );

		int inumpix = (xres*yres);

		//////////////////////////////////////////
		// fill in heightmap
		//////////////////////////////////////////

		ork::EndianContext ectx;
		ectx.mendian = ork::EENDIAN_LITTLE;

		U16 umin = 0xffff;
		U16 umax = 0x0000;
		for( int ip=0; ip<inumpix; ip++ )
		{
			U16 upix = pu16[ip];
			ork::swapbytes_dynamic<U16>( upix );
			if( upix>umax ) umax=upix;
			if( upix<umin ) umin=upix;
		}
		U16 urange = (umax-umin);
		urange = (urange==0) ? 1 : urange;

		this->SetGridSize( xres, yres );

		this->GetLock().Lock();
	
		{
			for( int iz=0; iz<yres; iz++ )
			{
				for( int ix=0; ix<xres; ix++ )
				{
					int iaddr = (iz*xres)+ix;

					U16 upix = (pu16[iaddr]-umin);
					ork::swapbytes_dynamic<U16>( upix );

					float fh = (float(upix)/float(urange))-0.5f;

					//printf( "FH<%f>\n", fh );
					this->SetHeight( ix, iz, fh );
				}
			}
		}
		this->GetLock().UnLock();
	}

	return bexists;
}

static 
CVector4 GetGradientColor( float fin, const orkmap<float,CVector4>& gmap )
{
	CVector4 rval = CColor4::White();

	if( gmap.size()>=2 )
	{
		fin = 1.0f-fin;

		if( fin<0.0f ) fin = 0.0f;
		if( fin>1.0f ) fin = 1.0f;

		orkmap<float,CVector4>::const_iterator itu = gmap.lower_bound(fin);

		if( itu == gmap.end() )
		{
			itu--;
			rval = itu->second;
		}
		else
		{
			orkmap<float,CVector4>::const_iterator itl = itu;
			itl--;

			if( itl != gmap.end() )
			{
				float frange = itu->first - itl->first;
				float flerp = (fin-itl->first)/frange;

				rval.Lerp( itl->second, itu->second, flerp );
			}
			
		}
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
CVector4 GradientSet::Lerp( float fu, float fv ) const
{
	CVector3 lo = GetGradientColor( fu, *mGradientLo );
	CVector3 hi = GetGradientColor( fu, *mGradientHi );
	CVector3 result;
	result.Lerp( lo, hi, fv ); 
	return result;
}
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
