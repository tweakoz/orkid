////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/application/application.h>
#if defined(USE_FCOLLADA)
#include <ork/kernel/prop.h>

#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/collada/daeutil.h>
#include <ork/lev2/lev2_asset.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {

///////////////////////////////////////////////////////////////////////////////

bool CColladaModel::ParseMaterialBindings( void )
{
	return ParseColladaMaterialBindings( *mDocument, mMaterialSemanticBindingMap );
}

///////////////////////////////////////////////////////////////////////////////

const SColladaMaterial & CColladaModel::GetMaterialFromShadingGroup( const std::string& ShadingGroupName ) const
{
	static const SColladaMaterial kDefaultMaterial;

	MeshUtil::material_semanticmap_t::const_iterator itsh = mMaterialSemanticBindingMap.find( ShadingGroupName );

	if( mMaterialSemanticBindingMap.end() != itsh )
	{
		const std::string & MaterialName = itsh->second.mMaterialDaeId;

		orkmap<std::string,SColladaMaterial>::const_iterator itmat = mMaterialMap.find( ShadingGroupName );

		if( mMaterialMap.end() != itmat )
		{
			return itmat->second;
		}
	}
	return kDefaultMaterial;
}

///////////////////////////////////////////////////////////////////////////////

struct StandardEffectTexGetter
{

	static void GetImgData( const FCDImage *pimg, std::string & TexFileName )
	{
		TexFileName = pimg->GetFilename();
	}
	static void GetTexData( const FCDTexture *ptex, std::string & TexFileName, float &RepeatU, float &RepeatV )
	{
		const FCDImage *Image = ptex->GetImage();

		GetImgData( Image, TexFileName );



		const FCDExtra *TexExtra = ptex->GetExtra();
		const FCDExtra *ImageExtra = Image->GetExtra();
		const FCDETechnique* TexMayaTek = TexExtra->GetDefaultType()->FindTechnique("MAYA");
		const FCDETechnique* ImgMayaTek = ImageExtra->GetDefaultType()->FindTechnique("MAYA");

		printf( "TEXNAME<%s>\n", TexFileName.c_str() );

		///////////////////////////////////////////////////////////
		// check image name (or image name base)
		///////////////////////////////////////////////////////////

		///////////////////////////////////////////////////////////
		// check for image sequence
		///////////////////////////////////////////////////////////
		if( ImgMayaTek )
		{
			const FCDENode *ImgSeqNode = ImgMayaTek->FindChildNode( "image_sequence" );

			int imgseq = 0;

			if( ImgSeqNode )
			{
				const fchar *ImgSeqVal = ImgSeqNode->GetContent();

				imgseq = atoi( ImgSeqVal );

				if( imgseq )
				{
					orkprintf( "flagged as image sequence!\n" );

					file::Path ImgBasePath( TexFileName.c_str() );

					std::string ext = ImgBasePath.GetExtension().c_str();

					ImgBasePath.SetExtension( "" );

					std::string base_index = ImgBasePath.GetExtension().c_str();

					ImgBasePath.SetExtension( "" );

					int ibidx = atoi( base_index.c_str() );

					std::string BaseName = ImgBasePath.GetName().c_str();

					ImgBasePath.SetFile( "" );

					std::string wildcard = CreateFormattedString("%s.*.%s",BaseName.c_str(),ext.c_str());
					orkset<file::Path::NameType> files = CFileEnv::filespec_search_sorted( wildcard.c_str(), ImgBasePath.c_str() );
					
					for( orkset<file::Path::NameType>::const_iterator it=files.begin(); it!=files.end(); it++ )
					{
						const file::Path::NameType& filename = (*it);

						orkprintf( "found sequence image <%s>\n", filename.c_str() );
					}
				}
			}
			
		}

		///////////////////////////////////////////////////////////
		// check repeat UV
		///////////////////////////////////////////////////////////

		RepeatU = 1.0f;
		RepeatV = 1.0f;
		if( TexMayaTek )
		{
			const FCDENode *RepeatUNode = TexMayaTek->FindChildNode( "repeatU" );
			const FCDENode *RepeatVNode = TexMayaTek->FindChildNode( "repeatV" );

			const fchar *prpU = RepeatUNode ? RepeatUNode->GetContent() : "1.0f"; 
			const fchar *prpV = RepeatVNode ? RepeatVNode->GetContent() : "1.0f"; 

			RepeatU = float(atof( prpU ));
			RepeatV = float(atof( prpV ));


		}

		///////////////////////////////////////////////////////////
	}

