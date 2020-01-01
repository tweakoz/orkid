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

#include <FCDocument/FCDTexture.h>

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

#include <cstring>

typedef ork::tool::ColladaFxAnimChannel<float> ColladaFxAnimChannelFloat;
typedef ork::tool::ColladaFxAnimChannel<ork::fvec3> ColladaFxAnimChannelFloat3;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ColladaAnimChannel,"ColladaAnimChannel");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ColladaUvAnimChannel,"ColladaUvAnimChannel");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ColladaMatrixAnimChannel,"ColladaMatrixAnimChannel");

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ColladaFxAnimChannelFloat,"ColladaFxAnimChannelFloat");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ColladaFxAnimChannelFloat3,"ColladaFxAnimChannelFloat3");

///////////////////////////////////////////////////////////////////////////////

using namespace ork::lev2;
namespace ork { namespace tool {

///////////////////////////////////////////////////////////////////////////////

Place2dData::Place2dData()
	: rotateUV(0.0f)
	, repeatU(0.0f)
	, repeatV(0.0f)
	, offsetU(0.0f)
	, offsetV(0.0f)
{
}

///////////////////////////////////////////////////////////////////////////////

void ColladaAnimChannel::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

void ColladaUvAnimChannel::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

void ColladaMatrixAnimChannel::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

template<> void ColladaFxAnimChannel<float>::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

template<>void ColladaFxAnimChannel<ork::fvec3>::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

void ColladaUvAnimChannel::SetData( int iframe, const std::string& itemname, float fval )
{
	if( iframe >= miSettingFrame )
	{
		OrkAssert( miSettingFrame == iframe ); // only allow adding 1 at a time 4 now
		mSampledFrames.push_back( Place2dData() );
		miSettingFrame = int(mSampledFrames.size());
	}

	Place2dData& pdata = mSampledFrames[ iframe ];

	if( itemname == "rotateUV" )
	{
		pdata.rotateUV = fval;
	}
	else if( itemname == "repeatU" )
	{
		pdata.repeatU = fval;
	}
	else if( itemname == "repeatV" )
	{
		pdata.repeatV = fval;
	}
	else if( itemname == "offsetU" )
	{
		pdata.offsetU = fval;
	}
	else if( itemname == "offsetV" )
	{
		pdata.offsetV = fval;
	}
	else
	{
	//	OrkAssert(false);
	}

}

///////////////////////////////////////////////////////////////////////////////

void ColladaMatrixAnimChannel::SetParam( int iframe, int irow, int icol, float fval )
{
	if( iframe >= miSettingFrame )
	{
		OrkAssert( miSettingFrame == iframe ); // only allow adding 1 at a time 4 now
		mSampledFrames.push_back( fmtx4::Identity );

		miSettingFrame = mSampledFrames.size();
	}

	fmtx4 & Mtx = mSampledFrames[ iframe ];
	Mtx.SetElemYX(irow,icol,fval);
}

///////////////////////////////////////////////////////////////////////////////

bool CColladaAnim::GetPose( void )
{
	const FCDVisualSceneNodeLibrary *VizLib = mDocument->GetVisualSceneLibrary();

	int inument( VizLib->GetEntityCount() );

	if( 1 == inument )
	{
		const FCDSceneNode *prootnode = VizLib->GetEntity(0);
		fmtx4 RootMtx = FCDMatrixTofmtx4( prootnode->ToMatrix() );

		orkstack<const FCDSceneNode *> NodeStack;
		orkstack<fmtx4> MtxStack;

		NodeStack.push( prootnode );
		MtxStack.push( RootMtx );

		while( false == NodeStack.empty() )
		{
			const FCDSceneNode *pnode = NodeStack.top();
			bool bparentjoint = pnode->GetJointFlag();

			const fmtx4 ParentMtx = MtxStack.top();
			NodeStack.pop();
			MtxStack.pop();


			int inumchildren = int(pnode->GetChildrenCount());
			for( int i=0; i<inumchildren; i++ )
			{	const FCDSceneNode *pchild = pnode->GetChild(i);
				bool bchildjoint = pchild->GetJointFlag();
				std::string ChildName = pchild->GetName().c_str();

				fmtx4 ChildMtx = FCDMatrixTofmtx4( pchild->ToMatrix() );

				NodeStack.push(pchild);
				MtxStack.push(ChildMtx);
			}

			std::string NodeName = pnode->GetName().c_str();

			if( bparentjoint )
			{
				mPose[ NodeName ] = ParentMtx;
			}
			else
			{
				//orkprintf( "discarding parent<%s>\n", NodeName.c_str() );
			}
		}

	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void CColladaAnim::ParseTextures()
{
	// TODO: Skip textures/materials on shapes in the ref layer
	for( orkmap<std::string,ColladaMaterial>::iterator it=mMaterialMap.begin(); it!=mMaterialMap.end(); it++ )
	{
		ColladaMaterial& mat = it->second;

		if( mat.mStdProfile )
		{
			int TexCount( mat.mStdProfile->GetTextureCount(FUDaeTextureChannel::DIFFUSE) );
			if(TexCount < 1)
				orkerrorlog("ERROR: Collada Parse Textures: Diffuse texture count is zero for %s (%s)\n", mat.mMaterialName.c_str(), mat.mShadingGroupName.c_str());
			else
			{
				const FCDTexture* ptex = mat.mStdProfile->GetTexture(FUDaeTextureChannel::DIFFUSE,0);
				if( ptex )
				{
					const FCDExtra *TexExtra = ptex->GetExtra();

					const FCDEffectParameterSampler* Sampler = ptex->GetSampler();
					const fm::string & refname = Sampler->GetReference();
					std::string srefname = refname.c_str();

					const char * prefname = srefname.c_str();

					////////////////////////////////////////////////////////////////////
					// check for uv animation
					////////////////////////////////////////////////////////////////////

					const FCDETechnique* TexMayaTek = TexExtra->GetDefaultType()->FindTechnique("MAYA");
					if( TexMayaTek != nullptr ){

							const FCDENode *Place2dNode = TexMayaTek->FindChildNode( "MayaNodeName" );

							const fchar *Place2dNodeName = Place2dNode ? Place2dNode->GetContent() : "";

							std::string Place2dNodeNameStr( Place2dNodeName );

							////////////////////////////////////////////////////////////
							// strip ref name

							const fchar* colon = std::strstr(Place2dNodeName, ":" );

							if( colon )
							{
								Place2dNodeNameStr = colon+1;
							}

							////////////////////////////////////////////////////////////

							mat.mDiffuseMapChannel.mPlacementNodeName = Place2dNodeNameStr;

							ColladaUvAnimChannel* pchannel = new ColladaUvAnimChannel( Place2dNodeNameStr );

							mUvAnimatables.insert( std::make_pair(Place2dNodeNameStr,pchannel) );

							pchannel->SetMaterialName( mat.mMaterialName.c_str() );
						}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void CColladaAnim::ParseMaterials()
{
	bool bmat_bindings = ParseColladaMaterialBindings( *mDocument, mShadingGroupMap );

	if( bmat_bindings )
	{
		for( const auto& item : mShadingGroupMap )
		{
			const std::string& ShadingGroupName = item.first;
			const std::string& MaterialName = item.second.mMaterialDaeId;

			ColladaMaterial colladamaterial;
			colladamaterial.ParseMaterial( mDocument, ShadingGroupName, MaterialName );

			mMaterialMap[ MaterialName ] = colladamaterial;

			lev2::GfxMaterialFx* pmatfx = rtti::autocast( colladamaterial._orkMaterial );

			if( pmatfx )
			{
				const GfxMaterialFxEffectInstance& fxi = pmatfx->GetEffectInstance();

				for( orklut<std::string,GfxMaterialFxParamBase*>::const_iterator it2=fxi.mParameterInstances.begin(); it2!=fxi.mParameterInstances.end(); it2++ )
				{
					const std::string& paramname = it2->first;

					GfxMaterialFxParamBase* param = it2->second;

					GfxMaterialFxParamArtist<float>* paramfloat = rtti::autocast( param );
					if( paramfloat )
					{
						std::string FullParamName = MaterialName + std::string("_") + paramname;
						mFxAnimatables.insert( std::make_pair(FullParamName,param) );
						continue;
					}

					GfxMaterialFxParamArtist<fvec3>* paramvect3 = rtti::autocast( param );
					if( paramvect3 )
					{
						std::string FullParamName = MaterialName + std::string("_") + paramname;
						mFxAnimatables.insert( std::make_pair(FullParamName,param) );
						continue;
					}

				}
			}
		}
	}

	ParseTextures();
}

///////////////////////////////////////////////////////////////////////////////

void ParseJoints( FCDocument* document, orkset<std::string>& jointset )
{
	const FCDControllerLibrary * ConLib = document->GetControllerLibrary();

	if( 0 == ConLib ) return;

	orkvector<const FCDSkinController*>	SkinControllers;

	size_t inument =  ConLib->GetEntityCount ();

	for( size_t ient=0; ient<inument; ient++ )
	{
		const FCDController* ConObj = ConLib->GetEntity(ient);
		if(ConObj->IsSkin())
		{
			const FCDSkinController *skinController = ConObj->GetSkinController();
			SkinControllers.push_back( skinController );
		}
	}

	////////////////////////////////////////////////////////////////////

	const FCDVisualSceneNodeLibrary *VizLib = document->GetVisualSceneLibrary();

	int inumvizent( VizLib->GetEntityCount() );

	orkmap<std::string,std::string> NodeSubIdMap;

	if( 1 == inumvizent )
	{
		const FCDSceneNode *prootnode = VizLib->GetEntity(0);

		orkstack<const FCDSceneNode *> NodeStack;

		NodeStack.push( prootnode );

		while( false == NodeStack.empty() )
		{
			const FCDSceneNode *pnode = NodeStack.top();
			NodeStack.pop();

			bool bjoint = pnode->GetJointFlag();

			if( bjoint )
			{
				std::string NodeName = pnode->GetName().c_str();
				std::string NodeSid = pnode->GetSubId().c_str();
				jointset.insert(  NodeName );
			}

			int inumchildren = int(pnode->GetChildrenCount());

			for( int i=0; i<inumchildren; i++ )
			{
				const FCDSceneNode *pchild = pnode->GetChild(i);
				FCDEntity::Type typ = pchild->GetType();
				NodeStack.push(pchild);
			}
		}
	}

	////////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

bool CColladaAnim::Parse( void )
{
	orkset<std::string> jointset;

	ParseJoints( mDocument, jointset );

	for( orkset<std::string>::const_iterator it=jointset.begin(); it!=jointset.end(); it++ )
	{
		const std::string& str = (*it);

		orkprintf( "joint<%s>\n", str.c_str() );
	}

	///////////////////////////////////

	ParseMaterials(); // check materials

	///////////////////////////////////

	bool brval = true;

	const float kfsampleincrement = 1.0f / mfSampleRate;

	///////////////////////////////////
	// sample the animation curves at a given framerate

	FCDAnimationLibrary *AnimLib = mDocument->GetAnimationLibrary();

	int inument( AnimLib->GetEntityCount() );

	printf( "CColladaAnim::Parse() inument<%d>\n", inument );

	std::map<std::string,FCDAnimationChannel*> ValidMatrixAnimSet;
	std::map<std::string,FCDAnimationChannel*> ValidUvAnimSet;
	std::map<std::string,FCDAnimationChannel*> ValidFxAnimSet;

	for( int ie=0; ie<inument; ie++ )
	{
		FCDAnimation* Anim = AnimLib->GetEntity(ie);

		std::string AnimName = Anim->GetName().c_str();
		FixedString<256> fx2 = AnimName.c_str();
		fx2.replace_in_place("Armature_","");
		fx2.replace_in_place("_pose","");
		AnimName = fx2.c_str();

		size_t num_chans = Anim->GetChannelCount();
		size_t num_child = Anim->GetChildrenCount();

		printf( "AnimName<%s> num_chans<%d> num_child<%d>\n", AnimName.c_str(), int(num_chans), int(num_child) );

		auto it_duscore = AnimName.find( "__" );

		if( it_duscore != std::string::npos )
		{
			size_t len = AnimName.length();
			AnimName = AnimName.substr(it_duscore+2,len-(it_duscore+2));
		}

		auto itstrmtx = AnimName.find( "_matrix" );

		if( itstrmtx != std::string::npos )
		{
			AnimName = AnimName.substr(0,itstrmtx);
			for( int icha=0; icha<num_chans; icha++ )
			{
				auto chan = Anim->GetChannel(icha);
				size_t numcrv = chan->GetCurveCount();// const { return curves.size(); }

				printf( "  AnimName<%s> Chan<%d><%p> numcurves<%zu>\n", AnimName.c_str(), icha, chan, numcrv );
				if( numcrv==16 )
					ValidMatrixAnimSet[AnimName]=chan;
			}
			//ValidMatrixAnimSet.insert(Anim);
		}
		else for( int ich=0; ich<num_child; ich++ )
		{
			auto chld = Anim->GetChild(ich);
			const std::string ChildName = chld->GetName().c_str();
			size_t num_cchans = chld->GetChannelCount();
			printf( "  Child<%d> Name<%s> numchans<%d>\n", ich, ChildName.c_str(), num_cchans  );
			for( int icha=0; icha<num_cchans; icha++ )
			{
				auto chan = chld->GetChannel(icha);
				size_t numcrv = chan->GetCurveCount();// const { return curves.size(); }

				printf( "  Chan<%d><%p> numcurves<%d>\n", icha, chan, numcrv );
				if( numcrv==16 )
					ValidMatrixAnimSet[AnimName]=chan;
			}
		}

		////////////////////////////////////////////////
		// split AnimName into base and component
		////////////////////////////////////////////////

		const size_t it_uscore = AnimName.find( "_" );
		const size_t it_euscore = AnimName.rfind( "_" );
		const size_t it_length = AnimName.length();
		const size_t it_npos = std::string::npos;

		const std::string AnimBaseName = (it_uscore!=it_npos) ? AnimName.substr( 0, it_uscore ) : AnimName;
		const std::string AnimCompName = (it_uscore!=it_npos) ? AnimName.substr( it_uscore+1, it_length-(it_uscore+1) ) : "";


		const std::string AnimRBaseName = (it_euscore!=it_npos) ? AnimName.substr( 0, it_euscore ) : AnimName;
		const std::string AnimRCompName = (it_euscore!=it_npos) ? AnimName.substr( it_euscore+1, it_length-(it_euscore+1) ) : "";

		////////////////////////////////////////////////
		// uv anim channel ?
		////////////////////////////////////////////////
		/*
		orkmap<std::string,ColladaUvAnimChannel*>::iterator it_uv = mUvAnimatables.find(AnimRBaseName);
		orkmap<std::string,GfxMaterialFxParamBase*>::const_iterator it_ma = mFxAnimatables.find(AnimName);

		if( it_uv != mUvAnimatables.end() ) // channel name found in FxAnimatables ?
		{
			ColladaUvAnimChannel* pchan = it_uv->second;

			mAnimationChannels[ AnimRBaseName ] = pchan;

			int inumchannels = Anim->GetChannelCount();
			OrkAssert( inumchannels==1 );
			FCDAnimationChannel* InputChannel = Anim->GetChannel(0);
			int inumcurves = InputChannel->GetCurveCount();
			OrkAssert( inumcurves==1 );
			FCDAnimationCurve *InputCurve = InputChannel->GetCurve(0);
			int inumkeys = InputCurve->GetKeyCount();
			float ffirstkey = InputCurve->GetKey(0)->input;
			float flastkey = InputCurve->GetKey( inumkeys-1 )->input;
			int iframe = 0;
			for( float fi=ffirstkey; fi<=flastkey; fi+=kfsampleincrement, iframe++ )
			{
				float KeyFrame = InputCurve->Evaluate( fi );
				pchan->SetData( iframe, AnimRCompName, KeyFrame );
			}

		}

		////////////////////////////////////////////////
		// material anim channel ?
		////////////////////////////////////////////////

		else if( it_ma != mFxAnimatables.end() ) // channel name found in FxAnimatables ?
		{
			const std::string& name = it_ma->first;
			const GfxMaterialFxParamBase* param = it_ma->second;

			int inumchannels = Anim->GetChannelCount();

			switch( param->GetRecord().meParameterType )
			{
				case ork::EPROPTYPE_REAL:
				{
					OrkAssert( inumchannels==1 );
					FCDAnimationChannel* InputChannel = Anim->GetChannel(0);
					int inumcurves = InputChannel->GetCurveCount();
					OrkAssert( inumcurves==1 );
					FCDAnimationCurve *InputCurve = InputChannel->GetCurve(0);
					ColladaFxAnimChannel<float>* OutputAnimChannel = new ColladaFxAnimChannel<float>( AnimName, AnimBaseName, AnimCompName );
					mAnimationChannels[ AnimName ] = OutputAnimChannel;
					int inumkeys = InputCurve->GetKeyCount();
					float ffirstkey = InputCurve->GetKey(0)->input;
					float flastkey = InputCurve->GetKey( inumkeys-1 )->input;
					int iframe = 0;
					for( float fi=ffirstkey; fi<=flastkey; fi+=kfsampleincrement, iframe++ )
					{
						float KeyFrame = InputCurve->Evaluate( fi );
						OutputAnimChannel->AddFrame( KeyFrame );
					}
					break;
				}
				case ork::EPROPTYPE_VEC3FLOAT:
				{
					OrkAssert( inumchannels==1 );
					FCDAnimationChannel* InputChannel = Anim->GetChannel(0);
					int inumcurves = InputChannel->GetCurveCount();
					OrkAssert( inumcurves==3 );
					ColladaFxAnimChannel<fvec3>* OutputAnimChannel = new ColladaFxAnimChannel<fvec3>( AnimName, AnimBaseName, AnimCompName );
					mAnimationChannels[ AnimName ] = OutputAnimChannel;
					FCDAnimationCurve *Curve0 = InputChannel->GetCurve(0);
					int inumkeys = Curve0->GetKeyCount();
					float ffirstkey = Curve0->GetKey(0)->input;
					float flastkey = Curve0->GetKey( inumkeys-1 )->input;
					int iframe = 0;
					FCDAnimationCurve *CurveX = InputChannel->GetCurve(0);
					FCDAnimationCurve *CurveY = InputChannel->GetCurve(1);
					FCDAnimationCurve *CurveZ = InputChannel->GetCurve(2);
					for( float fi=ffirstkey; fi<=flastkey; fi+=kfsampleincrement, iframe++ )
					{
						float KeyFrameX = CurveX->Evaluate( fi );
						float KeyFrameY = CurveY->Evaluate( fi );
						float KeyFrameZ = CurveZ->Evaluate( fi );
						OutputAnimChannel->AddFrame( fvec3(KeyFrameX,KeyFrameY,KeyFrameZ) );
					}
					break;
				}
			}

		}
		*/
	}

	////////////////////////////////////////////////
	// joint or matrix anim channel ?
	////////////////////////////////////////////////

	printf( "ValidMatrixAnimSet size<%d>\n", int(ValidMatrixAnimSet.size()));

	//////////////////////////
	// find domain
	//////////////////////////

	float fevallo = 9999999.0f;
	float fevalhi = 0.0f;

	for( auto& ITEM : ValidMatrixAnimSet )
	{
		const std::string& MtxName = ITEM.first;
		FCDAnimationChannel* Channel = ITEM.second;
		FCDAnimation* Anim = Channel->GetParent();
		int inumcurves = Channel->GetCurveCount();
		assert( 16 == inumcurves );
		for( int icu=0; icu<inumcurves; icu++ )
		{
			FCDAnimationCurve *Curve = Channel->GetCurve(icu);
			int inumkeys = Curve->GetKeyCount();
			float ffirstkey = Curve->GetKey(0)->input;
			float flastkey = Curve->GetKey( inumkeys-1 )->input;

			if( ffirstkey<fevallo )
				fevallo = ffirstkey;
			if( flastkey>fevalhi )
				fevalhi = flastkey;

			printf( "chan<%s> lo<%f> hi<%f>\n", MtxName.c_str(), ffirstkey, flastkey );

		}
	}

	if( fevallo<0.0f )
		fevallo = 0.0f;

	printf( "fevallo<%f> fevalhi<%f>\n", fevallo, fevalhi );

	//////////////////////////
	// sample
	//////////////////////////

	for( auto& ITEM : ValidMatrixAnimSet )
	{
		const std::string& MtxName = ITEM.first;
		FCDAnimationChannel* Channel = ITEM.second;

		FCDAnimation* Anim = Channel->GetParent();

		ColladaMatrixAnimChannel *OrkMtxAnimChannel = new ColladaMatrixAnimChannel( MtxName );

		int inumcurves = Channel->GetCurveCount();
		assert( 16 == inumcurves );

		mAnimationChannels[ MtxName ] = OrkMtxAnimChannel;
		for( int icu=0; icu<inumcurves; icu++ )
		{
			FCDAnimationCurve *Curve = Channel->GetCurve(icu);
			int inumkeys = Curve->GetKeyCount();
			float ffirstkey = Curve->GetKey(0)->input;
			float flastkey = Curve->GetKey( inumkeys-1 )->input;
			float KeyFrame = 0.0f;
			int iframe = 0;
			for( float fi=fevallo; fi<=fevalhi; fi+=kfsampleincrement )
			{
				KeyFrame = Curve->Evaluate( fi );
				int irow = (icu/4);
				int icol = (icu%4);
				OrkMtxAnimChannel->SetParam( iframe, irow, icol, KeyFrame );
				iframe++;
			}
		}


	}

	///////////////////////////////////
	// make sure everone has the same number of frames!

	printf( "NUMANIMCHAN<%d>\n", int(mAnimationChannels.size()));

	if( mAnimationChannels.size() )
	{
		const int knumframesref = mAnimationChannels.begin()->second->GetNumFrames();

		for( orkmap<std::string,ColladaAnimChannel*>::const_iterator it=mAnimationChannels.begin(); it!=mAnimationChannels.end(); it++ )
		{
			ColladaAnimChannel * Channel = (*it).second;
			int inumframes = Channel->GetNumFrames();

			printf( "CHANNEL<%s> iframe<%d>\n", Channel->GetName().c_str(), inumframes );

			if( knumframesref != inumframes )
			{
				orkerrorlog( "ERROR: ColladaAnim[%s] not all anim channels have the same number of frames!\n", this->mFileName.c_str() );
				return false;
			}
		}
		miNumFrames = knumframesref;
	}

	///////////////////////////////////

	return brval;
}

///////////////////////////////////////////////////////////////////////////////

}}
#endif // USE_FCOLLADA
