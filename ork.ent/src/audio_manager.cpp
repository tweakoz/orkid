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
#include <ork/reflect/enum_serializer.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioSystemData, "AudioSystemData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioManagerComponentInst, "AudioManagerComponentInst");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
void AudioSystemData::Describe()
{
//	ork::ent::RegisterFamily<AudioSystemData>(ork::AddPooledLiteral("audio"));

	ork::reflect::RegisterProperty("Reverb", &AudioSystemData::ReverbAccessor);

	/////////////////////////////////////////////////////////////////
	// distance attenuation parameters
	/////////////////////////////////////////////////////////////////

	ork::reflect::RegisterProperty("DistanceScale", &AudioSystemData::mfDistScale);
	ork::reflect::RegisterProperty("DistanceMin", &AudioSystemData::mfDistMin);
	ork::reflect::RegisterProperty("DistanceMax", &AudioSystemData::mfDistMax);
	ork::reflect::RegisterProperty("DistanceAttenPower", &AudioSystemData::mfDistAttenPower);
	
	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceScale", "editor.range.min", "0.001" );
	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceScale", "editor.range.max", "100.0" );
	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceScale", "editor.range.log", "true" );

	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceMin", "editor.range.min", "0.1" );
	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceMin", "editor.range.max", "10000.0" );
	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceMin", "editor.range.log", "true" );

	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceMax", "editor.range.min", "0.1" );
	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceMax", "editor.range.max", "10000.0" );
	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceMax", "editor.range.log", "true" );

	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceAttenPower", "editor.range.min", "0.5" );
	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceAttenPower", "editor.range.max", "2.0" );
	ork::reflect::AnnotatePropertyForEditor< AudioSystemData >("DistanceAttenPower", "editor.range.log", "true" );

}
///////////////////////////////////////////////////////////////////////////////
const float g_allsoundmod = 0.8f;
AudioSystemData::AudioSystemData()
	: mfDistMin(0.1f) // m
	, mfDistMax(10.0f) // m
	, mfDistScale( 0.003f )
	, mfDistAttenPower(0.0f)
{
}
///////////////////////////////////////////////////////////////////////////////
ork::ent::System *AudioSystemData::createSystem(ork::ent::SceneInst *pinst) const
{
	return new AudioManagerComponentInst( *this, pinst );
}
///////////////////////////////////////////////////////////////////////////////
void AudioManagerComponentInst::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
AudioManagerComponentInst::AudioManagerComponentInst( const AudioSystemData& ascd, ork::ent::SceneInst* psi )
	: System( & ascd, psi )
	, mAmcd( ascd )
{
	ork::lev2::AudioDevice::GetDevice()->SetReverbProperties( ascd.GetReverbProperties() );


}
AudioManagerComponentInst::~AudioManagerComponentInst()
{
	ork::lev2::AudioDevice::GetDevice()->ReInitDevice();
}
///////////////////////////////////////////////////////////////////////////////
void AudioManagerComponentInst::DoUpdate(ork::ent::SceneInst *inst)
{
	ork::lev2::AudioDevice* pdev = ork::lev2::AudioDevice::GetDevice();

	const ork::CCameraData* camdat1 = inst->GetCameraData(ork::AddPooledLiteral("game1"));
	const ork::CCameraData* camdat2 = inst->GetCameraData(ork::AddPooledLiteral("game2"));

	for( orkvector<AudioEffectComponentInst*>::const_iterator it=mEmitters.begin(); it!=mEmitters.end(); it++ )
	{
		AudioEffectComponentInst* aeci = (*it);
		aeci->UpdateEmitter( camdat1, camdat2 );
	}
	if( camdat1 )
	{
		ork::CVector3 ListenerPos = camdat1->GetEye();
		ork::CVector3 ListenerUp = -camdat1->GetYNormal();
		ork::CVector3 ListenerFw = camdat1->GetZNormal();
		pdev->SetListener1( ListenerPos, ListenerUp, ListenerFw );
	}
	if( camdat2 )
	{
		ork::CVector3 ListenerPos = camdat2->GetEye();
		ork::CVector3 ListenerUp = -camdat2->GetYNormal();
		ork::CVector3 ListenerFw = camdat2->GetZNormal();
		pdev->SetListener2( ListenerPos, ListenerUp, ListenerFw );
	}
	else if( camdat1 )
	{
		ork::CVector3 ListenerPos = camdat1->GetEye();
		ork::CVector3 ListenerUp = -camdat1->GetYNormal();
		ork::CVector3 ListenerFw = camdat1->GetZNormal();
		pdev->SetListener2( ListenerPos, ListenerUp, ListenerFw );
	}

	pdev->SetDistMin(mAmcd.GetDistMin());
	pdev->SetDistMax(mAmcd.GetDistMax());
	pdev->SetDistScale(mAmcd.GetDistScale());
	pdev->SetDistAttenPower(mAmcd.GetDistAttenPower());

	pdev->Update( inst->GetDeltaTime() );

	//////////////////////////////////////////////////////////////////////////////////
}
void AudioManagerComponentInst::DoStop(ork::ent::SceneInst *psi)
{
	ork::lev2::AudioDevice::GetDevice()->ReInitDevice();
}
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