	static void CheckChannel( FCDEffectStandard * StdProf, uint32 texbucket, int isubtex, SColladaMaterialChannel & RefMatCh )
	{
		int TexCount( StdProf->GetTextureCount(texbucket) );

		if( isubtex < TexCount )
		{
			const FCDTexture* ptex = StdProf->GetTexture(texbucket,isubtex); 
			GetTexData( ptex, RefMatCh.mTextureName, RefMatCh.mRepeatU, RefMatCh.mRepeatV );
		}
	}


	static void DoIt( FCDEffectStandard * StdProf, SColladaMaterial & ColMat )
	{
		switch( ColMat.mLightingType )
		{
			case SColladaMaterial::ELIGHTING_LAMBERT:
			case SColladaMaterial::ELIGHTING_BLINN:
			case SColladaMaterial::ELIGHTING_PHONG:
				CheckChannel( StdProf, FUDaeTextureChannel::DIFFUSE, 0, ColMat.mDiffuseMapChannel );
				CheckChannel( StdProf, FUDaeTextureChannel::SPECULAR, 0, ColMat.mSpecularMapChannel );
				CheckChannel( StdProf, FUDaeTextureChannel::BUMP, 0, ColMat.mNormalMapChannel );
				CheckChannel( StdProf, FUDaeTextureChannel::AMBIENT, 0, ColMat.mAmbientMapChannel );
				break;
		}

	}
};

///////////////////////////////////////////////////////////////////////////////

void SColladaMaterial::ParseStdMaterial( FCDEffectStandard *StdProf )
{
	FCDEffectStandard::LightingType lighting_type = StdProf->GetLightingType();

	mSpecularPower = StdProf->GetShininess();
	mSpecularPower = (0.0f==mSpecularPower) ? 256.0f : (10.0f / mSpecularPower);

	printf( "ParseStdMaterial\n");
	switch( lighting_type )
	{
		case FCDEffectStandard::LAMBERT:
			mLightingType = SColladaMaterial::ELIGHTING_LAMBERT;
			break;
		case FCDEffectStandard::BLINN:
			mLightingType = SColladaMaterial::ELIGHTING_BLINN;
			break;
		case FCDEffectStandard::PHONG:
			mLightingType = SColladaMaterial::ELIGHTING_PHONG;
			break;
		default:
			mLightingType = SColladaMaterial::ELIGHTING_NONE;
			break;
	}

	StandardEffectTexGetter::DoIt( StdProf, *this );

	const FMVector4 & EmColor = StdProf->GetEmissionColor();
	mEmissiveColor.SetXYZ( EmColor.x, EmColor.y, EmColor.z );
	mEmissiveColor.SetW( 1.0f );

	const FMVector4 & TransColor = StdProf->GetTranslucencyColor();
	mTransparencyMode = StdProf->GetTransparencyMode();
	mTransparencyColor.SetXYZ( TransColor.x, TransColor.y, TransColor.z );
	mTransparencyColor.SetW( TransColor.x );

}

///////////////////////////////////////////////////////////////////////////////

