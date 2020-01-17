////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/file/path.h>
#if defined(USE_FCOLLADA)
#include <FCollada.h>
#include <FCDocument/FCDocument.h>
#include <FCDocument/FCDExtra.h>
#include <FCDocument/FCDGeometry.h>
#include <FCDocument/FCDGeometryMesh.h>
#include <FCDocument/FCDGeometryPolygons.h>
#include <FCDocument/FCDGeometryPolygonsInput.h>
#include <FCDocument/FCDGeometryPolygonsTools.h> // For Triagulate
#include <FCDocument/FCDGeometrySource.h>
#include <FCDocument/FCDGeometryInstance.h>
#include <FCDocument/FCDLibrary.h>
#include <FCDocument/FCDAsset.h>

#include <FCDocument/FCDAnimation.h>
#include <FCDocument/FCDAnimationChannel.h>
#include <FCDocument/FCDAnimationCurve.h>

#include <FCDocument/FCDController.h>
#include <FCDocument/FCDSkinController.h>

#include <FCDocument/FCDSceneNode.h>

#include <FUtils/FUString.h> // For fm::StingList

#include <FCDocument/FCDAnimationKey.h>

#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/collada/daeutil.h>

///////////////////////////////////////////////////////////////////////////////

using namespace ork::lev2;

