
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/synth.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

lyrdata_ptr_t programData::newLayer() {
  auto l = std::make_shared<layerData>();
  _layerDatas.push_back(l);
  return l;
}

///////////////////////////////////////////////////////////////////////////////

bool RateLevelEnvData::isBiPolar() const {
  bool rval = false;
  for (auto& item : _segments)
    if (item._level < 0.0f)
      rval = true;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

kmregion* keymap::getRegion(int note, int vel) const {
  int k2vel = vel / 18; // map 0..127 -> 0..7

  for (auto r : _regions) {
    // printf( "testing region<%p> for note<%d>\n", r, note );
    // printf( "lokey<%d>\n", r->_lokey );
    // printf( "hikey<%d>\n", r->_hikey );
    // printf( "lovel<%d>\n", r->_lovel );
    // printf( "hivel<%d>\n", r->_hivel );
    if (note >= r->_lokey && note <= r->_hikey) {
      if (k2vel >= r->_lovel && k2vel <= r->_hivel) {
        return r;
      }
    }
  }

  return nullptr;
}

} // namespace ork::audio::singularity