void SColladaMaterial::ParseFxMaterial( FCDMaterial *FxProf )
{
	int inumparams = FxProf->GetEffectParameterCount();

	if( inumparams )
	{
		////////////////////////////////////////////////////
		ork::lev2::GfxMaterialFx* FxMaterial = new ork::lev2::GfxMaterialFx;
		mpOrkMaterial = FxMaterial;
		////////////////////////////////////////////////////

		for( int ip=0; ip<inumparams; ip++ )
		{
			const FCDEffectParameter* Param = FxProf->GetEffectParameter(ip);

			FCDEffectParameter::Type ParamType = Param->GetType();
			const fm::string & ParamSemantic = Param->GetSemantic();
			const fm::string & ParamReference = Param->GetReference();
			int imatlen = mMaterialName.length();

			std::string parameter_name( ParamReference.c_str() );

			printf( "mtl<%s> param<%s>\n", mMaterialName.c_str(), parameter_name.c_str() );
			///////////////////////////////////////////////////////////////////////
			// Skip parameters with the word "engine" in them
			if(parameter_name.find("engine") != std::string::npos)
				continue;

			///////////////////////////////////////////////////////////////////////
			// Remove Leading material name in the reference name

			std::string::size_type it = parameter_name.rfind(mMaterialName + "_");
			if(it != std::string::npos)
			{
				int iparlen = parameter_name.length();

				parameter_name = parameter_name.substr(it + imatlen + 1);
			}

			if( parameter_name == "BaseColor" )
			{
				printf( "yo\n" );
			}
			int inumanno = Param->GetAnnotationCount();

			for(int ia = 0; ia < inumanno; ++ia)
			{
				const FCDEffectParameterAnnotation* Anno = Param->GetAnnotation(ia);

				const fm::string& AnnoName = Anno->name;
				const fm::string& AnnoVal = Anno->value;

				printf( "annoname<%s>\n", AnnoName.c_str() );
				printf( "annoval<%s>\n", AnnoVal.c_str() );
				///////////////////////////////////////////////////////////////////////
				// Remove Leading material name in the reference name

				std::string anno_name( AnnoName.c_str() );

				std::string::size_type it = anno_name.find( mMaterialName );

				if( it != std::string::npos )
				{
					int ianolen = anno_name.length();
					anno_name = anno_name.substr( it+imatlen+1, (ianolen-(1+imatlen)) );

				}
				///////////////////////////////////////////////////////////////////////


				mAnnotations[anno_name] = AnnoVal.c_str();
			}

			std::string ptypeanno = "";
			std::map<std::string,std::string>::const_iterator itpta = mAnnotations.find("CgFxParamType");
			if( itpta != mAnnotations.end() )
			{
				ptypeanno = itpta->second;
			}

			ork::lev2::GfxMaterialFxParamBase* param = 0;

			switch( ParamType )
			{
				case FCDEffectParameter::FLOAT:
				{
					FCDEffectParameterFloat* FloatColladaParam = (FCDEffectParameterFloat*) Param;

					ork::lev2::GfxMaterialFxParamArtist<float> *paramf = new ork::lev2::GfxMaterialFxParamArtist<float>;
					paramf->mValue = FloatColladaParam->GetValue();
					param=paramf;
					param->GetRecord().meParameterType = CPropType<float>::GetType();
					break;
				}
				case FCDEffectParameter::FLOAT2:
				{
					FCDEffectParameterFloat2* Float2ColladaParam = (FCDEffectParameterFloat2*) Param;

					FMVector2 fval = Float2ColladaParam->GetValue();

					ork::lev2::GfxMaterialFxParamArtist<CVector2> *paramf = new ork::lev2::GfxMaterialFxParamArtist<CVector2>;
					paramf->mValue = CVector2( fval.x, fval.y );
					param=paramf;
					param->GetRecord().meParameterType = CPropType<CVector2>::GetType();
					break;
				}
				case FCDEffectParameter::FLOAT3:
				{
					FCDEffectParameterFloat3* Float3ColladaParam = (FCDEffectParameterFloat3*) Param;

					FMVector3 fval = Float3ColladaParam->GetValue();
					ork::lev2::GfxMaterialFxParamArtist<CVector3> *paramf = new ork::lev2::GfxMaterialFxParamArtist<CVector3>;
					paramf->mValue = CVector3( fval.x, fval.y, fval.z );
					param=paramf;
					param->GetRecord().meParameterType = CPropType<CVector3>::GetType();
					break;
				}
				case FCDEffectParameter::VECTOR:
				{
					FCDEffectParameterVector* Float4ColladaParam = (FCDEffectParameterVector*) Param;
					FMVector4 fval = Float4ColladaParam->GetValue();
					ork::lev2::GfxMaterialFxParamArtist<CVector4> *paramf = new ork::lev2::GfxMaterialFxParamArtist<CVector4>;
					paramf->mValue = CVector4( fval.x, fval.y, fval.z, fval.w );
					param=paramf;
					param->GetRecord().meParameterType = CPropType<CVector4>::GetType();
					break;
				}
				case FCDEffectParameter::BOOLEAN:
				{
					break;
				}
				case FCDEffectParameter::INTEGER:
				{
					FCDEffectParameterInt* IntColladaParam = (FCDEffectParameterInt*) Param;
					int ival = IntColladaParam->GetValue();
					ork::lev2::GfxMaterialFxParamArtist<int> *paramf = new ork::lev2::GfxMaterialFxParamArtist<int>;
					paramf->mValue = ival;
					paramf->GetRecord().meParameterType = EPROPTYPE_S32;
					param=paramf;
					break;
				}
				case FCDEffectParameter::MATRIX:
				{
					ork::lev2::GfxMaterialFxParamArtist<CMatrix4> *paramf = new ork::lev2::GfxMaterialFxParamArtist<CMatrix4>;
					//paramf->mValue = CPropType<CMatrix4>::FromString( CgFxParamValue );
					param=paramf;
					param->GetRecord().meParameterType = CPropType<CMatrix4>::GetType();
					break;
				}
				case FCDEffectParameter::STRING:
				{
					if( ptypeanno=="float" )
					{
						FCDEffectParameterString *ParamString = (FCDEffectParameterString*) Param;

						ork::lev2::GfxMaterialFxParamArtist<float> *paramf = new ork::lev2::GfxMaterialFxParamArtist<float>;
						paramf->mValue = float(atof( ParamString->GetValue().c_str() ));
						param=paramf;
						param->GetRecord().meParameterType = CPropType<float>::GetType();
					}
					else if( ptypeanno=="int" )
					{
						FCDEffectParameterString *ParamString = (FCDEffectParameterString*) Param;

						ork::lev2::GfxMaterialFxParamArtist<int> *paramf = new ork::lev2::GfxMaterialFxParamArtist<int>;
						paramf->mValue = atoi( ParamString->GetValue().c_str() );
						param=paramf;
						param->GetRecord().meParameterType = CPropType<int>::GetType();
					}
					else if( ptypeanno=="Vector3" )
					{
						FCDEffectParameterString *ParamString = (FCDEffectParameterString*) Param;

						ork::lev2::GfxMaterialFxParamArtist<CVector3> *paramf = new ork::lev2::GfxMaterialFxParamArtist<CVector3>;
						//paramf->mValue = CVector3( fval.x, fval.y, fval.z );
						param=paramf;
						param->GetRecord().meParameterType = CPropType<CVector3>::GetType();
						const char* pvalstr = ParamString->GetValue().c_str();
						printf( "valstr<%s>\n", pvalstr );
						orkvector<std::string> splitvect;
						ork::SplitString( pvalstr, splitvect, " " );
						OrkAssert( splitvect.size()==3 );
						float fx = float(atof( splitvect[0].c_str() ));
						float fy = float(atof( splitvect[1].c_str() ));
						float fz = float(atof( splitvect[2].c_str() ));
						//"1 1 1"
						paramf->mValue = CVector3( fx, fy, fz );
					}
					else if( ptypeanno=="Vector4" )
					{
						FCDEffectParameterString *ParamString = (FCDEffectParameterString*) Param;

						ork::lev2::GfxMaterialFxParamArtist<CVector4> *paramf = new ork::lev2::GfxMaterialFxParamArtist<CVector4>;
						//paramf->mValue = CVector3( fval.x, fval.y, fval.z );
						param=paramf;
						param->GetRecord().meParameterType = CPropType<CVector4>::GetType();
						const char* pvalstr = ParamString->GetValue().c_str();
						printf( "valstr<%s>\n", pvalstr );
						orkvector<std::string> splitvect;
						ork::SplitString( pvalstr, splitvect, " " );
						OrkAssert( splitvect.size()==4 );
						float fx = float(atof( splitvect[0].c_str() ));
						float fy = float(atof( splitvect[1].c_str() ));
						float fz = float(atof( splitvect[2].c_str() ));
						float fw = float(atof( splitvect[2].c_str() ));
						//"1 1 1"
						paramf->mValue = CVector4( fx, fy, fz, fw );
					}
					else
					{

						FCDEffectParameterString *ParamString = (FCDEffectParameterString*) Param;
						PropTypeString CgFxParamValue( ParamString->GetValue().c_str() );
						ork::lev2::GfxMaterialFxParamArtist<std::string> *paramf = new ork::lev2::GfxMaterialFxParamArtist<std::string>;
						paramf->mValue = CgFxParamValue.c_str();
						param=paramf;
						param->GetRecord().meParameterType = CPropType<std::string>::GetType();
						param->SetBindable(false);
					}
					break;
				}
				case FCDEffectParameter::SAMPLER:
				{
					///////////////////////////////////////////////
					std::string CgFxParamName = ParamReference.c_str();
					
					std::string::size_type icolon = CgFxParamName.find( ":" );
					if( icolon != std::string::npos )
					{
						int ilen = CgFxParamName.length();
						CgFxParamName = CgFxParamName.substr( icolon+1, (ilen-icolon)-1 );
					}

					///////////////////////////////////////////////

					printf( "SAMPLERPARAM<%s>\n", CgFxParamName.c_str() );
					///////////////////////////////////////////////

					FCDEffectParameterSampler *ParamSampler = (FCDEffectParameterSampler*) Param;
					FCDEffectParameterSampler::SamplerType SamplerType = ParamSampler->GetSamplerType();
					FCDEffectParameterSurface * Surface = ParamSampler->GetSurface();
					
					FCDImage *pimage = Surface->GetImage();
					const fm::string& ImageEntName = pimage->GetName();
					const fm::string& SurfaceRefName = Surface->GetReference();
					const fstring & ImageName = pimage->GetFilename();
					const char * ImageFileName = ImageName.c_str();

					ork::lev2::GfxMaterialFxParamArtist<lev2::Texture*> *paramf = new ork::lev2::GfxMaterialFxParamArtist<lev2::Texture*>;
					param=paramf;
					param->GetRecord().meParameterType = EPROPTYPE_SAMPLER;
					paramf->SetInitString( std::string(ImageFileName) );
					param->AddAnnotation( "orig_filename", ImageFileName );
					param->AddAnnotation( "image_ent_name", ImageEntName.c_str() );
					param->AddAnnotation( "surface_ref_name", SurfaceRefName.c_str() );
					switch( SamplerType )
					{
						case FCDEffectParameterSampler::SAMPLER1D:
							param->AddAnnotation( "sampler_type", "1d" );
							break;	
						case FCDEffectParameterSampler::SAMPLER2D:
							param->AddAnnotation( "sampler_type", "2d" );
							break;	
						case FCDEffectParameterSampler::SAMPLER3D:
							param->AddAnnotation( "sampler_type", "3d" );
							break;	
						case FCDEffectParameterSampler::SAMPLERCUBE:
							param->AddAnnotation( "sampler_type", "cube" );
							break;	
					}

					break;
				}

			}

			if( param )
			{
				param->GetRecord().mParameterName = parameter_name;
				FxMaterial->AddParameter( param );
			}
		}
		FxMaterial->Init(0);
	}

}

