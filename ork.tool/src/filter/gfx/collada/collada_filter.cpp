////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <orktool/filter/gfx/collada/collada.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <orktool/filter/filter.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/toolcore/FunctionManager.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/reflect/Functor.h>
#include <ork/rtti/downcast.h>
#include <ork/util/Context.hpp>

#if defined(USE_FCOLLADA)

template class ork::util::Context<ork::tool::ColladaExportPolicy>;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace MeshUtil 
{
	bool DAEToNAVCollision( const tokenlist& options );
	bool DAEToSECCollision( const tokenlist& options );
	bool DAEToDAE( const tokenlist& options );

} }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {

class DAENAVFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(DAENAVFilter,CAssetFilterBase);
public: //
	DAENAVFilter(  )
	{
	}
	bool ConvertAsset( const tokenlist& toklist ) final
	{
		return MeshUtil::DAEToNAVCollision( toklist );
	}
};
void DAENAVFilter::Describe() {}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class DAESECFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(DAESECFilter,CAssetFilterBase);
public: //
	DAESECFilter(  )
	{
	}
	bool ConvertAsset( const tokenlist& toklist ) final
	{
		return MeshUtil::DAEToSECCollision( toklist );
	}
};
void DAESECFilter::Describe() {}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class DAEDAEFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(DAEDAEFilter,CAssetFilterBase);
public: //
	DAEDAEFilter(  )
	{
	}
	bool ConvertAsset( const tokenlist& toklist ) final
	{
		return MeshUtil::DAEToDAE( toklist );
	}
};
void DAEDAEFilter::Describe() {}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ColladaVertexFormat::ColladaVertexFormat()
	: meVertexStreamFormat( ork::lev2::EVTXSTREAMFMT_END )
	, miNumJoints(0)
	, miNumColors(0)
	, mbNormals(false)
	, miNumUvs(0)
	, miNumBinormals(0)
	, miNumTangents(0)
	, miVertexSize(0)
{
}

void ColladaVertexFormat::SetFormat( ork::lev2::EVtxStreamFormat efmt )
{
	meVertexStreamFormat = efmt;

	switch( efmt )
	{
		case ork::lev2::EVTXSTREAMFMT_V12N12T8I4W4:		// PC basic skinned
			mbNormals = true;
			miNumJoints = 4;
			miNumUvs = 1;
			miVertexSize = sizeof(ork::lev2::SVtxV12N12T8I4W4);
			break;
		case ork::lev2::EVTXSTREAMFMT_V12N12B12T8I4W4:	// PC binormal skinned
			mbNormals = true;
			miNumBinormals = 1;
			miNumJoints = 4;
			miNumUvs = 1;
			miVertexSize = sizeof(ork::lev2::SVtxV12N12B12T8I4W4);
			break;
		case ork::lev2::EVTXSTREAMFMT_V12N12B12T8C4:	// PC basic rigid
			mbNormals = true;
			miNumUvs = 1;
			miNumColors = 1;
			miNumBinormals = 1;
			miVertexSize = sizeof(ork::lev2::SVtxV12N12B12T8C4);
			break;
		case ork::lev2::EVTXSTREAMFMT_V12N12T16C4:	// PC rigid
			mbNormals = true;
			miNumUvs = 2;
			miNumColors = 1;
			miVertexSize = sizeof(ork::lev2::SVtxV12N12T16C4);
			break;
		case ork::lev2::EVTXSTREAMFMT_V12N12B12T16:	// PC basic rigid
			mbNormals = true;
			miNumUvs = 2;
			miNumColors = 1;
			miNumBinormals = 1;
			miVertexSize = sizeof(ork::lev2::SVtxV12N12B12T16);
			break;
		case ork::lev2::EVTXSTREAMFMT_V12N6I1T4:		// WII basic skinned
			mbNormals = true;
			miNumJoints = 1;
			miNumUvs = 1;
			miVertexSize = sizeof(ork::lev2::SVtxV12N6I1T4);
			break;
		case ork::lev2::EVTXSTREAMFMT_V12N6C2T4:		// WII basic rigid
			mbNormals = true;
			miNumUvs = 1;
			miNumColors = 1;
			miVertexSize = sizeof(ork::lev2::SVtxV12N6C2T4);
			break;
	}
}

