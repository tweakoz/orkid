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
#include <ork/reflect/enum_serializer.inl>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioSystemData, "AudioSystemData");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
void AudioSystemData::Describe(){
	using namespace ork::reflect;
	RegisterProperty("Reverb", &AudioSystemData::ReverbAccessor);
	/////////////////////////////////////////////////////////////////
	// distance attenuation parameters
	/////////////////////////////////////////////////////////////////
	RegisterProperty("DistanceScale", &AudioSystemData::mfDistScale);
	RegisterProperty("DistanceMin", &AudioSystemData::mfDistMin);
	RegisterProperty("DistanceMax", &AudioSystemData::mfDistMax);
	RegisterProperty("DistanceAttenPower", &AudioSystemData::mfDistAttenPower);

	AnnotatePropertyForEditor<AudioSystemData>("DistanceScale", "editor.range.min", "0.001" );
	AnnotatePropertyForEditor<AudioSystemData>("DistanceScale", "editor.range.max", "100.0" );
	AnnotatePropertyForEditor<AudioSystemData>("DistanceScale", "editor.range.log", "true" );

	AnnotatePropertyForEditor<AudioSystemData>("DistanceMin", "editor.range.min", "0.1" );
	AnnotatePropertyForEditor<AudioSystemData>("DistanceMin", "editor.range.max", "10000.0" );
	AnnotatePropertyForEditor<AudioSystemData>("DistanceMin", "editor.range.log", "true" );

	AnnotatePropertyForEditor<AudioSystemData>("DistanceMax", "editor.range.min", "0.1" );
	AnnotatePropertyForEditor<AudioSystemData>("DistanceMax", "editor.range.max", "10000.0" );
	AnnotatePropertyForEditor<AudioSystemData>("DistanceMax", "editor.range.log", "true" );

	AnnotatePropertyForEditor<AudioSystemData>("DistanceAttenPower", "editor.range.min", "0.5" );
	AnnotatePropertyForEditor<AudioSystemData>("DistanceAttenPower", "editor.range.max", "2.0" );
	AnnotatePropertyForEditor<AudioSystemData>("DistanceAttenPower", "editor.range.log", "true" );
}
///////////////////////////////////////////////////////////////////////////////
const float g_allsoundmod = 0.8f;
AudioSystemData::AudioSystemData()
	: mfDistMin(0.1f) // m
	, mfDistMax(10.0f) // m
	, mfDistScale( 0.003f )
	, mfDistAttenPower(0.0f){
}
///////////////////////////////////////////////////////////////////////////////
ork::ent::System *AudioSystemData::createSystem(ork::ent::Simulation *pinst) const{
	return new AudioSystem( *this, pinst );
}
///////////////////////////////////////////////////////////////////////////////
AudioSystem::AudioSystem( const AudioSystemData& ascd, ork::ent::Simulation* psi )
	: System( & ascd, psi )
	, mAmcd( ascd ){
	ork::lev2::AudioDevice::GetDevice()->SetReverbProperties( ascd.GetReverbProperties() );


}
///////////////////////////////////////////////////////////////////////////////
AudioSystem::~AudioSystem(){
	ork::lev2::AudioDevice::GetDevice()->ReInitDevice();
}
///////////////////////////////////////////////////////////////////////////////
void AudioSystem::DoUpdate(ork::ent::Simulation* inst){
	auto pdev = ork::lev2::AudioDevice::GetDevice();

	auto camdat1 = inst->GetCameraData(ork::AddPooledLiteral("game1"));
	auto camdat2 = inst->GetCameraData(ork::AddPooledLiteral("game2"));

	for( auto emitter : mEmitters )
		emitter->UpdateEmitter( camdat1, camdat2 );

	if( camdat1 ){
		auto ListenerPos = camdat1->GetEye();
		auto ListenerUp = -camdat1->GetYNormal();
		auto ListenerFw = camdat1->GetZNormal();
		pdev->SetListener1( ListenerPos, ListenerUp, ListenerFw );
	}
	if( camdat2 ){
		auto ListenerPos = camdat2->GetEye();
		auto ListenerUp = -camdat2->GetYNormal();
		auto ListenerFw = camdat2->GetZNormal();
		pdev->SetListener2( ListenerPos, ListenerUp, ListenerFw );
	}
	else if( camdat1 ){
		auto ListenerPos = camdat1->GetEye();
		auto ListenerUp = -camdat1->GetYNormal();
		auto ListenerFw = camdat1->GetZNormal();
		pdev->SetListener2( ListenerPos, ListenerUp, ListenerFw );
	}

	pdev->SetDistMin(mAmcd.GetDistMin());
	pdev->SetDistMax(mAmcd.GetDistMax());
	pdev->SetDistScale(mAmcd.GetDistScale());
	pdev->SetDistAttenPower(mAmcd.GetDistAttenPower());

	pdev->Update( inst->GetDeltaTime() );

}
////////////////////////////////////////////////////////////////////////////////
void AudioSystem::DoStop(ork::ent::Simulation *psi){
	ork::lev2::AudioDevice::GetDevice()->ReInitDevice();
}
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
