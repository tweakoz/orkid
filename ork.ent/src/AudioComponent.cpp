////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/AudioComponent.h>
#include <ork/lev2/aud/audiobank.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>
#include <ork/gfx/camera.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/core_interface.h>
#include <pkg/ent/event/StartAudioEffectEvent.h>
#include <pkg/ent/event/StopSoundEvent.h>
#include <ork/reflect/enum_serializer.h>
//#include <common/archetypes/player.h>
#include <pkg/ent/ReferenceArchetype.h>
#include <ork/kernel/Array.h>
#include <ork/kernel/Array.hpp>
#include <ork/kernel/csystem.h>
#include <pkg/ent/dataflow.h>
#include <ork/application/application.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.hpp>

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioEffectComponentData, "AudioEffectComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioEffectComponentInst, "AudioEffectComponentInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioEffectPlayDataBase, "AudioEffectPlayDataBase");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioMultiEffectPlayData, "AudioMultiEffectPlayData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioMultiEffectPlayDataItemBase, "AudioMultiEffectPlayDataItemBase");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioMultiEffectPlayDataItemFixed, "AudioMultiEffectPlayDataItemFixed");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioMultiEffectPlayDataItemModular, "AudioMultiEffectPlayDataItemModular");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioMultiEffectPlayInst, "AudioMultiEffectPlayInst");

///////////////////////////////////////////////////////////////////////////////
BEGIN_ENUM_SERIALIZER( ork::ent, EMultiSoundSelectMode )
	DECLARE_ENUM(EMS_RANDOM)
	DECLARE_ENUM(EMS_CYCLE)
