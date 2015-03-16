///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_fx.hpp>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/camera/cameraman.h>
#include <ork/file/file.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/prop.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/kernel/Array.hpp>
#include <ork/kernel/string/string.h>

bool gbheadlight = true;

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterialFx, "MaterialFx" )
INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::GfxMaterialFxParamBase, "FxParamBase" );


INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::FxMatrixBlockApplicator, "FxMatrixBlockApplicator" )

namespace ork { 

template class orklut<std::string,lev2::GfxMaterialFxParamBase*>;
template class orklut<PoolString,lev2::GfxMaterialFx*>;

namespace lev2 {

/////////////////////////////////////////////////////////////////////////

bool GfxMaterialFx::gEnableLightPreview = false;

void GfxMaterialFx::Describe() {}
void GfxMaterialFxParamBase::Describe() {}

template<> void GfxMaterialFxParam<int>::Describe() {}
template<> void GfxMaterialFxParamArtist<int>::Describe() {}
template<> void GfxMaterialFxParamEngine<int>::Describe() {}

template<> void GfxMaterialFxParam<float>::Describe() {}
template<> void GfxMaterialFxParamArtist<float>::Describe() {}
template<> void GfxMaterialFxParamEngine<float>::Describe() {}

template<> void GfxMaterialFxParam<CVector2>::Describe() {}
template<> void GfxMaterialFxParamArtist<CVector2>::Describe() {}
template<> void GfxMaterialFxParamEngine<CVector2>::Describe() {}

template<> void GfxMaterialFxParam<CVector3>::Describe() {}
template<> void GfxMaterialFxParamArtist<CVector3>::Describe() {}
template<> void GfxMaterialFxParamEngine<CVector3>::Describe() {}

template<> void GfxMaterialFxParam<CVector4>::Describe() {}
template<> void GfxMaterialFxParamArtist<CVector4>::Describe() {}
template<> void GfxMaterialFxParamEngine<CVector4>::Describe() {}

template<> void GfxMaterialFxParam<CMatrix4>::Describe() {}
template<> void GfxMaterialFxParamArtist<CMatrix4>::Describe() {}
template<> void GfxMaterialFxParamEngine<CMatrix4>::Describe() {}

template<> void GfxMaterialFxParam<CMatrix3>::Describe() {}
template<> void GfxMaterialFxParamArtist<CMatrix3>::Describe() {}
template<> void GfxMaterialFxParamEngine<CMatrix3>::Describe() {}

template<> void GfxMaterialFxParam<std::string>::Describe() {}
template<> void GfxMaterialFxParamArtist<std::string>::Describe() {}
template<> void GfxMaterialFxParamEngine<std::string>::Describe() {}

template<> void GfxMaterialFxParam<Texture*>::Describe() {}
template<> void GfxMaterialFxParamArtist<Texture*>::Describe() {}
template<> void GfxMaterialFxParamEngine<Texture*>::Describe() {}

///////////////////////////////////////////////////////////////////////////////

GfxMaterialFxParamBase::GfxMaterialFxParamBase(GfxMaterialFx *parent)
	: mbIsBindable( true ), mParentMaterial(parent)
{
}

void GfxMaterialFxParamBase::AddAnnotation( const char* pkey, const char* pval )
{
	mAnnotations.AddSorted(std::string(pkey),std::string(pval));
}

GfxMaterialFxEffectInstance::GfxMaterialFxEffectInstance()
	: mpEffect( 0 )
{
}

GfxMaterialFxEffectInstance::~GfxMaterialFxEffectInstance()
{
	for( ork::orklut<std::string,GfxMaterialFxParamBase*>::const_iterator
		it = mParameterInstances.begin();
		it!= mParameterInstances.end();
		it++ )
	{
		delete it->second;
	}

}
///////////////////////////////////////////////////////////////////////////////

GfxMaterialFx::GfxMaterialFx()
	: mActiveTechnique( 0 )
	, mActiveShadowTechnique( 0 )
	, mActiveLightMapTechnique(0)
	, mActiveVertexLightTechnique(0)
	, mActiveLightPreviewTechnique(0)
	, mActiveSkinnedTechnique( 0 )
	, mActiveSkinnedShadowTechnique( 0 )
	, mActivePickTechnique( 0 )
	, mActiveSkinnedPickTechnique( 0 )
	, mBonesParam( 0 )
	, mWorldMtxParam(0)
	, mWorldViewMtxParam(0)
	, mWorldViewProjectionMtxParam(0)
	, mLightMapParam(0)
	, mLightMapTexture(0)
{
	miNumPasses = 1;
    mRasterState.SetShadeModel( ESHADEMODEL_SMOOTH );
    mRasterState.SetDepthTest( EDEPTHTEST_LEQUALS );

	for(int i = 0; i < kMaxEngineParamFloats; i++)
		mEngineParamFloats[i] = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

GfxMaterialFx::~GfxMaterialFx()
{
	
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialFx::SetEffect( FxShader* pshader )
{
	mEffectInstance.mpEffect = pshader;

	if( mEffectInstance.mpEffect )
	{
		mBonesParam = GfxEnv::GetRef().GetLoaderTarget()->FXI()->GetParameterH( mEffectInstance.mpEffect, "BoneMatrices" );
	}

	OrkAssert( mEffectInstance.mpEffect != 0 );

	//////////////////////////////////////////
	// Lights

	mLightingInterface.Init( mEffectInstance.mpEffect );

	//////////////////////////////////////////
}

void GfxMaterialFx::LoadEffect( const AssetPath& EffectAssetName )
{
	AssetPath pth = EffectAssetName;

	bool hasurl = pth.HasUrlBase();
	if( hasurl == false )
	{
		pth.SetUrlBase( "orkshader://" );
	}


	FxShaderAsset* passet = asset::AssetManager<FxShaderAsset>::Find( pth.c_str() );
	passet = asset::AssetManager<FxShaderAsset>::Load( pth.c_str() );

	mAssetPath=pth;
	SetEffect( passet ? passet->GetFxShader() : 0 );
}

///////////////////////////////////////////////////////////////////////////////

static orkvector<std::string> StringListParser( const std::string & instr )
{
	size_t ilen = instr.length();
	orkvector<std::string> out;
	for( size_t ia=0; ia<ilen; )
	{	std::string::size_type ilb = instr.find( "{", ia );
		if( ilb == std::string::npos )
		{	ia = ilen;
		}
		else
		{	std::string::size_type ilc = instr.find( "}", ilb );
			if( ilc == std::string::npos )
			{	ia = ilen;
			}
			else
			{	std::string tokstr = instr.substr( ilb+1, (ilc-ilb)-1 );
				out.push_back( tokstr );
				ia = ilc+1;
			}
		}

	}
	return out;
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialFx::Init( GfxTarget* pTARG )
{
	//////////////////////////////////////////
	/// parse shader vertex configuration data
	/// this figures out the the required vertex data for
	/// a given shader, this info will influence the
	/// vertex buffer builder.
	//////////////////////////////////////////

	std::string FxVtxAttrib = GetParamValue( "vertexAttributeList" );
	std::string FxVtxSource = GetParamValue( "vertexAttributeSource" );
	orkvector<std::string> vattribs = StringListParser( FxVtxAttrib );
	orkvector<std::string> vsources = StringListParser( FxVtxSource );
	size_t inumtoks( vattribs.size() );
	OrkAssert( (inumtoks%4) == 0 );
	for( size_t ia=0,is=0; ia<inumtoks; )
	{
		VertexConfig VC;
		VC.Name = vattribs[ ia++ ];
		VC.Type = vattribs[ ia++ ];
		const std::string & c = vattribs[ ia++ ];
		VC.Semantic = vattribs[ ia++ ];
		const auto& src = vsources[is++];

		FixedString<65536> tmp(src.c_str());
		tmp.replace_in_place("color:","");
		tmp.replace_in_place("uv:","");
		VC.Source=tmp.c_str();
		mVertexConfigData.push_back( VC );
	}

	//////////////////////////////////////////

	std::string FxDesc = GetParamValue( "description" );
	std::string TekName = GetParamValue( "technique" );
	
	mMainTechniqueName = TekName;

	//////////////////////////////////////////
	// extract morkshader<> from description

	std::string::size_type iposb = FxDesc.find( "morkshader<" );
	std::string::size_type ipose = FxDesc.find( ">" );

	OrkAssert( iposb!=std::string::npos && ipose!=std::string::npos );

	std::string FxName = FxDesc.substr( 11, ipose-11 );

	//////////////////////////////////////////


	///////////////////////////
	///////////////////////////
	//orkprintf( "Loading FxShader<%s>\n", FxName.c_str() );
	LoadEffect( AssetPath(FxName.c_str() ) );
	///////////////////////////
	///////////////////////////

	if( mEffectInstance.mpEffect )
	{
		//////////////////////////////////////////////////////
		/// Bind Effect Parameters (Effect Instance Parameters)
		/// Artist Supplied Parameters
		//////////////////////////////////////////////////////

		orklut<std::string,GfxMaterialFxParamBase*> & ParamInstanceMap = mEffectInstance.mParameterInstances;

		const orkmap<std::string,const FxShaderParam*> & ParamNameMap = mEffectInstance.mpEffect->GetParametersByName();
		//const orkmap<std::string,const FxShaderParam*> & ParamSemMap = mEffectInstance.mpEffect->GetParametersBySemantic();

		for( orklut<std::string,GfxMaterialFxParamBase*>::iterator iti=ParamInstanceMap.begin(); iti!=ParamInstanceMap.end(); iti++ )
		{
			GfxMaterialFxParamBase * ParamInst		= (*iti).second;
			const std::string & ParamName		= ParamInst->GetRecord().mParameterName;
			//const std::string & ParamSem		= ParamInst->GetRecord().mParameterSemantic;
			orkmap<std::string,const FxShaderParam*>::const_iterator itp=ParamNameMap.find(ParamName);
			if( itp != ParamNameMap.end() ) // try by semantic
			{
				const FxShaderParam* Param = (*itp).second;
				ParamInst->GetRecord().mParameterHandle = Param;
				//orkprintf( "FxShader: ModelFXParameterInstance to FX shader [material %s] [ParamInstName %s] BOUND !\n", GetName().c_str(), ParamName.c_str() );
			}

			orkprintf( "<FX> ArtistParam %s plath %08x\n", ParamName.c_str(), ParamInst->GetRecord().mParameterHandle );

			if( ParamInst->IsBindable() )
			{
				if( ParamInst->GetRecord().mParameterHandle == 0)
				{
					ParamInst->SetBindable(false);
				}
				else
				{
					ParamInst->GetRecord().meBindingScope = FxParamRec::ESCOPE_PERMATERIALINST;
				}
			}
		}

		//////////////////////////////////////////////////////
		// Bind Effect Parameters (Engine Supplied Parameters)
		//////////////////////////////////////////////////////

		for( orkmap<std::string,const FxShaderParam*>::const_iterator itp=ParamNameMap.begin(); itp!=ParamNameMap.end(); itp++ )
		{
			std::string Name = (*itp).first;
			const FxShaderParam* FxParam = (*itp).second;
			std::string Semantic = FxParam->mParameterSemantic;

			std::transform( Semantic.begin(), Semantic.end(), Semantic.begin(), lower() );

			GfxMaterialFxParamBase *param = 0;

			////////////////////////////////////
			////////////////////////////////////
			////////////////////////////////////
			////////////////////////////////////

			bool bisinverse = Semantic.find("inverse") != std::string::npos;
			bool bistranspose = Semantic.find("transpose") != std::string::npos;

			if( Semantic == "samplerlmp" )
			{
				mLightMapParam = FxParam;
			}
			if( Semantic == "bonematrices" )
			{
				mBonesParam = FxParam;
			}
			else if( Semantic == "isshadowreciever" )
			{
				mIsShadowRecieverParam = FxParam;
			}
			else if( Semantic == "isshadowcaster" )
			{
				mIsShadowCasterParam = FxParam;
			}
			else if( Semantic == "isskinned" )
			{
				mIsSkinnedParam = FxParam;
			}
			else if( Semantic == "ispick" )
			{
				mIsPickParam = FxParam;
			}
			////////////////////////////////////
			else if( Semantic == "worldviewprojection" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CMatrix4> *ParamMatrix4 = new GfxMaterialFxParamEngine<CMatrix4>(this);
				param = ParamMatrix4;
				struct yo
				{
					static const CMatrix4& RefMvpMtx(GfxTarget *pTARG,const GfxMaterialFxParamBase*param)
					{
						return pTARG->MTXI()->RefMVPMatrix();
					}
				};
				ParamMatrix4->mFuncptrParam = yo::RefMvpMtx;
				mWorldViewProjectionMtxParam = FxParam;
			}
			////////////////////////////////////
			else if( Semantic == "worldrot4" )
			////////////////////////////////////
			{
				struct getmat
				{
					static const CMatrix4& doit_worldrot( GfxTarget *pTARG )
					{
						return pTARG->MTXI()->RefR4Matrix();
					}
				};
				GfxMaterialFxParamEngine<CMatrix4> *ParamMatrix4 = new GfxMaterialFxParamEngine<CMatrix4>(this);
				ParamMatrix4->mFuncptrVoid = getmat::doit_worldrot;
				param = ParamMatrix4;
			}
			////////////////////////////////////
			else if( Semantic == "worldrot3" )
			////////////////////////////////////
			{
				struct getmat
				{
					static const CMatrix3& doit_worldrot3( GfxTarget *pTARG )
					{
						return pTARG->MTXI()->RefR3Matrix();
					}
				};
				GfxMaterialFxParamEngine<CMatrix3> *ParamMatrix3 = new GfxMaterialFxParamEngine<CMatrix3>(this);
				ParamMatrix3->mFuncptrVoid = getmat::doit_worldrot3;
				param = ParamMatrix3;
			}	
			////////////////////////////////////
			else if( Semantic == "worldroti" )
			////////////////////////////////////
			{
				struct getmat
				{
					static const CMatrix4& doit_worldroti( GfxTarget *pTARG )
					{
						static CMatrix4 MatR;
						MatR = pTARG->MTXI()->RefR4Matrix();
						MatR.Inverse();
						return MatR;
					}
				};
				GfxMaterialFxParamEngine<CMatrix4> *ParamMatrix4 = new GfxMaterialFxParamEngine<CMatrix4>(this);
				ParamMatrix4->mFuncptrVoid = getmat::doit_worldroti;
				param = ParamMatrix4;
			}
			////////////////////////////////////
			else if( Semantic == "worldinverse" )
			////////////////////////////////////
			{
				struct getmat
				{
					static const CMatrix4& doit_worldinverse( GfxTarget *pTARG )
					{
						static CMatrix4 wimat;
						wimat = pTARG->MTXI()->RefMMatrix();
						wimat.Inverse();
						return wimat;
					}
				};
				GfxMaterialFxParamEngine<CMatrix4> *ParamMatrix4 = new GfxMaterialFxParamEngine<CMatrix4>(this);
				ParamMatrix4->mFuncptrVoid = getmat::doit_worldinverse;
				param = ParamMatrix4;
			}
			////////////////////////////////////
			else if( Semantic == "worldviewprojectioninverse" )
			////////////////////////////////////
			{
				struct getmat
				{
					static const CMatrix4& doit_worldviewprojectioninverse( GfxTarget *pTARG )
					{
						static CMatrix4 wvpimat;
						wvpimat = pTARG->MTXI()->RefMVPMatrix();
						wvpimat.Inverse();
						return wvpimat;
					}
				};
				GfxMaterialFxParamEngine<CMatrix4> *ParamMatrix4 = new GfxMaterialFxParamEngine<CMatrix4>(this);
				ParamMatrix4->mFuncptrVoid = getmat::doit_worldviewprojectioninverse;
				param = ParamMatrix4;
			}
			////////////////////////////////////
			else if( Semantic == "viewprojection" )
			////////////////////////////////////
			{
				struct getmat
				{
					static const CMatrix4& doit_viewprojection( GfxTarget *pTARG )
					{
						return pTARG->MTXI()->RefVPMatrix();
					}
				};
				GfxMaterialFxParamEngine<CMatrix4> *ParamMatrix4 = new GfxMaterialFxParamEngine<CMatrix4>(this);
				ParamMatrix4->mFuncptrVoid = getmat::doit_viewprojection;
				param = ParamMatrix4;
			}
			////////////////////////////////////
			else if( Semantic == "world" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CMatrix4> *ParamMatrix4 = new GfxMaterialFxParamEngine<CMatrix4>(this);
				param = ParamMatrix4;

				struct getmat
				{
					static const CMatrix4& doit_world( GfxTarget *pTARG )
					{
						return pTARG->MTXI()->RefMMatrix();
					}
				};

				ParamMatrix4->mFuncptrVoid = getmat::doit_world;
				mWorldMtxParam = FxParam;
			}
			////////////////////////////////////
			else if( Semantic == "worldview" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CMatrix4> *ParamMatrix4 = new GfxMaterialFxParamEngine<CMatrix4>(this);
				param = ParamMatrix4;

				struct getmat
				{
					static const CMatrix4& doit_worldview( GfxTarget *pTARG )
					{
						//const CMatrix4& wmat = pTARG->MTXI()->RefMMatrix();
						//const CMatrix4& vmat = pTARG->MTXI()->RefVMatrix();
						return pTARG->MTXI()->RefMVMatrix(); //(wmat*vmat);
					}
				};

				ParamMatrix4->mFuncptrVoid = getmat::doit_worldview;
				mWorldViewMtxParam = FxParam;
			}
			////////////////////////////////////
			else if( Semantic == "viewportdim" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CVector2> *ParamVector2 = new GfxMaterialFxParamEngine<CVector2>(this);
				param = ParamVector2;
				struct getcurrentdims
				{	static const CVector2& doit_viewportdim( GfxTarget *pTARG )
					{
						static CVector2 Dims;
						//Dims.SetX( CReal(GfxEnv::GetRef().GetCT()->GetW()) );
						//Dims.SetY( CReal(GfxEnv::GetRef().GetCT()->GetH()) );
						return Dims;
					}
				};
				ParamVector2->mFuncptrVoid = getcurrentdims::doit_viewportdim;
			}
			////////////////////////////////////
			else if( Semantic.find("view") != std::string::npos )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CMatrix4> *ParamMatrix4 = new GfxMaterialFxParamEngine<CMatrix4>(this);
				param = ParamMatrix4;

				struct getmat
				{
					static const CMatrix4& doit_std( GfxTarget *pTARG )
					{
						return pTARG->MTXI()->RefVMatrix();
					}
					static const CMatrix4& doit_i( GfxTarget *pTARG )
					{
						static CMatrix4 mat;
						mat = pTARG->MTXI()->RefVMatrix();
						mat.Inverse();
						return mat;
					}
					static const CMatrix4& doit_t( GfxTarget *pTARG )
					{
						static CMatrix4 mat;
						mat = pTARG->MTXI()->RefVMatrix();
						mat.Transpose();
						return mat;
					}
					static const CMatrix4& doit_it( GfxTarget *pTARG )
					{
						return pTARG->MTXI()->RefVITIYMatrix();
					}
				};
				if( bisinverse )
				{
					ParamMatrix4->mFuncptrVoid = bistranspose ? getmat::doit_it : getmat::doit_i;
				}
				else
				{
					ParamMatrix4->mFuncptrVoid = bistranspose ? getmat::doit_t : getmat::doit_std;
				}
			}
			////////////////////////////////////
			else if( Semantic == "projection" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CMatrix4> *ParamMatrix4 = new GfxMaterialFxParamEngine<CMatrix4>(this);
				param = ParamMatrix4;

				struct getmat
				{
					static const CMatrix4& doit_std( GfxTarget *pTARG )
					{
						return pTARG->MTXI()->RefPMatrix();
					}
				};

				ParamMatrix4->mFuncptrVoid = getmat::doit_std;
			}
			////////////////////////////////////
			else if( Semantic == "modcolor" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CVector4> *ParamVector4 = new GfxMaterialFxParamEngine<CVector4>(this);
				param = ParamVector4;
				struct yo
				{
					static const CVector4& RefModColor(GfxTarget *pTARG)
					{
						return pTARG->RefModColor();
					}
				};
				ParamVector4->mFuncptrVoid = yo::RefModColor;
			}
			////////////////////////////////////
			else if( Semantic == "currentobj" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CVector4> *ParamVector4 = new GfxMaterialFxParamEngine<CVector4>(this);
				param = ParamVector4;
				struct getcurrentobj
				{	static const CVector4& doit( GfxTarget *pTARG )
					{
						//const CObject *pobj = GfxEnv::GetRef().GetCT()->GetCurrentObject();
						static CVector4 ObjColor ;
						//ObjColor.SetRGBAU32( (U32) pobj );
						return ObjColor;
					}
				};
				ParamVector4->mFuncptrVoid = getcurrentobj::doit;
			}
			////////////////////////////////////
			else if( Semantic == "reltimemod300" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CReal> *ParamFloat = new GfxMaterialFxParamEngine<CReal>(this);
				param = ParamFloat;
				struct gettime
				{	static const CReal& doit( GfxTarget *pTARG )
					{	static CReal reltime;
						reltime = std::fmod( CReal( CSystem::GetRef().GetLoResRelTime() ), 300.0f );
						return reltime;
					}
				};
				ParamFloat->mFuncptrVoid = gettime::doit;
			}
			////////////////////////////////////
			else if( Semantic == "reltime" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CReal> *ParamFloat = new GfxMaterialFxParamEngine<CReal>(this);
				param = ParamFloat;
				struct gettime
				{	static const CReal& doit( GfxTarget *pTARG )
					{	static CReal reltime;
						reltime = CReal( CSystem::GetRef().GetLoResRelTime() );
						return reltime;
					}
				};
				ParamFloat->mFuncptrVoid = gettime::doit;
			}
			////////////////////////////////////
			else if( Semantic == "engine_float_0" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CReal> *ParamFloat = new GfxMaterialFxParamEngine<CReal>(this);
				param = ParamFloat;
				struct get_engine_float_0
				{
					static const CReal& doit(GfxTarget *pTARG, const GfxMaterialFxParamBase *param)
					{	
						if(GfxMaterialFx *pmaterial = param->GetParentMaterial())
						{
							//if(pmaterial->mEngineParamFloats[0] > 0.0f && pmaterial->mEngineParamFloats[0] < 1.0f)
							//	orkprintf("pmaterial->mEngineParamFloats[0] = %g\n", pmaterial->mEngineParamFloats[0]);

							return pmaterial->mEngineParamFloats[0];
						}
						static const CReal zero = 0.0f;
						return zero;
					}
				};
				ParamFloat->mFuncptrParam = get_engine_float_0::doit;
			}
			////////////////////////////////////
			else if( Semantic == "lightdir" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<CVector3> *ParamT = new GfxMaterialFxParamEngine<CVector3>(this);
				param = ParamT;
				struct paramget
				{	static const CVector3& doit( GfxTarget *pTARG )
					{
						static CVector3 vout; //GfxEnv::GetRef().GetCurrentContext();
						//CCamera *pcam = 0; //pTarg->GetCurrentCamera();
						return vout; //pcam->mCameraData.mCamZNormal;
					}
				};
				ParamT->mFuncptrVoid = paramget::doit;
			}
			////////////////////////////////////
			else if( Semantic == "shadowmap" )
			////////////////////////////////////
			{
				GfxMaterialFxParamEngine<Texture*> *ParamT = new GfxMaterialFxParamEngine<Texture*>(this);
				param = ParamT;
				struct paramget
				{	static Texture* const & doit( GfxTarget *pTARG )
					{
						//GfxTarget *pTarg = 0; //GfxEnv::GetRef().GetCurrentContext();
						//GfxBuffer* pShadowBuf = 0; //pTarg->GetShadowBuffer();
						static Texture* gout = 0;
						return gout; //pShadowBuf ? pShadowBuf->GetTexture() : gout;
					}
				};
				ParamT->mFuncptrVoid = paramget::doit;
			}
			else if( Semantic == "toshadow" )
			{
				GfxMaterialFxParamEngine<CMatrix4> *ParamT = new GfxMaterialFxParamEngine<CMatrix4>(this);
				param = ParamT;
				struct paramget
				{	static const CMatrix4& doit( GfxTarget *pTARG )
					{
						const CMatrix4& ShadVMat = pTARG->MTXI()->GetShadowVMatrix();
						const CMatrix4& ShadPMat = pTARG->MTXI()->GetShadowPMatrix();

						static CMatrix4 Mat;
						Mat = pTARG->MTXI()->RefVMatrix();
						Mat.Inverse();

						Mat = Mat*ShadVMat;
						Mat = Mat*ShadPMat;

						//D3DXMATRIXA16 mViewToLightProj;
						//mViewToLightProj = *pmView;
						//D3DXMatrixInverse( &mViewToLightProj, NULL, &mViewToLightProj );
						//D3DXMatrixMultiply( &mViewToLightProj, &mViewToLightProj, &mLightView );
						//D3DXMatrixMultiply( &mViewToLightProj, &mViewToLightProj, &g_mShadowProj );
						//GfxBuffer* pShadowBuf = pTarg->GetShadowBuffer();
						//return pShadowBuf ? pShadowBuf->mpTexture : 0;

						return Mat;
					}
				};
				ParamT->mFuncptrVoid = paramget::doit;
			}
			////////////////////////////////////
			if( param )
			{
				//printf( "   FxParam<%p> pname2<%s>\n", FxParam, FxParam->mParameterName.c_str() );
				param->GetRecord().mParameterName = FxParam->mParameterName;
				param->GetRecord().mParameterSemantic = Semantic;
				param->GetRecord().mParameterHandle = FxParam;
				param->SetBindable(true);
				
				///////////////////////////////////////////////////////
				// for now we replace (instead of add), because:
				//   1. maya might export a material with a 'time' artist param
				//   2. the loaded engineside shader might bind by semantic
				//        causing two 'times' to be created
				//
				//  TODO - fix this with a more elegant solution
				////////////////////////////////////////////////////////
				
				mEffectInstance.ReplaceParameter( param );

				const orklut<std::string,std::string>& Annos = FxParam->mAnnotations;

				const orklut<std::string,std::string>::const_iterator it = Annos.find( "scope" );

				if( it != Annos.end() )
				{
					const std::string & Val = (*it).second;

					if( Val == std::string( "perframe" ) )
					{
						param->GetRecord().meBindingScope = FxParamRec::ESCOPE_PERFRAME;
					}
				}
			}
		}
		////////////////////////////////
		// set the active technique
		////////////////////////////////
		const orkmap<std::string,const FxShaderTechnique*> & TekMap = mEffectInstance.mpEffect->GetTechniques();
		orkmap<std::string,const FxShaderTechnique*>::const_iterator itt = TekMap.find( TekName );
		if( itt != TekMap.end() )
		{
			mActiveTechnique = (*itt).second;
		}
		else
		{
			//OrkAssert( TekMap.size() > 0 );
			orkprintf( "Technique<%s> Not Found!\n", TekName.c_str() );
			mActiveTechnique = 0; //(*TekMap.begin()).second;
		}
		////////////////////////////////
		// set the active lightmap technique (if it exists)
		////////////////////////////////
		std::string LightPreviewTekName = TekName + "LightPreview";
		orkmap<std::string,const FxShaderTechnique*>::const_iterator itlp = TekMap.find( LightPreviewTekName );
		if( itlp != TekMap.end() )
		{
			mActiveLightPreviewTechnique = (*itlp).second;
		}
		else
		{
			mActiveLightPreviewTechnique = 0;
			//orkprintf( "LightPreviewTechnique<%s> Not Found!\n", LightPreviewTekName.c_str() );
		}
		////////////////////////////////
		// set the active lightmap technique (if it exists)
		////////////////////////////////
		std::string LightVertexTekName = TekName + "VertexLit";
		orkmap<std::string,const FxShaderTechnique*>::const_iterator itvl = TekMap.find( LightVertexTekName );
		if( itvl != TekMap.end() )
		{
			mActiveVertexLightTechnique = (*itvl).second;
		}
		else
		{
			mActiveVertexLightTechnique = 0;
			//orkprintf( "VertexLitTechnique<%s> Not Found!\n", LightVertexTekName.c_str() );
		}
		////////////////////////////////
		// set the active lightmap technique (if it exists)
		////////////////////////////////
		std::string LightMappedTekName = TekName + "LightMapped";
		orkmap<std::string,const FxShaderTechnique*>::const_iterator its = TekMap.find( LightMappedTekName );
		if( its != TekMap.end() )
		{
			mActiveLightMapTechnique = (*its).second;
		}
		else
		{
			mActiveLightMapTechnique = 0;
			//orkprintf( "LightMappedTechnique<%s> Not Found!\n", LightMappedTekName.c_str() );
		}
		////////////////////////////////
		// set the active skinned technique (if it exists)
		////////////////////////////////
		std::string SkinnedTekName = TekName + "Skinned";
		its = TekMap.find( SkinnedTekName );
		if( its != TekMap.end() )
		{
			mActiveSkinnedTechnique = (*its).second;
		}
		else
		{
			mActiveSkinnedTechnique = 0;
		}
		////////////////////////////////
		// set the active skinned technique (if it exists)
		////////////////////////////////
		std::string SkinnedShadowTekName = TekName + "SkinnedShadow";
		orkmap<std::string,const FxShaderTechnique*>::const_iterator itsh = TekMap.find( SkinnedShadowTekName );
		if( itsh != TekMap.end() )
		{
			mActiveSkinnedShadowTechnique = (*itsh).second;
		}
		else
		{
			mActiveSkinnedShadowTechnique = 0;
			//orkprintf( "SkinnedShadowTechnique<%s> Not Found!\n", SkinnedShadowTekName.c_str() );
		}
		////////////////////////////////
		// set the pick technique
		orkmap<std::string,const FxShaderTechnique*>::const_iterator itp2 = TekMap.find( "pick" );
		mActivePickTechnique = (itp2 == TekMap.end()) ? 0 : (*itp2).second;
		////////////////////////////////
		// set the skinned_pick technique
		orkmap<std::string,const FxShaderTechnique*>::const_iterator itp3 = TekMap.find( "skinned_pick" );
		mActiveSkinnedPickTechnique = (itp3 == TekMap.end()) ? 0 : (*itp3).second;
		////////////////////////////////
	}
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialFxEffectInstance::ReplaceParameter( GfxMaterialFxParamBase*param )
{
	mParameterInstances.Replace(param->GetRecord().mParameterName,param);
}

void GfxMaterialFxEffectInstance::AddParameter( GfxMaterialFxParamBase*param )
{
	mParameterInstances.AddSorted( param->GetRecord().mParameterName, param );
}

std::string GfxMaterialFxEffectInstance::GetParamValue( const std::string & pname ) const
{
	orklut<std::string,GfxMaterialFxParamBase*>::const_iterator it = mParameterInstances.find( pname );
	return (it==mParameterInstances.end()) ? "" : (*it).second->GetValueString();
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialFx::AddParameter( GfxMaterialFxParamBase*param )
{
	mEffectInstance.AddParameter( param );
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialFx::SetTechnique( const std::string & TechniqueName )
{
	mActiveTechniqueName = TechniqueName;
}

///////////////////////////////////////////////////////////////////////////////

CPerformanceItem* GfxMaterialFx::gMatFxBeginPassPerfItem = 0;
CPerformanceItem* GfxMaterialFx::gMatFxBeginBlockPerfItem = 0;

///////////////////////////////////////////////////////////////////////////////

bool GfxMaterialFx::BeginPass( GfxTarget *pTarg, int iPass )
{
	bool rval = false;

	bool bpick = pTarg->FBI()->IsPickState();
	U32 ucurtarghash = pTarg->GetTargetFrame();

	if( mEffectInstance.mpEffect )
	{
		pTarg->RSI()->BindRasterState( mRasterState, true );

		////////////////////////////////

		pTarg->FXI()->BindPass( mEffectInstance.mpEffect, iPass );

		////////////////////////////////

		if( mLightMapTexture )
		{
			if( mLightMapParam )
			{
				FxShader* hshader = mEffectInstance.mpEffect;
				pTarg->FXI()->BindParamCTex( hshader, mLightMapParam, mLightMapTexture );
			}
		}

		////////////////////////////////
		// Bind Parameters
		////////////////////////////////

		const orklut<std::string,GfxMaterialFxParamBase*> & ParamInstances = mEffectInstance.mParameterInstances;
		size_t isize = ParamInstances.size();

		static int LastPass = 0;

		//for( orklut<std::string,GfxMaterialFxParamBase*>::const_iterator it=ParamInstances.begin(); it!=ParamInstances.end(); it++ )
		for( size_t it=0; it<isize; it++ )
		{
			GfxMaterialFxParamBase* ParamInst = ParamInstances.GetItemAtIndex(it).second;

			bool hashmatch = ( ParamInst->GetRecord().mTargetHash == ucurtarghash );
			ParamInst->GetRecord().mTargetHash = ucurtarghash;

			if( ParamInst->IsBindable() )
			{
				switch( ParamInst->GetRecord().meBindingScope )
				{
					case FxParamRec::ESCOPE_PERMATERIALINST:
					{
						
						bool bfximatch = (this==pTarg->FXI()->GetLastFxMaterial());
						bool bpassmatch = (iPass==LastPass);
						if( (false==bfximatch) || (false==bpassmatch) )
						{
							ParamInst->Bind( mEffectInstance.mpEffect, pTarg );
						}
						break;
					}
					case FxParamRec::ESCOPE_PERFRAME:
					{
						if( false == hashmatch )
						{
							ParamInst->Bind( mEffectInstance.mpEffect, pTarg );
						}
						break;
					}
					case FxParamRec::ESCOPE_PEROBJECT:
					{	ParamInst->Bind( mEffectInstance.mpEffect, pTarg );
						break;
					}
				}
			}
		}

		LastPass = iPass;

		mLightingInterface.ApplyLighting( pTarg, iPass );
		////////////////////////////////

		pTarg->FXI()->CommitParams();

		rval=true;
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialFx::EndPass( GfxTarget *pTarg )
{
	pTarg->FXI()->EndPass( mEffectInstance.mpEffect );
}

///////////////////////////////////////////////////////////////////////////////

int GfxMaterialFx::BeginBlock( GfxTarget *pTarg, const RenderContextInstData &MatCtx )
{
	int inumpasses = 0;

	if( mEffectInstance.mpEffect )
	{
		//const DagRenderableContextData& DRCD = MatCtx.GetDagRenderable()->GetDagObject()->GetRenderableContextData();
		//int   ActiveLayerIndex = MatCtx.GetRenderer()->GetActiveDisplayLayer()->GetLayerIndex();

		GfxBuffer* ShadowBuffer = 0; //pTarg->GetShadowBuffer();

		bool bpick = pTarg->FBI()->IsPickState();
		bool bisshadowcaster = false; //(DRCD.GetRenderFlags()&2)&&(1==ActiveLayerIndex);
		bool bisshadowreciever = false; //(false==bisshadowcaster)&&(0==ActiveLayerIndex)&&(ShadowBuffer!=0);
		bool bisskinned = MatCtx.IsSkinned();
		bool bislightmap = MatCtx.IsLightMapped() && (mActiveLightMapTechnique!=0);
		bool bisvertexlit = MatCtx.IsVertexLit() && mActiveVertexLightTechnique;

		int iefx = (int(bpick)<<0) | (int(bisshadowcaster)<<1) | (int(bisskinned)<<2);

		const FxShaderTechnique* htek = 0;

		switch( iefx )
		{
			case 0:
				htek = bisvertexlit ? mActiveVertexLightTechnique :
						bislightmap ? mActiveLightMapTechnique : mActiveTechnique;

				if( bisvertexlit )
				{
				}
				else if( bislightmap )
				{
					mLightMapTexture = MatCtx.GetLightMap();
				}
				else if( gEnableLightPreview )
				{
					if( mActiveLightPreviewTechnique )
					{
						htek=mActiveLightPreviewTechnique;
					}
				}
				break;
			case 1:
				htek = mActivePickTechnique;
				break;
			case 2:
				htek = mActiveShadowTechnique;
				break;
			case 3:
				break;

			case 4: // skinned
				htek = mActiveSkinnedTechnique;
				break;
			case 5:
				htek = mActiveSkinnedPickTechnique;
				break;
			case 6:
				htek = mActiveSkinnedShadowTechnique;
				break;
			case 7:
				break;

		}

		//////////////////////////////
		for( int i=0; i<kMaxEngineParamFloats; i++ )
		{
			mEngineParamFloats[i] = MatCtx.GetEngineParamFloat(i);
		}

		//////////////////////////////
		if( 0 == htek )
		{
			printf( "effect<%s> NoTechnique<%s> iefx<%d>\n", mAssetPath.c_str(), mMainTechniqueName.c_str(), iefx );
			return 0;
		}
		OrkAssert( htek != 0 );
		pTarg->FXI()->BindTechnique( mEffectInstance.mpEffect, htek );
		inumpasses = pTarg->FXI()->BeginBlock( mEffectInstance.mpEffect, MatCtx );
		//////////////////////////////
		//pTarg->FXI()->BindParamBool( mEffectInstance.mpEffect, mIsShadowCasterParam, bisshadowcaster );
		//pTarg->FXI()->BindParamBool( mEffectInstance.mpEffect, mIsShadowRecieverParam, bisshadowreciever );
		//pTarg->FXI()->BindParamBool( mEffectInstance.mpEffect, mIsSkinnedParam, bisskinned );
		//pTarg->FXI()->BindParamBool( mEffectInstance.mpEffect, mIsPickParam, bpick );
		//////////////////////////////

		const ork::lev2::RenderContextFrameData* framedata = pTarg->GetRenderContextFrameData();
		const ork::CCameraData* cdata = framedata->GetCameraData();

		mScreenZDir = cdata->GetZNormal();

	}

	return inumpasses;
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialFx::EndBlock( GfxTarget *pTarg )
{
	if( mEffectInstance.mpEffect )
	{
		pTarg->FXI()->EndBlock( mEffectInstance.mpEffect );
	}

	pTarg->FXI()->InvalidateStateBlock();
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialFx::Update( void )
{

}

///////////////////////////////////////////////////////////////////////////////

template<>
void GfxMaterialFxParam<bool>::Bind( FxShader* fxh, GfxTarget *ptarg )
{
	ptarg->FXI()->BindParamBool( fxh, GetRecord().mParameterHandle, GetValue(ptarg) );
}

///////////////////////////////////////////////////////////////////////////////

template<>
void GfxMaterialFxParam<int>::Bind( FxShader* fxh, GfxTarget *ptarg )
{
	OrkAssert(false);
	//const std::string& paramname = GetRecord().mParameterName;
	//ptarg->FXI()->BindParamFloat( fxh, GetRecord().mParameterHandle, GetValue(ptarg) );
}

///////////////////////////////////////////////////////////////////////////////

template<>
void GfxMaterialFxParam<CReal>::Bind( FxShader* fxh, GfxTarget *ptarg )
{
	const std::string& paramname = GetRecord().mParameterName;
	ptarg->FXI()->BindParamFloat( fxh, GetRecord().mParameterHandle, GetValue(ptarg) );
}

///////////////////////////////////////////////////////////////////////////////

template<>
void GfxMaterialFxParam<CVector2>::Bind( FxShader* fxh, GfxTarget *ptarg )
{
	CVector4 Value( GetValue(ptarg) );
	ptarg->FXI()->BindParamVect2( fxh, GetRecord().mParameterHandle, Value );
}

///////////////////////////////////////////////////////////////////////////////

template<>
void GfxMaterialFxParam<CVector3>::Bind( FxShader* fxh, GfxTarget *ptarg )
{
	CVector3 Value = GetValue(ptarg);
	ptarg->FXI()->BindParamVect3( fxh, GetRecord().mParameterHandle, Value );
}

///////////////////////////////////////////////////////////////////////////////

template<>
void GfxMaterialFxParam<CVector4>::Bind( FxShader* fxh, GfxTarget *ptarg )
{
	CVector4 Value = GetValue(ptarg);
	ptarg->FXI()->BindParamVect4( fxh, GetRecord().mParameterHandle, Value );
}

///////////////////////////////////////////////////////////////////////////////

template<>
void GfxMaterialFxParam<CMatrix4>::Bind( FxShader* fxh, GfxTarget *ptarg )
{
	CMatrix4 Value( GetValue(ptarg) );
	ptarg->FXI()->BindParamMatrix( fxh, GetRecord().mParameterHandle, Value );
}

///////////////////////////////////////////////////////////////////////////////

template<>
void GfxMaterialFxParam<CMatrix3>::Bind( FxShader* fxh, GfxTarget *ptarg )
{
	CMatrix3 Value( GetValue(ptarg) );
	ptarg->FXI()->BindParamMatrix( fxh, GetRecord().mParameterHandle, Value );
}

///////////////////////////////////////////////////////////////////////////////

template<>
void GfxMaterialFxParam<lev2::Texture *>::Bind( FxShader* fxh, GfxTarget *ptarg )
{
	Texture *ptex = GetValue(ptarg);

	//const char* paramname = GetRecord().mParameterName.c_str();
	//const std::string texfname = ptex->GetProperty( "filename" );
	//const char* ptexfname = texfname.c_str();
	//if( strstr( ptexfname, "flare" ) != 0 )
	//{
	//	orkprintf( "yo\n" );
	//}

	ptarg->FXI()->BindParamCTex( fxh, GetRecord().mParameterHandle, ptex );

}

///////////////////////////////////////////////////////////////////////////////

template<>
void GfxMaterialFxParam<std::string>::Bind( FxShader* fxh, GfxTarget *ptarg )
{

}

///////////////////////////////////////////////////////////////////////////////

template<>
std::string GfxMaterialFxParam<std::string>::GetValueString( void ) const
{
	return GetValue(0);
}
template<>
std::string GfxMaterialFxParam<int>::GetValueString( void ) const
{
	PropTypeString valstr;
	CPropType<int>::ToString( GetValue(0), valstr );
	return valstr.c_str();
}
template<>
std::string GfxMaterialFxParam<CReal>::GetValueString( void ) const
{
	PropTypeString valstr;
	CPropType<CReal>::ToString( GetValue(0), valstr );
	return valstr.c_str();
}
template<>
std::string GfxMaterialFxParam<CVector2>::GetValueString( void ) const
{
	PropTypeString valstr;
	CPropType<CVector2>::ToString( GetValue(0), valstr );
	return valstr.c_str();
}
template<>
std::string GfxMaterialFxParam<CVector3>::GetValueString( void ) const
{
	PropTypeString valstr;
	CPropType<CVector3>::ToString( GetValue(0), valstr );
	return valstr.c_str();
}
template<>
std::string GfxMaterialFxParam<CVector4>::GetValueString( void ) const
{
	PropTypeString valstr;
	CPropType<CVector4>::ToString( GetValue(0), valstr );
	return valstr.c_str();
}
template<>
std::string GfxMaterialFxParam<CMatrix3>::GetValueString( void ) const
{
	PropTypeString valstr;
	CPropType<CMatrix3>::ToString( GetValue(0), valstr );
	return valstr.c_str();
}
template<>
std::string GfxMaterialFxParam<CMatrix4>::GetValueString( void ) const
{
	PropTypeString valstr;
	CPropType<CMatrix4>::ToString( GetValue(0), valstr );
	return valstr.c_str();
}
template<>
std::string GfxMaterialFxParam<lev2::Texture *>::GetValueString( void ) const
{
	PropTypeString valstr;
	//CPropType<CAsset *>::ToString( (CAsset *) GetValue(0), valstr );
	return valstr.c_str();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static ork::fixed_pool<FxMatrixBlockApplicator,1> MtxBlockApplicators;
static ork::fixed_pool<FxMatrixBlockApplicator,256> MtxApplicators;

void FxMatrixBlockApplicator::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

FxMatrixBlockApplicator::FxMatrixBlockApplicator( MaterialInstItemMatrixBlock*	mtxblockitem, const GfxMaterialFx* pmat )
	: mMatrixBlockItem( mtxblockitem )
	, mMaterial( pmat )
{
}

///////////////////////////////////////////////////////////////////////////////

void FxMatrixBlockApplicator::ApplyToTarget( GfxTarget *pTARG ) // virtual
{
	size_t inumbones = mMatrixBlockItem->GetNumMatrices();
	const CMatrix4* Matrices =  mMatrixBlockItem->GetMatrices();
	FxShader* hshader = mMaterial->GetEffectInstance().mpEffect;
	//CMatrix4 iwmat;
	//iwmat.GEMSInverse(pTARG->MTXI()->RefMVMatrix());
	pTARG->FXI()->BindParamMatrixArray( hshader, mMaterial->mBonesParam, Matrices, (int) inumbones );
	pTARG->FXI()->CommitParams();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void GfxMaterialFx::UpdateMVPMatrix( GfxTarget *pTARG )
{
	FxShader* hshader = GetEffectInstance().mpEffect;

	if( mWorldMtxParam )
	{
		pTARG->FXI()->BindParamMatrix( hshader, mWorldMtxParam, pTARG->MTXI()->RefMMatrix() );
	}
	if( mWorldViewMtxParam )
	{
		pTARG->FXI()->BindParamMatrix( hshader, mWorldViewMtxParam, pTARG->MTXI()->RefMVMatrix() );
	}
	if( mWorldViewProjectionMtxParam )
	{
		pTARG->FXI()->BindParamMatrix( hshader, mWorldViewProjectionMtxParam, pTARG->MTXI()->RefMVPMatrix() );
	}
	pTARG->FXI()->CommitParams();
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialFx::BindMaterialInstItem( MaterialInstItem* pitem ) const
{
	///////////////////////////////////

	MaterialInstItemMatrixBlock* mtxblockitem = rtti::autocast(pitem);

	if( mtxblockitem )
	{
		if( mBonesParam->GetPlatformHandle() )
		{
			FxMatrixBlockApplicator* pyo = MtxBlockApplicators.allocate();
			OrkAssert( pyo!= 0 );
			new (pyo) FxMatrixBlockApplicator( mtxblockitem, this );
			mtxblockitem->SetApplicator( pyo );
		}
		return;
	}

	///////////////////////////////////

	/*MaterialInstItemMatrix* mtxitem = rtti::autocast(pitem);

	if( mtxitem )
	{
		FxMatrixBlockApplicator* pyo =  MtxApplicators.allocate();
		OrkAssert( pyo!= 0 );
		new (pyo) FxMatrixBlockApplicator( mtxitem, this );
		mtxitem->SetApplicator( pyo );
		return;
	}*/

	///////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void GfxMaterialFx::UnBindMaterialInstItem( MaterialInstItem* pitem ) const
{
	///////////////////////////////////

	MaterialInstItemMatrixBlock* mtxblockitem = rtti::autocast(pitem);

	if( mtxblockitem )
	{
		if( mBonesParam->GetPlatformHandle() )
		{
			FxMatrixBlockApplicator* wiimtxblkapp = rtti::autocast(mtxblockitem->mApplicator);
			if( wiimtxblkapp )
			{
				MtxBlockApplicators.deallocate( wiimtxblkapp );
			}
		}
		return;
	}

	///////////////////////////////////

	MaterialInstItemMatrix* mtxitem = rtti::autocast(pitem);

	if( mtxitem )
	{
		FxMatrixBlockApplicator* wiimtxapp = rtti::autocast(mtxitem->mApplicator);
		if( wiimtxapp )
		{
			MtxApplicators.deallocate( wiimtxapp );
		}
		return;
	}

	///////////////////////////////////
}


void GfxMaterialFx::SetMaterialProperty( const char* prop, const char* val ) // virtual
{
	if( 0 == strcmp(prop,"shader") )
	{
		//printf( "LOADING<%s>\n", val );
		LoadEffect( val );
		asset::AssetManager<lev2::FxShaderAsset>::AutoLoad();
		
		GfxMaterialFxParamArtist<std::string>* paramstr = new GfxMaterialFxParamArtist<std::string>;
		std::string descstr = ork::CreateFormattedString("morkshader<%s>", val);
		paramstr->mValue = descstr;
		paramstr->SetBindable(false);
		paramstr->GetRecord().mParameterName = "description";
		paramstr->GetRecord().meParameterType = EPROPTYPE_STRING;
		AddParameter( paramstr );


	}
	else if( 0 == strcmp(prop,"technique") )
	{
		GfxMaterialFxParamArtist<std::string>* paramstr = new GfxMaterialFxParamArtist<std::string>;
		paramstr->mValue = val;
		paramstr->SetBindable(false);
		paramstr->GetRecord().mParameterName = "technique";
		paramstr->GetRecord().meParameterType = EPROPTYPE_STRING;
		AddParameter( paramstr );
	}
	else
	{
	
		const orkmap<std::string,const FxShaderParam*> & ParamNameMap = mEffectInstance.mpEffect->GetParametersByName();

		orkmap<std::string,const FxShaderParam*>::const_iterator itparam = ParamNameMap.find(prop);

		const FxShaderParam* param = (itparam==ParamNameMap.end()) ? 0 : itparam->second;
		
		//printf( "GfxMaterialFx::SetMaterialProperty() prop<%s> val<%s> param<%p>\n", prop, val, param );
		
/*			GfxMaterialFxParamArtist<lev2::Texture*> *paramf = new GfxMaterialFxParamArtist<lev2::Texture*>;
							AssetPath texname ( paramval );
							Texture* ptex(NULL);
							if(0 != strcmp( texname.c_str(), "None" ) )
							{
								ork::lev2::TextureAsset* ptexa = asset::AssetManager<TextureAsset>::Create(texname.c_str());
								ptex = ptexa ? ptexa->GetTexture() : 0;
#if defined(_DEBUG)				
								if( ptex )
								{
									ptex->SetProperty( "filename", texname.c_str() );
								}
#endif
							}
							orkprintf( "ModelIO::LoadTexture mdl<%s> tex<%s> ptex<%p>\n", "", texname.c_str(), ptex );
							paramf->mValue = ptex;
							param = paramf;
							break;
*/
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template class GfxMaterialFxParamArtist<int>;
template class GfxMaterialFxParamArtist<float>;
template class GfxMaterialFxParamArtist<CVector2>;
template class GfxMaterialFxParamArtist<CVector3>;
template class GfxMaterialFxParamArtist<CVector4>;
template class GfxMaterialFxParamArtist<CMatrix3>;
template class GfxMaterialFxParamArtist<ork::lev2::Texture*>;
	
} }

typedef ork::lev2::GfxMaterialFxParam<int>							OrkLev2GfxMaterialFxParamInt;
typedef ork::lev2::GfxMaterialFxParamArtist<int>					OrkLev2GfxMaterialFxParamIntArtist;
typedef ork::lev2::GfxMaterialFxParamEngine<int>					OrkLev2GfxMaterialFxParamIntEngine;

typedef ork::lev2::GfxMaterialFxParam<float>						OrkLev2GfxMaterialFxParamFloat;
typedef ork::lev2::GfxMaterialFxParamArtist<float>					OrkLev2GfxMaterialFxParamFloatArtist;
typedef ork::lev2::GfxMaterialFxParamEngine<float>					OrkLev2GfxMaterialFxParamFloatEngine;

typedef ork::lev2::GfxMaterialFxParam<ork::CVector2>				OrkLev2GfxMaterialFxParamCVector2;
typedef ork::lev2::GfxMaterialFxParamArtist<ork::CVector2>			OrkLev2GfxMaterialFxParamCVector2Artist;
typedef ork::lev2::GfxMaterialFxParamEngine<ork::CVector2>			OrkLev2GfxMaterialFxParamCVector2Engine;

typedef ork::lev2::GfxMaterialFxParam<ork::CVector3>				OrkLev2GfxMaterialFxParamCVector3;
typedef ork::lev2::GfxMaterialFxParamArtist<ork::CVector3>			OrkLev2GfxMaterialFxParamCVector3Artist;
typedef ork::lev2::GfxMaterialFxParamEngine<ork::CVector3>			OrkLev2GfxMaterialFxParamCVector3Engine;

typedef ork::lev2::GfxMaterialFxParam<ork::CVector4>				OrkLev2GfxMaterialFxParamCVector4;
typedef ork::lev2::GfxMaterialFxParamArtist<ork::CVector4>			OrkLev2GfxMaterialFxParamCVector4Artist;
typedef ork::lev2::GfxMaterialFxParamEngine<ork::CVector4>			OrkLev2GfxMaterialFxParamCVector4Engine;

typedef ork::lev2::GfxMaterialFxParam<ork::CMatrix3>				OrkLev2GfxMaterialFxParamCMatrix3;
typedef ork::lev2::GfxMaterialFxParamArtist<ork::CMatrix3>			OrkLev2GfxMaterialFxParamCMatrix3Artist;
typedef ork::lev2::GfxMaterialFxParamEngine<ork::CMatrix3>			OrkLev2GfxMaterialFxParamCMatrix3Engine;

typedef ork::lev2::GfxMaterialFxParam<ork::CMatrix4>				OrkLev2GfxMaterialFxParamCMatrix4;
typedef ork::lev2::GfxMaterialFxParamArtist<ork::CMatrix4>			OrkLev2GfxMaterialFxParamCMatrix4Artist;
typedef ork::lev2::GfxMaterialFxParamEngine<ork::CMatrix4>			OrkLev2GfxMaterialFxParamCMatrix4Engine;

typedef ork::lev2::GfxMaterialFxParam<std::string>					OrkLev2GfxMaterialFxParamStdString;
typedef ork::lev2::GfxMaterialFxParamArtist<std::string>			OrkLev2GfxMaterialFxParamStdStringArtist;
typedef ork::lev2::GfxMaterialFxParamEngine<std::string>			OrkLev2GfxMaterialFxParamStdStringEngine;

typedef ork::lev2::GfxMaterialFxParam<ork::lev2::Texture*>			OrkLev2GfxMaterialFxParamTex;
typedef ork::lev2::GfxMaterialFxParamArtist<ork::lev2::Texture*>	OrkLev2GfxMaterialFxParamTexArtist;
typedef ork::lev2::GfxMaterialFxParamEngine<ork::lev2::Texture*>	OrkLev2GfxMaterialFxParamTexEngine;

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamInt, "FxParamInt" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamIntArtist, "FxParamArtistInt" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamIntEngine, "FxParamEngineInt" );

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamFloat, "FxParamFloat" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamFloatArtist, "FxParamArtistFloat" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamFloatEngine, "FxParamEngineFloat" );

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCVector2, "FxParamFloat2" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCVector2Artist, "FxParamArtistFloat2" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCVector2Engine, "FxParamEngineFloat2" );

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCVector3, "FxParamFloat3" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCVector3Artist, "FxParamArtistFloat3" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCVector3Engine, "FxParamEngineFloat3" );

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCVector4, "FxParamFloat4" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCVector4Artist, "FxParamArtistFloat4" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCVector4Engine, "FxParamEngineFloat4" );

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCMatrix3, "FxParamMatrix3" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCMatrix3Artist, "FxParamArtistMatrix3" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCMatrix3Engine, "FxParamEngineMatrix3" );

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCMatrix4, "FxParamMatrix4" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCMatrix4Artist, "FxParamArtistMatrix4" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamCMatrix4Engine, "FxParamEngineMatrix4" );

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamStdString, "FxParamString" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamStdStringArtist, "FxParamArtistString" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamStdStringEngine, "FxParamEngineString" );

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamTex, "FxParamTexture" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamTexArtist, "FxParamArtistTexture" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( OrkLev2GfxMaterialFxParamTexEngine, "FxParamEngineTexture" );

