////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/spatializer.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/reflect/properties/registerX.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void SoundFieldSendData::describeX(object::ObjectClass* clazz) {
  clazz->floatProperty("Level", float_range{-96, 24}, &SoundFieldSendData::_level);
  clazz->floatProperty("Spread", float_range{0, 1}, &SoundFieldSendData::_spread);
}

///////////////////////////////////////////////////////////////////////////////

void SpatializerData::describeX(object::ObjectClass* clazz) {
  clazz->directObjectProperty("SoundFieldSend", &SpatializerData::_soundfieldSend);
}

///////////////////////////////////////////////////////////////////////////////

void configureSoundFieldSend(
    lyrdata_ptr_t layer,
    spatializerdata_ptr_t spatializer,
    dspblkdata_ptr_t pannerBlock) {

  if (not spatializer)
    return;
  auto send = spatializer->_soundfieldSend;
  if (not send)
    return;

  // a send needs a direction, and the ONLY per-voice direction in the live path
  //  is the panner's ANGLE. an authored send on a voice with no panner is a
  //  contradiction in the data, so it says so rather than encoding at az=0.
  const char* pname = layer->_programdata ? layer->_programdata->_name.c_str() : "?";
  OrkAssertIFMT(
      pannerBlock != nullptr, //
      "program<%s> layer<%s> authors a soundfieldSend but has no PANNER2D - spatialize the sound or drop the send",
      pname,
      layer->_name.c_str());

  auto algd = layer->_algdata;
  OrkAssertIFMT(algd != nullptr, "layer<%s> soundfieldSend: layer has no alg graph", layer->_name.c_str());

  // structural coordinates, not pointers (see SoundFieldSendConfig).
  int stageindex = -1;
  int blockindex = -1;
  for (int si = 0; si < kmaxdspstagesperlayer and stageindex < 0; si++) {
    auto stage = algd->_stages[si];
    if (not stage)
      continue;
    for (size_t bi = 0; bi < stage->_blockdatas.size(); bi++) {
      if (stage->_blockdatas[bi] == pannerBlock) {
        stageindex = si;
        blockindex = int(bi);
        break;
      }
    }
  }
  OrkAssertIFMT(
      stageindex >= 0, //
      "layer<%s> soundfieldSend: panner block<%s> is not in this layer's alg graph",
      layer->_name.c_str(),
      pannerBlock->_name.c_str());

  int paramindex = -1;
  for (size_t pi = 0; pi < pannerBlock->_paramd.size(); pi++) {
    if (pannerBlock->_paramd[pi]->_name == "ANGLE") {
      paramindex = int(pi);
      break;
    }
  }
  OrkAssertIFMT(
      paramindex >= 0, //
      "layer<%s> soundfieldSend: panner block<%s> has no ANGLE param",
      layer->_name.c_str(),
      pannerBlock->_name.c_str());

  auto& cfg       = layer->_soundfieldSend;
  cfg._enabled    = true;
  cfg._levelDB    = send->_level;
  cfg._spread     = std::clamp(send->_spread, 0.0f, 1.0f);
  cfg._angleStage = stageindex;
  cfg._angleBlock = blockindex;
  cfg._angleParam = paramindex;
}

///////////////////////////////////////////////////////////////////////////////

void PannerSpatializerData::describeX(object::ObjectClass* clazz) {
  clazz->floatProperty("RefDistance", float_range{0.01f, 1000}, &PannerSpatializerData::_refDistance);
  clazz->floatProperty("MaxDistance", float_range{1, 10000}, &PannerSpatializerData::_maxDistance);
  clazz->floatProperty("Rolloff", float_range{0, 10}, &PannerSpatializerData::_rolloff);
  clazz->floatProperty("MinGainDB", float_range{-96, 0}, &PannerSpatializerData::_minGainDB);
  clazz->floatProperty("HeadShadowMix", float_range{0, 1}, &PannerSpatializerData::_headShadowMix);
  clazz->floatProperty("IidBaseFreq", float_range{100, 20000}, &PannerSpatializerData::_iidBaseFreq);
  clazz->floatProperty("IidMaxFreq", float_range{100, 20000}, &PannerSpatializerData::_iidMaxFreq);
}

///////////////////////////////////////////////////////////////////////////////

spatializer_ptr_t PannerSpatializerData::createInstance() const {
  return std::make_shared<PannerSpatializer>();
}

///////////////////////////////////////////////////////////////////////////////

void PannerSpatializer::configureBus(outbus_ptr_t bus) {
  auto layerdata = std::make_shared<LayerData>();
  auto stage     = layerdata->appendStage("PAN");
  stage->setNumIos(2, 2);
  _pannerBlock = stage->appendTypedBlock<PANNER2D>("PANNER");
  bus->setBusDSP(layerdata);
}

///////////////////////////////////////////////////////////////////////////////

void PannerSpatializer::updateSpatialParams(
    const fmtx4& listenerMatrix,
    const fvec3& emitterPos) {
  if (!_pannerBlock)
    return;

  fmtx4 invListener = listenerMatrix.inverse();
  fvec4 relPos4     = fvec4(emitterPos, 1.0f).transform(invListener);
  fvec3 relPos(relPos4.x, relPos4.y, relPos4.z);

  float distance = relPos.magnitude();
  float azimuth  = atan2f(relPos.x, relPos.z);

  auto angleParam = _pannerBlock->paramByName("ANGLE");
  auto distParam  = _pannerBlock->paramByName("DISTANCE");
  if (angleParam)
    angleParam->_coarse = azimuth;
  if (distParam)
    distParam->_coarse = std::max(1.0f, distance);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::audio::singularity::SoundFieldSendData, "SoundFieldSendData");
ImplementReflectionX(ork::audio::singularity::SpatializerData, "SpatializerData");
ImplementReflectionX(ork::audio::singularity::PannerSpatializerData, "PannerSpatializerData");