void ColladaAvailVertexFormats::Add( ork::lev2::EVtxStreamFormat efmt )
{
	ColladaVertexFormat format;
	format.SetFormat( efmt );
	mFormats.insert( std::make_pair( efmt, format ) );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void RegisterColladaFilters()
{
	CAssetFilter::RegisterFilter("dae:xga", DAEXGAFilter::DesignNameStatic().c_str());
	CAssetFilter::RegisterFilter("dae:xgm", DAEXGMFilter::DesignNameStatic().c_str());
	//CAssetFilter::RegisterFilter("dae:ggm", DAEGGMFilter::DesignNameStatic().c_str());
	CAssetFilter::RegisterFilter("dae:nav", DAENAVFilter::DesignNameStatic().c_str());
	CAssetFilter::RegisterFilter("dae:dae", DAEDAEFilter::DesignNameStatic().c_str());
	CAssetFilter::RegisterFilter("dae:sec", DAESECFilter::DesignNameStatic().c_str());
}

///////////////////////////////////////////////////////////////////////////////

DAEXGAFilter::DAEXGAFilter( )
{
}
DAEXGMFilter::DAEXGMFilter( )
{
}
DAEGGMFilter::DAEGGMFilter( )
{
}
void DAEXGAFilter::Describe()
{
}
void DAEXGMFilter::Describe()
{
}
void DAEGGMFilter::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

bool SaveGGM( const AssetPath& Filename, const lev2::XgmModel *mdl );

bool DAEXGMFilter::ConvertAsset( const tokenlist& toklist )
{
	ork::tool::FilterOptMap options;
	options.SetDefault( "-dice" ,"false" );
	options.SetDefault( "-dicedim" ,"128.0f" );
	options.SetDefault( "-in" ,"yo" );
	options.SetDefault( "-out" ,"yo" );
	options.SetOptions( toklist );

	const std::string inf = options.GetOption( "-in" )->GetValue();
	const std::string outf = options.GetOption( "-out" )->GetValue();

	bool bDICE = options.GetOption( "-dice" )->GetValue()=="true";
	bool brval = false;
	
	CSystem::SetGlobalStringVariable( "StripJoinPolicy", "true" );

	///////////////////////////////////////////////////
	// swap endian on xb360 assets
	///////////////////////////////////////////////////
	ork::EndianContext* pctx = 0;
	if( outf.find( "xb360") != std::string::npos )
	{
		pctx = new ork::EndianContext;
		pctx->mendian = ork::EENDIAN_BIG;
	}
	///////////////////////////////////////////////////
	// swap endian on xb360 assets
	///////////////////////////////////////////////////

	ColladaExportPolicy policy;
	policy.mDDSInputOnly = true; // TODO
	policy.mUnits = UNITS_METER;
	policy.mSkinPolicy.mWeighting = ColladaSkinPolicy::EPOLICY_MATRIXPALETTESKIN_W4;
	policy.miNumBonesPerCluster = 32;
	policy.mColladaInpName = inf;
	policy.mColladaOutName = outf;
	policy.mDicingPolicy.SetPolicy( bDICE ? ColladaDicingPolicy::ECTP_DICE : ColladaDicingPolicy::ECTP_DONT_DICE );
	policy.mTriangulation.SetPolicy( ColladaTriangulationPolicy::ECTP_TRIANGULATE );

	////////////////////////////////////////////////////////////////
	// PC vertex formats supported
	policy.mAvailableVertexFormats.Add( lev2::EVTXSTREAMFMT_V12N12T8I4W4 );		// PC basic skinned
	policy.mAvailableVertexFormats.Add( lev2::EVTXSTREAMFMT_V12N12B12T8I4W4 );	// PC 1 tanspace skinned
	policy.mAvailableVertexFormats.Add( lev2::EVTXSTREAMFMT_V12N12B12T8C4 );	// PC 1 tanspace unskinned
	policy.mAvailableVertexFormats.Add( lev2::EVTXSTREAMFMT_V12N12B12T16 );		// PC 1 tanspace, 2UV unskinned
	policy.mAvailableVertexFormats.Add( lev2::EVTXSTREAMFMT_V12N12T16C4 );		// PC 2UV 1 color unskinned
	////////////////////////////////////////////////////////////////

	CColladaModel *colmdl = CColladaModel::Load( inf.c_str() );

	if( colmdl )
	{
		file::Path OutPath(outf.c_str());
		brval = ConvertTextures(colmdl,OutPath);

		bool saveres = ork::lev2::SaveXGM( OutPath, & colmdl->mXgmModel );
		if(!saveres)
			orkerrorlog("ERROR: <xgmconvert> failed to save model<%s>\n", OutPath.c_str());
		brval &= saveres;
	}

	///////////////////////////////////////////////////
	// swap endian on xb360 assets
	///////////////////////////////////////////////////
	if( pctx )
	{
		delete pctx;
	}
	///////////////////////////////////////////////////
	// swap endian on xb360 assets
	///////////////////////////////////////////////////

	return (colmdl != 0 || !brval); // any error
}

///////////////////////////////////////////////////////////////////////////////

bool DAEXGMFilter::ConvertTextures( CColladaModel* mdl, const file::Path& outmdlpth )
{
	ork::tool::FilterOptMap options;
	options.SetDefault( "-in", "yo" );
	options.SetDefault( "-out", "yo" );
	options.SetDefault( "-platform", "pc" );
	options.SetDefault( "-flipy" ,"true" );
	return mdl->ConvertTextures(outmdlpth, options );
}

///////////////////////////////////////////////////////////////////////////////

bool DAEGGMFilter::ConvertAsset( const tokenlist& toklist )
{
	ork::EndianContext pctx;
	pctx.mendian = ork::EENDIAN_BIG;

	ork::tool::FilterOptMap options;
	options.SetDefault( "-dice" ,"false" );
	options.SetDefault( "-dicedim" ,"128.0f" );
	options.SetDefault( "-in" ,"yo" );
	options.SetDefault( "-out" ,"yo" );
	options.SetOptions( toklist );

	const std::string inf = options.GetOption( "-in" )->GetValue();
	const std::string outf = options.GetOption( "-out" )->GetValue();
	bool bDICE = options.GetOption( "-dice" )->GetValue()=="true";

	bool brval = false;
	
	CSystem::SetGlobalStringVariable( "StripJoinPolicy", "true" );

	ColladaExportPolicy policy;
	policy.mSkinPolicy.mWeighting = ColladaSkinPolicy::EPOLICY_MATRIXPALETTESKIN_W1;
	policy.mPrimGroupPolicy.mMaxIndices = ColladaPrimGroupPolicy::EPOLICY_MAXINDICES_WII;
	policy.miNumBonesPerCluster = 8;
	policy.mColladaInpName = inf;
	policy.mColladaOutName = outf;
	policy.mTriangulation.SetPolicy( ColladaTriangulationPolicy::ECTP_TRIANGULATE );
	policy.mDicingPolicy.SetPolicy( bDICE ? ColladaDicingPolicy::ECTP_DICE : ColladaDicingPolicy::ECTP_DONT_DICE );
	////////////////////////////////////////////////////////////////
	// WII vertex formats supported
	policy.mAvailableVertexFormats.Add( lev2::EVTXSTREAMFMT_V12N6I1T4 ); // WII basic skinned
	policy.mAvailableVertexFormats.Add( lev2::EVTXSTREAMFMT_V12N6C2T4 ); // WII basic unskinned
	////////////////////////////////////////////////////////////////

	CColladaModel *colmdl = CColladaModel::Load( inf.c_str() );

	if( colmdl )
	{
		file::Path OutPath(outf.c_str());
		brval = ConvertTextures(colmdl,OutPath);
		brval &= SaveGGM( OutPath, & colmdl->mXgmModel );
	}

	return (colmdl!=0);
}

///////////////////////////////////////////////////////////////////////////////

bool DAEGGMFilter::ConvertTextures( CColladaModel* mdl, const file::Path& outmdlpth )
{
	ork::tool::FilterOptMap options;
	options.SetDefault( "-flipy" ,"true" );
	options.SetDefault( "-platform", "wii" );
	options.SetDefault( "-in", "yo" );
	options.SetDefault( "-out", "yo" );
	return mdl->ConvertTextures(outmdlpth, options );
}

}
}

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::DAEXGMFilter,"DAEXGMFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::DAEXGAFilter,"DAEXGAFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::DAEGGMFilter,"DAEGGMFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::DAENAVFilter,"DAENAVFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::DAESECFilter,"DAESECFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::DAEDAEFilter,"DAEDAEFilter");

#endif
