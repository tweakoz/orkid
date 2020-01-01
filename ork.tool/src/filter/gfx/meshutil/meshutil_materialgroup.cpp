////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/application/application.h>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/file/chunkfile.h>
#include <ork/application/application.h>
#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/gfx/meshutil/clusterizer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::MeshUtil {
///////////////////////////////////////////////////////////////////////////////

void BuildXgmClusterPrimGroups( lev2::XgmCluster & XgmCluster, const std::vector<unsigned int> & TriangleIndices );

void ToolMaterialGroup::ComputeVtxStreamFormat()
{
	ork::lev2::GfxMaterialFx* MatFx = 0;

	if( _orkMaterial )
	{
		MatFx = rtti::downcast<ork::lev2::GfxMaterialFx*>( _orkMaterial );
	}

	meVtxFormat = lev2::EVTXSTREAMFMT_END;

	const orkvector<ork::lev2::VertexConfig>& VertexConfigDataAvl = mAvailVertexConfigData;
	int inumvtxcfgavailable = VertexConfigDataAvl.size();

	const auto& TargetFormatsAvailable = tool::ColladaExportPolicy::context()->mAvailableVertexFormats.GetFormats();

	//////////////////////////////////////////
	// get vertex configuration data
	//////////////////////////////////////////

	int imin_jnt = mMeshConfigurationFlags.mbSkinned ? 1 : 0;
	int	imin_clr = 0;
	int	imin_tex = 0;
	int	imin_nrm = 0;
	int	imin_bin = 0;
	int	imin_tan = 0;

	if( MatFx ) // fx material
	{	const orkvector<ork::lev2::VertexConfig>& VertexConfigDataMtl = MatFx->RefVertexConfig();
		int inumvtxcfgmaterial = VertexConfigDataMtl.size();
		for( int iv=0; iv<inumvtxcfgmaterial; iv++ )
		{	const ork::lev2::VertexConfig& vcfg = VertexConfigDataMtl[iv];
			if( vcfg.Semantic.find("COLOR") != std::string::npos ) imin_clr++;
			if( vcfg.Semantic.find("TEXCOORD") != std::string::npos ) imin_tex++;
			if( vcfg.Semantic.find("BINORMAL") != std::string::npos ) imin_bin++;
			if( vcfg.Semantic.find("TANGENT") != std::string::npos ) imin_tan++;
			if( vcfg.Semantic == "NORMAL") imin_nrm++;
		}
	}
	else // basic materials
	{
		imin_clr = mMeshConfigurationFlags.mbSkinned ? 0 : 1;
		imin_tex = 1;
		imin_nrm = 1;
	}

	if( mLightMapPath.length() )
	{
		imin_tex++;
		//orkprintf( "lightmap<%s> found\n", LightMapPath.c_str() );
	}
	//////////////////////////////////////////
	// find lowest cost match
	//////////////////////////////////////////

	static const int kbad_score = 0x10000;
	int inv_score = kbad_score;

	for( auto itf=TargetFormatsAvailable.begin(); itf!=TargetFormatsAvailable.end(); itf++ )
	{
		auto format = itf->second;

		bool bok_jnt = mMeshConfigurationFlags.mbSkinned ? (format.miNumJoints >= imin_jnt) : (format.miNumJoints==0);
		bool bok_clr = (format.miNumColors >= imin_clr);
		bool bok_tex = (format.miNumUvs >= imin_tex);
		bool bok_nrm = (imin_nrm==1) ? format.mbNormals : true;
		bool bok_bin = (format.miNumBinormals >= imin_bin);

		bool bok = (bok_jnt&bok_clr&bok_tex&bok_nrm&bok_bin);

		/////////////////////////////////

		int iscore = bok	?	// build weighted score (lower score is better)
								format.miVertexSize
							:	kbad_score;

		/////////////////////////////////

		if( iscore<inv_score )
		{
			meVtxFormat = format.meVertexStreamFormat;
			inv_score = iscore;
		}
	}

	//////////////////////////////////////////


}

///////////////////////////////////////////////////////////////////////////////

void ToolMaterialGroup::Parse( const tool::ColladaMaterial& colmat )
{
	ork::lev2::GfxMaterial* pmat = colmat._orkMaterial;

	ork::lev2::GfxMaterialFx* pmatfx = rtti::autocast( pmat );

	if( pmatfx )
	{
		meMaterialClass = EMATCLASS_FX;
		mVertexConfigData = pmatfx->RefVertexConfig();
		_orkMaterial = colmat._orkMaterial;
	}
	else
	{
		meMaterialClass = EMATCLASS_STANDARD;
	}
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::MeshUtil {