END_ENUM_SERIALIZER()
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {

static bool gbusetemplate = false;

bool AudioMultiEffectPlayDataItemModular::DoNotify(const ork::event::Event *event)
{	if( const ork::ObjectGedVisitEvent* pev = ork::rtti::autocast( event ) )
	{	gbusetemplate = true;
		return true;
	}
	return false;
}

typedef std::pair<const ork::lev2::AudioProgram*,float> PlayCounter;
static const int kmaxplaytrack = 16;
static ork::fixedvector<PlayCounter,kmaxplaytrack> gplaytracker(kmaxplaytrack);
static int giplaytrackidx = 0;

static bool CheckPlayTracker( const ork::lev2::AudioProgram* pdb )
{
	return true; //ork::PoolString progname = pdb->GetName();
#if 0
	if( 0 == strstr( progname.c_str(), "ickup" ) )
	{
		return true;
	}

	float fcurtime = ork::CSystem::GetRef().GetLoResTime();

	int icount = 0;

	for( int i=0; i<kmaxplaytrack; i++ )
	{
		const std::pair<const ork::lev2::AudioProgram*,float>& pr = gplaytracker[i];

		if( pr.first == pdb )
		{
			float ftime = pr.second;
			float ftd = ork::CFloat::Abs( fcurtime-ftime );

			if( ftd < (1.0f/30.0f) )
			{
				icount++;
			}
		}

	}

	bool bplay = (icount<1);

	if( bplay )
	{
		gplaytracker[giplaytrackidx] = std::make_pair(pdb,fcurtime);
	}
	else
	{
		//printf( "skipsound <%08x>\n", pdb );
	}

	giplaytrackidx = (giplaytrackidx+1)%kmaxplaytrack;

	return bplay;
#endif

}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AudioEffectPlayDataBase::Describe()
{
	ork::reflect::RegisterProperty( "MutexGroup", & AudioEffectPlayDataBase::mMutexGroup );
	ork::reflect::RegisterProperty( "MutexPriority", & AudioEffectPlayDataBase::miMutexPriority );
	ork::reflect::RegisterProperty( "SubMixGroup", & AudioEffectPlayDataBase::mSubMixGroup );
	ork::reflect::RegisterProperty( "MaxEmitterRange", & AudioEffectPlayDataBase::mMaxEmitterDist);
	ork::reflect::RegisterProperty( "IsEmitter", & AudioEffectPlayDataBase::mbEmitter);
	ork::reflect::RegisterProperty( "AttenCurve", & AudioEffectPlayDataBase::AttenCurveAccessor );

	ork::reflect::AnnotatePropertyForEditor< AudioEffectPlayDataBase >( "MutexPriority", "editor.range.min", "0" );
	ork::reflect::AnnotatePropertyForEditor< AudioEffectPlayDataBase >( "MutexPriority", "editor.range.max", "15" );

	ork::reflect::AnnotatePropertyForEditor< AudioEffectPlayDataBase >( "MutexGroup", "ged.userchoice.delegate", "AudioMutexGroupChoiceDelegate" );
	ork::reflect::AnnotatePropertyForEditor< AudioEffectPlayDataBase >( "SubMixGroup", "ged.userchoice.delegate", "AudioSubMixChoiceDelegate" );

	ork::reflect::AnnotatePropertyForEditor< AudioEffectPlayDataBase >("MaxEmitterRange", "editor.range.min", "0.0" );
	ork::reflect::AnnotatePropertyForEditor< AudioEffectPlayDataBase >("MaxEmitterRange", "editor.range.max", "100.0" );
	ork::reflect::AnnotatePropertyForEditor< AudioEffectPlayDataBase >("MaxEmitterRange", "editor.range.log", "true" );
}
///////////////////////////////////////////////////////////////////////////////
AudioEffectPlayDataBase::AudioEffectPlayDataBase()
	: mData( 0 )
	, miMutexPriority(8)
	, mMutexGroup(ork::AddPooledLiteral("none"))
	, mSubMixGroup(ork::AddPooledLiteral("none"))
	, mMaxEmitterDist( 2000.0f ) // 2000 cm
	, mbEmitter(false)
{
}
///////////////////////////////////////////////////////////////////////////////
void AudioMultiEffectPlayData::Describe()
{
	ork::reflect::RegisterMapProperty( "SubSounds", & AudioMultiEffectPlayData::mSubSoundMap );
	ork::reflect::AnnotatePropertyForEditor< AudioMultiEffectPlayData >("SubSounds", "editor.factorylistbase", "AudioMultiEffectPlayDataItemBase" );
	ork::reflect::RegisterProperty( "SelectMode", & AudioMultiEffectPlayData::mSelectMode );
	ork::reflect::AnnotatePropertyForEditor<AudioMultiEffectPlayData>(	"SelectMode", "editor.class", "ged.factory.enum" );
	ork::reflect::RegisterProperty( "Enable3D", & AudioMultiEffectPlayData::mEnable3D );
}
///////////////////////////////////////////////////////////////////////////////
AudioMultiEffectPlayData::AudioMultiEffectPlayData()
	: mSelectMode(EMS_RANDOM)
	, mSubSoundMap( ork::EKEYPOLICY_MULTILUT )
	, mEnable3D( true )
{
}
AudioMultiEffectPlayData::~AudioMultiEffectPlayData()
{
	for( ork::orklut< ork::PoolString, AudioMultiEffectPlayDataItemBase* >::const_iterator it = GetSubSoundMap().begin(); it!=GetSubSoundMap().end(); it++ )
	{
		const ork::PoolString& subname = it->first;
		AudioMultiEffectPlayDataItemBase* inst = it->second;
		delete inst;
	}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ork::lev2::AudioInstrumentPlayback* AudioMultiEffectPlayInst::Play( AudioEffectComponentInst* aeci, int inote, int ivel, const ork::TransformNode3D* pnode )
{
	ork::lev2::AudioIntrumentPlayParam param = GetParams();

	param.mEnable3D = mData.GetEnable3D();

	if( param.mEnable3D )
	{
		param.mXf3d = pnode ? pnode : & aeci->GetEntity()->GetDagNode().GetTransformNode();
	}
	////////////////////////////////////
	// check for note,velocity overrides
	////////////////////////////////////

	if( inote>=0 )
		param.miNote = inote;
	if( ivel>=0 )
		param.miVelocity = ivel;

	////////////////////////////////////

	ork::lev2::AudioInstrumentPlayback* sound_playback = DoPlay( aeci, param );
	//mPlaybacks.AddSorted( soundname, & sound_playback );
	return sound_playback;
}
void AudioMultiEffectPlayInst::Stop(AudioEffectComponentInst* aeci,ork::lev2::AudioInstrumentPlayback*pb)
{
	DoStop(aeci,pb);
}
///////////////////////////////////////////////////////////////////////////////
// multi play item
///////////////////////////////////////////////////////////////////////////////
void AudioMultiEffectPlayInst::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
AudioMultiEffectPlayInst::AudioMultiEffectPlayInst( const AudioEffectComponentInst& aeci, const AudioMultiEffectPlayData& pdata )
	: mAECI( aeci )
	, mSubSoundMap( ork::EKEYPOLICY_MULTILUT )
	, mData(pdata)
	, mLastPlayIndex(-1)
	, mRange(0)
{
	const ork::orklut< ork::PoolString, AudioMultiEffectPlayDataItemBase* > sndmap = pdata.GetSubSoundMap();

	int icount = sndmap.size();

	int inumv = 0;
	for( ork::orklut< ork::PoolString, AudioMultiEffectPlayDataItemBase* >::const_iterator it = sndmap.begin(); it!=sndmap.end(); it++ )
	{
		const ork::PoolString& subname = it->first;
		AudioMultiEffectPlayDataItemBase* data = it->second;

		if( data )
		{
			mSubSoundMap.AddSorted( subname, data->CreateInst() );
			inumv++;
		}
	}

	mSoundMapSize = inumv;
}
AudioMultiEffectPlayInst::~AudioMultiEffectPlayInst()
{
	for( ork::orklut< ork::PoolString, AudioMultiEffectPlayInstItemBase* >::const_iterator it = GetSubSoundMap().begin(); it!=GetSubSoundMap().end(); it++ )
	{
		const ork::PoolString& subname = it->first;
		AudioMultiEffectPlayInstItemBase* inst = it->second;
		delete inst;
	}
}
///////////////////////////////////////////////////////////////////////////////
ork::lev2::AudioInstrumentPlayback* AudioMultiEffectPlayInst::DoPlay( AudioEffectComponentInst* aeci, ork::lev2::AudioIntrumentPlayParam& param )
{	EMultiSoundSelectMode eselmod = mData.GetSelectMode();
	const ork::orklut< ork::PoolString, AudioMultiEffectPlayInstItemBase* > sndmap = GetSubSoundMap();
	ork::orklut< ork::PoolString, AudioMultiEffectPlayInstItemBase* >::const_iterator it = sndmap.end();
	switch( eselmod )
	{	case EMS_RANDOM:
		{	bool bok = false;
			while( false == bok )
			{	if( 0 == mRange )
				{	bok=true;
				}
				if( mSoundMapSize!=0 )
				{	int irand = std::rand()%mSoundMapSize;
					int ilo = 0;
					ork::orklut< ork::PoolString, AudioMultiEffectPlayInstItemBase* >::const_iterator itt = sndmap.GetIterAtIndex(irand);;
					{	//int ihi = ilo+itt->first;
						//if( irand>=ilo && irand<ihi )
						{	//if( irand != mLastPlayIndex )
							{	it = itt;
								mLastPlayIndex = irand;
								bok = true;
								break;
							}
						}
						//ilo = ihi;
					}
				}
			}
			break;
		}
		case EMS_CYCLE:
		{	if( mLastPlayIndex < 0 )
			{	mLastPlayIndex = 0;
			}
			if( mLastPlayIndex >= mSoundMapSize )
			{	mLastPlayIndex = 0;
			}
			it = sndmap.begin()+mLastPlayIndex;
			mLastPlayIndex++;
			break;
		}
	}
	if( it != sndmap.end() )
	{
		AudioMultiEffectPlayInstItemBase* pitem = it->second;
		const AudioMultiEffectPlayDataItemBase& itemdata = pitem->GetData();

		const ork::PoolString&	BankName = itemdata.GetBankName();
		const ork::PoolString&	ProgramName = itemdata.GetProgramName();

		param.miNote = itemdata.GetNote();
		param.miVelocity = itemdata.GetVelocity();

		const AudioEffectComponentData& aecd = mAECI.GetData();
		param.mMutexGroup = mData.GetMutexGroup().c_str();
		param.mSubMixGroup = mData.GetSubMixGroup().c_str();
		param.mEnable3D = mData.GetEnable3D();
		ork::orklut<ork::PoolString,ork::lev2::AudioBank*>::const_iterator itb = aecd.GetBankMap().find( BankName );
		ork::lev2::AudioBank* pbank = (itb==aecd.GetBankMap().end()) ? 0 : itb->second;
		if( pbank )
		{	ork::lev2::AudioProgram* Program = pbank->RefProgram( ProgramName );
			if( Program )
			{
				if( CheckPlayTracker( Program ) )
				{
					param.mProgram = Program;
					ork::lev2::AudioInstrumentPlayback* sound_playback = pitem->Play(aeci,param);
					if( sound_playback ) sound_playback->SetUserData0(anyp(pitem));
					return sound_playback;
				}
			}
		}
	}
	return 0;
}
void AudioMultiEffectPlayInst::DoStop(AudioEffectComponentInst* aeci,ork::lev2::AudioInstrumentPlayback*pb) 
{
	AudioMultiEffectPlayInstItemBase* pitem = pb->GetUserData0().Get<AudioMultiEffectPlayInstItemBase*>();

	if( pitem )
	{
		pitem->Stop(aeci,pb);
	}

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AudioMultiEffectPlayDataItemBase::Describe()
{
	ork::reflect::RegisterProperty( "BankName", & AudioMultiEffectPlayDataItemBase::mBankName );
	ork::reflect::RegisterProperty( "ProgramName", & AudioMultiEffectPlayDataItemBase::mProgramName );
	ork::reflect::RegisterProperty( "Note", & AudioMultiEffectPlayDataItemBase::miNote );
	ork::reflect::RegisterProperty( "Velocity", & AudioMultiEffectPlayDataItemBase::miVelocity );

	ork::reflect::AnnotatePropertyForEditor< AudioMultiEffectPlayDataItemBase >( "Velocity", "editor.range.min", "0" );
	ork::reflect::AnnotatePropertyForEditor< AudioMultiEffectPlayDataItemBase >( "Velocity", "editor.range.max", "255" );

	ork::reflect::AnnotatePropertyForEditor< AudioMultiEffectPlayDataItemBase >( "Note", "editor.range.min", "-1" );
	ork::reflect::AnnotatePropertyForEditor< AudioMultiEffectPlayDataItemBase >( "Note", "editor.range.max", "127" );

	ork::reflect::AnnotatePropertyForEditor< AudioMultiEffectPlayDataItemBase >( "Note", "ged.userchoice.delegate", "AudioNoteChoiceDelegate" );
	ork::reflect::AnnotatePropertyForEditor< AudioMultiEffectPlayDataItemBase >( "BankName", "ged.userchoice.delegate", "AudioBankChoiceDelegate" );
	ork::reflect::AnnotatePropertyForEditor< AudioMultiEffectPlayDataItemBase >( "ProgramName", "ged.userchoice.delegate", "AudioProgramChoiceDelegate" );
}
AudioMultiEffectPlayDataItemBase::AudioMultiEffectPlayDataItemBase()
	: mProgramName( ork::AddPooledLiteral("") )
	, miNote( 60 )
	, miVelocity( 127 )
	, mPan( 0.0f )
{
}
ork::lev2::AudioInstrumentPlayback* AudioMultiEffectPlayInstItemBase::Play(AudioEffectComponentInst* aeci, ork::lev2::AudioIntrumentPlayParam& param)
{
	return DoPlay(aeci,param);
}
void AudioMultiEffectPlayInstItemBase::Stop(AudioEffectComponentInst* aeci, ork::lev2::AudioInstrumentPlayback*pb )
{
	return DoStop(aeci,pb);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AudioMultiEffectPlayDataItemFixed::Describe()
{
}
AudioMultiEffectPlayDataItemFixed::AudioMultiEffectPlayDataItemFixed()
{
}
AudioMultiEffectPlayInstItemBase* AudioMultiEffectPlayDataItemFixed::CreateInst() const
{
	return new AudioMultiEffectPlayInstItemFixed( *this );
}
AudioMultiEffectPlayInstItemFixed::AudioMultiEffectPlayInstItemFixed(const AudioMultiEffectPlayDataItemFixed& itemdata)
	: AudioMultiEffectPlayInstItemBase(itemdata)
	, mItemData(itemdata)
{
}
ork::lev2::AudioInstrumentPlayback* AudioMultiEffectPlayInstItemFixed::DoPlay(AudioEffectComponentInst* aeci, ork::lev2::AudioIntrumentPlayParam& param)
{
	ork::lev2::AudioInstrumentPlayback* sound_playback = 
		ork::lev2::AudioDevice::GetDevice()->PlaySound( param.mProgram, param );
	return sound_playback;
}
void AudioMultiEffectPlayInstItemFixed::DoStop(AudioEffectComponentInst* aeci,ork::lev2::AudioInstrumentPlayback*pb)
{
	ork::lev2::AudioDevice::GetDevice()->StopSound( pb );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AudioMultiEffectPlayDataItemModular::Describe()
{
	ork::reflect::RegisterProperty( "ControlGraph", & AudioMultiEffectPlayDataItemModular::TemplateAccessor );
	ork::reflect::RegisterProperty( "NumVoices", & AudioMultiEffectPlayDataItemModular::miNumVoices );
	ork::reflect::AnnotatePropertyForEditor< AudioMultiEffectPlayDataItemModular >("NumVoices", "editor.range.min", "1" );
	ork::reflect::AnnotatePropertyForEditor< AudioMultiEffectPlayDataItemModular >("NumVoices", "editor.range.max", "16" );
}
AudioMultiEffectPlayDataItemModular::AudioMultiEffectPlayDataItemModular()
	: miNumVoices(1)
{
}
AudioMultiEffectPlayInstItemBase* AudioMultiEffectPlayDataItemModular::CreateInst() const
{
	return new AudioMultiEffectPlayInstItemModular( *this );
}
///////////////////////////////////////////////////////////////////////////////
AudioMultiEffectPlayInstItemModular::AudioMultiEffectPlayInstItemModular(const AudioMultiEffectPlayDataItemModular& itemdata)
	: AudioMultiEffectPlayInstItemBase(itemdata)
	, mItemData(itemdata)
{
	const ork::lev2::AudioGraph& template_graph = mItemData.GetTemplate();
	int inumvoices = mItemData.GetNumVoices();

	mGraphPool.BindTemplate( template_graph, inumvoices );

}
void AudioMultiEffectPlayInstItemModular::DoStop(AudioEffectComponentInst* aeci,ork::lev2::AudioInstrumentPlayback*pb)
{
	ork::lev2::AudioDevice::GetDevice()->StopSound( pb );

	ork::lev2::AudioGraph*	graph = pb->GetGraph();

	if( false == gbusetemplate )
	{
		if( graph )
		{
			mGraphPool.Free( graph );
		}
	}
}
ork::lev2::AudioInstrumentPlayback* AudioMultiEffectPlayInstItemModular::DoPlay(AudioEffectComponentInst* aeci, ork::lev2::AudioIntrumentPlayParam& param)
{
	ork::lev2::AudioGraph* graph = gbusetemplate 
									?	(ork::lev2::AudioGraph*) & mItemData.GetTemplate() 
									:	mGraphPool.Allocate();

	if( graph )
	{	
		ork::dataflow::dyn_external* dgmod = 0;

		ork::ent::DataflowRecieverComponentInst* dflowreciever = aeci->GetDflowReciver();

		if( dflowreciever )
		{
			dgmod = & dflowreciever->RefExternal();
		}

		graph->BindExternal(dgmod);

		graph->PrepForStart();

		ork::lev2::AudioInstrumentPlayback* sound_playback =
			ork::lev2::AudioDevice::GetDevice()->PlaySound( param.mProgram, param, graph );
		return sound_playback;
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class SoundMapPropertyType : public ork::reflect::DirectObjectMapPropertyType< ork::orklut<ork::PoolString, AudioEffectPlayDataBase*> >
{
	typedef ork::reflect::DirectObjectMapPropertyType< ork::orklut<ork::PoolString, AudioEffectPlayDataBase*> > SuperType;
	typedef ork::orklut<ork::PoolString, AudioEffectPlayDataBase*> MapType;

public:
	SoundMapPropertyType(MapType ork::Object::*obj) : SuperType(obj) {}

protected:

	bool WriteItem( ork::Object *pobj, const ork::PoolString& key, int imulti, AudioEffectPlayDataBase* const* ppaepd) const // virtual
	{
		ork::Object* poobj = static_cast<ork::Object*>( pobj );
		if( ppaepd )
		{
			AudioEffectPlayDataBase* aepd = (*ppaepd);
			AudioEffectComponentData* aecd = ork::rtti::autocast(poobj);
			OrkAssert(aecd);
			if( aepd ) aepd->SetAECD( aecd );
		}
		return SuperType::WriteItem( pobj, key, imulti, ppaepd );
	}
};
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AudioEffectComponentData::Describe()
{
	ork::ent::RegisterFamily<AudioEffectComponentData>(ork::AddPooledLiteral("audio"));

	ork::reflect::RegisterProperty("MutexGroups", & AudioEffectComponentData::mMutexGroups);
	ork::reflect::RegisterMapProperty("SoundMap", &AudioEffectComponentData::mSoundMap);
	ork::reflect::RegisterMapProperty("BankMap", & AudioEffectComponentData::mBankMap);


	ork::reflect::AnnotatePropertyForEditor<AudioEffectComponentData>("BankMap", "editor.assettype", "lev2::audiobank");
	ork::reflect::AnnotatePropertyForEditor<AudioEffectComponentData>("BankMap", "editor.assetclass", "lev2::audiobank");

	ork::reflect::AnnotatePropertyForEditor< AudioEffectComponentData >("SoundMap", "editor.factorylistbase", "AudioEffectPlayDataBase" );

}
void AudioEffectComponentData::DoRegisterWithScene( ork::ent::SceneComposer& sc )
{
	sc.Register<AudioManagerComponentData>();
}
///////////////////////////////////////////////////////////////////////////////
AudioEffectComponentData::AudioEffectComponentData()
{
}
AudioEffectComponentData::~AudioEffectComponentData()
{
	for( ork::orklut<ork::PoolString,AudioEffectPlayDataBase*>::const_iterator
			it=mSoundMap.begin();
			it!=mSoundMap.end(); 
			it++ )
	{
		delete it->second;
	}

}
///////////////////////////////////////////////////////////////////////////////
ork::ent::ComponentInst *AudioEffectComponentData::CreateComponent(ork::ent::Entity *pent) const
{
	return OrkNew AudioEffectComponentInst( *this, pent );
}

void AudioEffectComponentInst::Describe()
{
	//ork::reflect::RegisterFunctor("PlaySound", & AudioEffectComponentInst::PlaySound );
	//ork::reflect::RegisterFunctor("StopSound", &AudioEffectComponentInst::StopSound );
}
///////////////////////////////////////////////////////////////////////////////

void AudioEffectComponentInst::AddSound( const ork::PoolString& ps, AudioMultiEffectPlayInst* pib )
{
	mSoundMap.AddSorted( ps, pib );
}

///////////////////////////////////////////////////////////////////////////////

EmitterCtx::EmitterCtx()
	: mEmitter( 0 )
	, mbEmitterToggle( false )
	, mEmitterPB( 0 )
	, mAttenuationCurve(0)
{
}

AudioEffectComponentInst::AudioEffectComponentInst( const AudioEffectComponentData& aecd, ork::ent::Entity* pent )
	: ComponentInst( & aecd, pent )
	, mData( aecd )
	, mAmci( 0 )
	, mDflowRecv( 0 )
{
	mAmci = pent->GetSceneInst()->FindTypedSceneComponent<AudioManagerComponentInst>();
	///////////////////////////////////////////////////////////

	const ork::orklut<ork::PoolString,ork::lev2::AudioBank*>& bmap = mData.GetBankMap();
	const ork::orklut<ork::PoolString,AudioEffectPlayDataBase*>& smap = mData.GetSoundMap();
	for( ork::orklut<ork::PoolString,AudioEffectPlayDataBase*>::const_iterator it=smap.begin(); it!=smap.end(); it++ )
	{	const ork::PoolString& ps = it->first;
		const AudioMultiEffectPlayData* playmultidata = ork::rtti::autocast(it->second);
		if( playmultidata )
		{
			AudioMultiEffectPlayInst* pinstbase = new AudioMultiEffectPlayInst( *this, * playmultidata );
			AddSound( ps, pinstbase );
			if( playmultidata->IsEmitter() )
			{
				EmitterCtx newctx;
				newctx.mEmitter = pinstbase;
				newctx.mMaxDist = playmultidata->GetMaxEmitterDist();
				newctx.mAttenuationCurve = & playmultidata->GetAttenuationCurve();
				mEmitters.push_back( newctx );
			}
		}
		///////////////////////////////////////////////////////////////
	}
	///////////////////////////////////////////////////////////////

	ork::ent::SceneInst *inst = pent->GetSceneInst();
	mXform = & pent->GetDagNode().GetTransformNode();
}
///////////////////////////////////////////////////////////////////////////////
AudioEffectComponentInst::~AudioEffectComponentInst()
{
	StopAllSounds();
	for( ork::orklut<ork::PoolString,AudioMultiEffectPlayInst*>::const_iterator it=mSoundMap.begin(); it!=mSoundMap.end(); it++ )
	{
		AudioMultiEffectPlayInst* pinst = it->second;
		delete pinst;
	}
}
///////////////////////////////////////////////////////////////////////////////

void AudioEffectComponentInst::DoUpdate(ork::ent::SceneInst *inst)
{
	if( mPlaybacks.size() )
	{
		float fDT = inst->GetDeltaTime();

		const ork::ent::Entity * pent = GetEntity();

		ork::CVector3 emitter_trans = mXform->GetTransform()->GetPosition();

		const int kmaxpbs = 4;
		int inumpbsfreed = 0;
		ork::lev2::AudioInstrumentPlayback* remove_pbs[kmaxpbs];

		for( orkvector<ork::lev2::AudioInstrumentPlayback*>::const_iterator
				it = mPlaybacks.begin();
				it != mPlaybacks.end();
				it ++ )
		{
			ork::lev2::AudioInstrumentPlayback* playback = (*it);

			if( 0 == playback->GetNumActiveChannels() )
			{
				OrkAssert( inumpbsfreed<kmaxpbs );
				remove_pbs[inumpbsfreed] = playback;
				inumpbsfreed++;
			}
			else
			{
				/*const ork::lev2::AudioProgram* progr = playback->GetProgram();
				const ork::CMatrix4& mtx = mXform->GetMatrix();
				const ork::CVector3 pos = mtx.GetTranslation();
				if( 0 == strcmp( "SpecialAttack_Sponge", progr->GetName().c_str() ) )
				{
					//orkprintf( "pb<%s> pos<%f,%f,%f>\n", progr->GetName().c_str(), pos.GetX(), pos.GetY(), pos.GetZ() );
				}*/
				//playback->SetEmitterMatrix( & mtx );
				playback->Update(fDT);
			}
		}

		for( int i=0; i<inumpbsfreed; i++ )
		{
			ork::lev2::AudioInstrumentPlayback* pb = remove_pbs[i];
			RemovePlayback(pb);
		}

		////////////////////////////////////////////////
	}
}

///////////////////////////////////////////////////////////////////////////////

void AudioEffectComponentInst::UpdateEmitter( const ork::CCameraData* camdat1, const ork::CCameraData* camdat2 )
{
	int inumemitters = mEmitters.size();

	for( int ie=0; ie<inumemitters; ie++ )
	{
		EmitterCtx& emitterctx = mEmitters[ie];

		const ork::MultiCurve1D* atten_curve = emitterctx.mAttenuationCurve;

		const float kmaxemitterdistsq = emitterctx.mMaxDist * emitterctx.mMaxDist;

		ork::CVector3 emitter_trans = mXform->GetTransform()->GetPosition();
		ork::CVector3 eye1 = camdat1 ? camdat1->GetEye() : emitter_trans;
		ork::CVector3 eye2 = camdat2 ? camdat2->GetEye() : emitter_trans;

		float fdistsq1 = (eye1-emitter_trans).MagSquared();
		float fdistsq2 = (eye2-emitter_trans).MagSquared();

		float fdistsq = (fdistsq1<fdistsq2) ? fdistsq1 : fdistsq2;

		//////////////////////////////////////////////////
		// check for orphaned emitters
		//////////////////////////////////////////////////

		if( emitterctx.mbEmitterToggle )
		{
			if( emitterctx.mEmitterPB )
			{
				if( emitterctx.mEmitterPB->GetSerialNumber()!=emitterctx.miEmitterSN )
				{	// voices were cutoff, restart
					emitterctx.mbEmitterToggle=false;
					emitterctx.mEmitterPB = 0;
				}
			}
			else
			{
				emitterctx.mbEmitterToggle=false;
			}
		}
		//////////////////////////////////////////////////

		if( fdistsq < kmaxemitterdistsq )
		{
			if( false == emitterctx.mbEmitterToggle )
			{
				OrkAssert( 0 == emitterctx.mEmitterPB );

				emitterctx.mEmitter->GetParams().mAttenCurve = atten_curve;
				emitterctx.mEmitter->GetParams().mfMaxDistance = emitterctx.mMaxDist;
	
				emitterctx.mEmitterPB = emitterctx.mEmitter->Play( this, -1, -1, 0 );
				if( emitterctx.mEmitterPB )
				{
					emitterctx.miEmitterSN = emitterctx.mEmitterPB->GetSerialNumber();
					orkprintf( "start emitter<%08x>\n", emitterctx.mEmitterPB );
					AddPlayback( emitterctx.mEmitterPB );
				}
				emitterctx.mbEmitterToggle = true;
			}

			if( emitterctx.mEmitterPB )
			{
				float funitdist = ::sqrtf(fdistsq)/emitterctx.mMaxDist;
				funitdist = (funitdist<0.0f) ? 0.0f : funitdist;
				funitdist = (funitdist>1.0f) ? 1.0f : funitdist;
				float atten = atten_curve->Sample( funitdist );
				emitterctx.mEmitterPB->SetDistanceAtten( atten );
				emitterctx.mEmitterPB->SetEmitterMatrix( mXform );
			}
		}
		else //
		{
			if( emitterctx.mbEmitterToggle )
			{
				if( emitterctx.mEmitterPB )
				{
					orkprintf( "stop emitter<%08x>\n", emitterctx.mEmitterPB );
					RemovePlayback( emitterctx.mEmitterPB );
					emitterctx.mEmitter->Stop(this,emitterctx.mEmitterPB);
					emitterctx.mEmitterPB = 0;
				}
				emitterctx.mbEmitterToggle = false;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
AudioMultiEffectPlayInst* AudioEffectComponentInst::GetPlayInst( ork::PoolString pname ) const
{
	ork::orklut<ork::PoolString,AudioMultiEffectPlayInst*>::const_iterator it = mSoundMap.find( pname );
	return (it==mSoundMap.end()) ? 0 : it->second;
}
///////////////////////////////////////////////////////////////////////////////
ork::lev2::AudioInstrumentPlayback* AudioEffectComponentInst::PlaySoundEx( ork::PoolString soundname, int inote, int ivel, const ork::TransformNode3D* pnode )
{
	ork::lev2::AudioInstrumentPlayback* rval = 0;
	AudioMultiEffectPlayInst* playinst = GetPlayInst( soundname );
	if( playinst )
	{
		bool Is3d = playinst->GetData().GetEnable3D();

		orkprintf( "PlaySoundEx<%s>\n", soundname.c_str() );
		//StopSound( soundname );
		rval = playinst->Play( this, inote, ivel, pnode );
		if( Is3d )
		{
			//this->AddPlayback(rval);
		}
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
ork::lev2::AudioInstrumentPlayback* AudioEffectComponentInst::PlaySound( ork::PoolString soundname, const ork::TransformNode3D* pnode )
{
	return PlaySoundEx( soundname, -1, -1, pnode );
}
///////////////////////////////////////////////////////////////////////////////
void AudioEffectComponentInst::StopSound( ork::PoolString soundname )
{
}
void AudioEffectComponentInst::AddPlayback( ork::lev2::AudioInstrumentPlayback* pb )
{
	if( 0 == pb ) return;

	int itest = -1;
	int inumpb = mPlaybacks.size();
	for( int i=0; i<inumpb; i++ )
	{
		if( mPlaybacks[i]==pb )
		{
			itest = i;
		}
	}
	if( itest < 0 )
	{
		mPlaybacks.push_back(pb);
	}
}

void AudioEffectComponentInst::RemovePlayback( ork::lev2::AudioInstrumentPlayback* pb )
{
	if( 0 == pb ) return;

	int itest = -1;
	int inumpb = mPlaybacks.size();
	for( int i=0; i<inumpb; i++ )
	{
		if( mPlaybacks[i]==pb )
		{
			itest = i;
		}
	}
	if( itest >= 0 )
	{
		mPlaybacks.erase(mPlaybacks.begin()+itest);
	}
}

///////////////////////////////////////////////////////////////////////////////
bool AudioEffectComponentInst::DoNotify(const ork::event::Event *pev)
{
	if(const event::PlaySoundEvent *playevent = ork::rtti::autocast(pev))
	{
		const ork::PoolString& sound_name = playevent->GetSoundName();
		PlaySound( sound_name );
	}
	if(const event::StopSoundEvent *playevent = ork::rtti::autocast(pev))
	{
		const ork::PoolString& sound_name = playevent->GetSoundName();
		StopSound( sound_name );
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool AudioEffectComponentInst::DoLink(ork::ent::SceneInst *psi)
{
	ork::ent::Entity* pent = GetEntity();
	mDflowRecv = pent->GetTypedComponent<ork::ent::DataflowRecieverComponentInst>();
	return true;
}
bool AudioEffectComponentInst::DoStart(ork::ent::SceneInst *psi, const ork::CMatrix4 &world)
{
	////////////////////////////////////////////////
	// do we have an emitter ?
	////////////////////////////////////////////////

	if( mEmitters.size() )
	{
		if(mAmci)
		{
			mAmci->AddEmitter( this );
		}
	}
	return true;
}
///////////////////////////////////////////////////////////////////////////////
void AudioEffectComponentInst::DoStop(ork::ent::SceneInst *psi)
{
	StopAllSounds();
}
void AudioEffectComponentInst::StopAllSounds()
{
	for( orkvector<ork::lev2::AudioInstrumentPlayback*>::iterator it = mPlaybacks.begin(); it!=mPlaybacks.end(); it++ )
	{
		ork::lev2::AudioInstrumentPlayback* playback = *it;
		ork::lev2::AudioDevice::GetDevice()->StopSound( playback );
	}
	mPlaybacks.clear();
}
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