///////////////////////////////////////////////////////////////////////////////

void SColladaMaterial::ParseMaterial( FCDocument* doc, const std::string & ShadingGroupName, const std::string& MaterialName )
{

	printf( "ParseMaterial\n");

	FCDMaterialLibrary *MatLib = doc->GetMaterialLibrary();
	FCDMaterial* material = MatLib->FindDaeId(MaterialName.c_str());

	//const fstring& material_note = material->GetNote();
	//std::string smaterial_note = material_note.c_str();
	//printf( "smaterial_note<%s>\n", smaterial_note.c_str() );

	/*u32 itm = smaterial_note.find( "materialobject(" );
	std::string material_object;
	std::string material_namespace;
	if( itm != std::string::npos )
	{
		itm = smaterial_note.find( "(" )+1;
		u32 itm2 = smaterial_note.find( ")" );
		
		if( itm2 != std::string::npos )
		{
			material_object = smaterial_note.substr( itm, (itm2-itm) );

			itm = material_object.find( ":" );
			if( itm != std::string::npos )
			{
				material_namespace = material_object.substr( 0, itm );
			}


		}
		
	}*/

	mShadingGroupName = ShadingGroupName;
	mMaterialName = MaterialName;
	mSpecularPower = 1.0f;

	printf( "mShadingGroupName<%s> MaterialName<%s> material<%p>\n", mShadingGroupName.c_str(), MaterialName.c_str(), material );

	if(material)
	{
		FCDEffect* Effect = material->GetEffect();

		if( Effect )
		{
			mFx = Effect;
			FCDEffectStandard *StdProf = static_cast<FCDEffectStandard*>( Effect->FindProfile( FUDaeProfileType::COMMON ) );
			FCDEffectProfileFX *FxProf = static_cast<FCDEffectProfileFX*>( Effect->FindProfile( FUDaeProfileType::CG ) );
            auto extra = Effect->GetExtra();
            printf( "extra<%p>\n", extra );

            bool do_fxmtl = false;

            if( FxProf )
            {
                do_fxmtl = true;
            }
			else if( extra )
            {
                auto exx = extra->FindType("import");
                printf( "exx<%p>\n", exx );
                if( exx )
                {
                    auto tek = exx->GetTechnique(0);
                    printf( "tek<%p>\n", tek );
                    if( tek )
                    {
                        auto profile = tek->GetProfile();
                        printf( "profile<%s>\n", profile );
                        if( 0==strcmp(profile,"NVIDIA_FXCOMPOSER") )
                        {
                            do_fxmtl = true;
                        }
                    }
                }
            }


            if( do_fxmtl )
            {
                ParseFxMaterial( material );
                mFxProfile = (FCDMaterial*) material->Clone(0);
            }
			else if( StdProf )
			{
				ParseStdMaterial( StdProf );
				mStdProfile = StdProf;
			}
		}

		if( mpOrkMaterial )
		{
			mpOrkMaterial->SetName( AddPooledString( MaterialName.c_str() ) );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void SColladaMatGroup::Parse( const SColladaMaterial& colmat )
{
	ork::lev2::GfxMaterial* pmat = colmat.mpOrkMaterial;

	ork::lev2::GfxMaterialFx* pmatfx = rtti::autocast( pmat );

	if( pmatfx )
	{
		meMaterialClass = EMATCLASS_FX;
		mVertexConfigData = pmatfx->RefVertexConfig();
		mpOrkMaterial = colmat.mpOrkMaterial;
	}
	else
	{
		meMaterialClass = EMATCLASS_STANDARD;
	}
}

///////////////////////////////////////////////////////////////////////////////

SColladaMaterial::SColladaMaterial()
	: mpOrkMaterial( 0 )
	, mStdProfile( 0 )
{

}

///////////////////////////////////////////////////////////////////////////////

bool NvttCompress( const ork::tool::FilterOptMap& options );

bool CColladaModel::ConvertTextures(const file::Path& outmdlpth, ork::tool::FilterOptMap& options )
{
	bool rv = true;

	ork::file::Path InDir( mFileName.c_str() );
	InDir.SetExtension(0);
	InDir.SetFile(0);

	ork::file::Path OutDir = outmdlpth;
	OutDir.SetExtension(0);
	OutDir.SetFile(0);

	for( auto passet : mTextures )
	{
		const ork::AssetPath& path = passet->GetName();

		lev2::Texture* ptex = (passet==nullptr) ? nullptr : passet->GetTexture();

		ork::file::Path InPath = path;

		std::string tmpstr(InPath.c_str());

		size_t f = tmpstr.find("data/src/");
		if(f == std::string::npos)
		{
			orkerrorlog("ERROR: Input texture path is outside of 'data/src/'! (%s)\n", InPath.c_str());
			return false;
		}
		auto base_out_dir = std::string("data/") + options.GetOption("-platform")->GetValue();

		file::Path OutPath = outmdlpth;
		OutPath.SetFile( path.GetName().c_str() );
		OutPath.SetExtension( "dds" );
		options.GetOption( "-in" )->SetValue( InPath.c_str() );
		options.GetOption( "-out" )->SetValue( OutPath.c_str() );

		ork::file::Path::SmallNameType extension = InPath.GetExtension();

		if(strcmp(extension.c_str(), "dds") == 0)
		{
			printf( "OutPath<%s>\n", OutPath.c_str() );
			fxstring<1024> cmd_str;
			cmd_str.format( "cp %s %s", InPath.c_str(),  OutPath.c_str() );
			system( cmd_str.c_str() );
		}
		else
		{	// convert via NVTT ?
			/*if(ColladaExportPolicy::GetContext() && ColladaExportPolicy::GetContext()->mDDSInputOnly)
			{
				orkerrorlog("ERROR: <%s> Only DDS files should be referenced from DAE (and hence Maya) models! (%s)\n", InPath.c_str(), mFileName.c_str());

				return false;
			}
			OrkAssert(false);*/
			rv &= NvttCompress( options );
		}
	}

	return rv;
}

} }
#endif