namespace ork { namespace tool {

///////////////////////////////////////////////////////////////////////////////

CColladaAsset::~CColladaAsset()
{
	if( mDocument )
	{
		delete( mDocument );
	}
}

///////////////////////////////////////////////////////////////////////////////

CColladaAsset::EAssetType CColladaAsset::GetAssetType( const AssetPath & fname )
{
	CColladaAsset::EAssetType etype = CColladaAsset::ECOLLADA_END;
	File ColladaFile;
	ColladaFile.OpenFile( fname, EFM_READ );
	size_t isize = 0;
	ColladaFile.GetLength(isize);
	printf( "ColladaFile<%s> Size<%d>\n", fname.c_str(), int(isize) );
	//OrkAssert( isize >= 1024 );
	char* buffer = new char[isize+1];
	ColladaFile.Read( (void*) buffer, isize );
	buffer[isize] = 0;
	etype = CColladaAsset::ECOLLADA_MODEL;
	const char* AnimEq0 = strstr( buffer, "exportAnimations=0" );
	const char* AnimEq1 = strstr( buffer, "exportAnimations=1" );
	const char* AnimEq2 = strstr( buffer, "library_animations" );
	const char* GeomLib = strstr( buffer, "library_geometries" );

	printf( "AnimEq0<%p>\n", AnimEq0 );
	printf( "AnimEq1<%p>\n", AnimEq1 );
	printf( "AnimEq2<%p>\n", AnimEq2 );
	printf( "GeomLib<%p>\n", GeomLib );

	if( /*(nullptr==GeomLib)  &&*/ (AnimEq1 || AnimEq2) )
	{
		etype = CColladaAsset::ECOLLADA_ANIM;
	}
	OrkAssert( etype!=CColladaAsset::ECOLLADA_END );

	printf( "AssetType<%d>\n", int(etype) );

	return etype;
}

///////////////////////////////////////////////////////////////////////////////

bool CColladaAsset::LoadDocument(const AssetPath& fname)
{
	printf( "CColladaAsset::LoadDocument fname<%s>\n", fname.c_str() );
	FCollada::Initialize();
	mDocument = new FCDocument;

	AssetPath ActualPath = fname.ToAbsolute();

	ActualPath.SetExtension("dae");

	meAssetType = GetAssetType(ActualPath);

	bool bok = FCollada::LoadDocumentFromFile( mDocument, ActualPath.c_str() );

	mpColladaAsset = mDocument->GetAsset();
	mUnitsPerMeter = mpColladaAsset->GetUnitConversionFactor();
	std::string UnitName(mpColladaAsset->GetUnitName().c_str());

	if(ColladaExportPolicy::context() && ColladaExportPolicy::context()->mUnits != UNITS_ANY)
	{
		/*if(ColladaExportPolicy::context()->mUnits == UNITS_METER
				&& std::string("meter") != UnitName)
		{
			orkerrorlog("ERROR: Units must be in meters! Set your Maya preferences accordingly. (%s)\n", fname.c_str());
			FCollada::Release();
			return false;
		}
		else if(ColladaExportPolicy::context()->mUnits == UNITS_CENTIMETER
				&& std::string("centimeter") != UnitName)
		{
			orkerrorlog("ERROR: Units must be in centimeters! Set your Maya preferences accordingly. (%s)\n", fname.c_str());

			FCollada::Release();
			return false;
		}*/
	}
	FCollada::Release();

	return bok;

}

///////////////////////////////////////////////////////////////////////////////

CColladaModel * CColladaModel::Load( const AssetPath & fname )
{
	DaeReadOpts opts;

	CColladaModel *Model = new CColladaModel( fname.c_str(), opts );

	bool bok = Model->LoadDocument( fname );

	printf( "model loaded<%d>\n", int(bok) );

	bok &= (CColladaAsset::ECOLLADA_MODEL==Model->meAssetType);

	printf( "model loaded2<%d>\n", int(bok) );

	if( bok )
	{
		///////////////////////////
		// is it a model ?

		FCDGeometryLibrary* GeoLib = Model->mDocument->GetGeometryLibrary();

		printf( "has geolib<%p>\n", GeoLib );

		bok = false;

		if( GeoLib )
		{
			size_t inument =  GeoLib->GetEntityCount ();

			for( size_t ient=0; ient<inument; ient++ )
			{
				FCDGeometry *GeoObj = GeoLib->GetEntity(ient);
				if( GeoObj->IsMesh() )
				{
					bok = true;
				}
			}
		}

		///////////////////////////
		// is it an anim ?

		FCDAnimationLibrary *AnimLib = Model->mDocument->GetAnimationLibrary();
		int inument( AnimLib->GetEntityCount() );

		//if( inument ) bok=false;

		printf( "has animlib<%p>\n", AnimLib );

		/////////////////////////////////////
		if(bok) bok=Model->FindDaeMeshes();
		/////////////////////////////////////
		if(bok) bok=Model->ParseMaterialBindings();
		/////////////////////////////////////
		if(bok) bok=Model->ParseControllers();
		if(bok) bok=Model->BuildXgmSkeleton();
		/////////////////////////////////////
		if(bok) bok=Model->ParseGeometries();
		if(bok) bok=Model->BuildXgmTriStripModel();
		/////////////////////////////////////
	}

	int ibonespercluster = ColladaExportPolicy::context()->miNumBonesPerCluster;

	Model->mXgmModel.SetBonesPerCluster( ibonespercluster );

	if( false == bok )
	{
		delete Model;
		Model = 0;

		orkerrorlog( "ERROR: <xgmconvert> failed to load model<%s>\n", fname.c_str() );
	}

	return Model;
}

///////////////////////////////////////////////////////////////////////////////

CColladaAnim *CColladaAnim::Load(const AssetPath &fname)
{
	CColladaAnim *Anim = new CColladaAnim( fname.c_str() );

	bool bok = Anim->LoadDocument( fname );

	assert( bok );

	bok &= (CColladaAsset::ECOLLADA_ANIM==Anim->meAssetType);

	if( bok )
	{
		FCDAnimationLibrary *AnimLib = Anim->mDocument->GetAnimationLibrary();
		printf( "AnimLib<%p>\n", AnimLib );
		assert(AnimLib!=nullptr);
		bok = Anim->Parse();
		bok = Anim->GetPose();
	}
	else
	{
		printf( "Not an AnimAsset!\n" );
	}
	if( false == bok )
	{
		delete Anim;
		Anim = 0;
		orkerrorlog( "ERROR: <xgaconvert> could not open anim<%s>\n", fname.c_str() );
	}
	return Anim;
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 FCDMatrixTofmtx4( const FMMatrix44 & inmat )
{
	fmtx4 ret = fmtx4::Identity;

	const float *psrc = inmat;

	for( int i=0; i<16; i++ )
	{
		int irow = (i%4);
		int icol = (i/4);
		ret.SetElemYX( irow, icol, psrc[ i ] );
	}

	return ret;
}

///////////////////////////////////////////////////////////////////////////////

} }
#endif
