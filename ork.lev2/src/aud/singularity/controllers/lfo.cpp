#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synth.h>
#include <ork/reflect/properties/registerX.inl>
ImplementReflectionX(ork::audio::singularity::LfoData, "SynLfoData");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
void LfoData::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

LfoData::LfoData()
    : _initialPhase(0.0f)
    , _minRate(1.0f)
    , _maxRate(1.0f)
    , _shape("Sine") {
}

ControllerInst* LfoData::instantiate(Layer* l) const {
  auto r = new LfoInst(this, l);
  return r;
}

///////////////////////////////////////////////////////////////////////////////

LfoInst::LfoInst(const LfoData* data, Layer* l)
    : ControllerInst(l)
    , _data(data)
    , _phaseInc(0.0f)
    , _phase(0.0f)
    , _currate(0.0f)
    , _enabled(false)
    , _rateLerp(0.0f)
    , _bias(0.0f) {
  _mapper = [](float inp) -> float { return 0.0f; };
}

///////////////////////////////////////////////////////////////////////////////

void LfoInst::reset() {
  _phaseInc = (0.0f);
  _phase    = (0.0f);
  _currate  = (0.0f);
  _enabled  = false;
  _rateLerp = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void LfoInst::keyOn(const KeyOnInfo& KOI) // final
{
  if (nullptr == _data)
    return;

  _phase    = _data->_initialPhase * 0.25f;
  _enabled  = true;
  _rateLerp = 0.0f;
  _bias     = 0.0f;
  if (_data->_controller == "ON") {
    _rateLerp = 1.0f;
  }
  if (_data->_shape == "Sine")
    _mapper = [](float inp) -> float { return sinf(inp * pi2); };
  else if (_data->_shape == "None")
    _mapper = [](float inp) -> float { return 0.0f; };
  else if (_data->_shape == "+Sine")
    _mapper = [](float inp) -> float { return 0.5f + sinf(inp * pi2) * 0.5f; };
  else if (_data->_shape == "Triangle")
    _mapper = [](float inp) -> float {
      float tri = fabs(fmod(inp * 4.0f, 4.0f) - 2.0f) - 1.0f;
      return tri;
    };
  else if (_data->_shape == "+Triangle")
    _mapper = [](float inp) -> float {
      float tri = fabs(fmod(inp * 4.0f, 4.0f) - 2.0f) * 0.5f;
      return tri;
    };
  else if (_data->_shape == "Square")
    _mapper = [](float inp) -> float {
      float squ = sinf(inp * pi2) >= 0.0f ? +1.0f : -1.0f;
      return squ;
    };
  else if (_data->_shape == "+Square")
    _mapper = [](float inp) -> float {
      float squ = sinf(inp * pi2) >= 0.0f ? +1.0f : 0.0f;
      return squ;
    };
  else if (_data->_shape == "Rise Saw")
    _mapper = [](float inp) -> float {
      float saw = fmod(inp * 2.0f, 2.0f) - 1.0f;
      return saw;
    };
  else if (_data->_shape == "Fall Saw")
    _mapper = [](float inp) -> float {
      float saw = fmod(inp * 2.0f, 2.0f) - 1.0f;
      return -saw;
    };
  else if (_data->_shape == "4 Step")
    _mapper = [](float inp) -> float {
      static const float table[] = {
          0.25f,
          1.0f,
          -1.0f,
          -.25,
      };
      int x = int(inp * 4.0f) & 3;
      return table[x];
    };
  else if (_data->_shape == "+4 Step")
    _mapper = [](float inp) -> float {
      static const float table[] = {
          0.666666f, // cents_to_linear_freq_ratio(900),
          1.0f,
          0.0f,
          0.333333f // cents_to_linear_freq_ratio(300),
      };
      int x = int(inp * 4.0f) & 3;
      return table[x];
    };
  else if (_data->_shape == "+3 Step")
    _mapper = [](float inp) -> float {
      static const float table[] = {
          1.0f,
          0.0f,
          0.5f,
      };
      int x = int((inp + 0.3333f) * 3.0f) % 3;
      return table[x];
    };
  else
    _mapper = [](float inp) -> float { return float(rand() % 100) * 0.01f; };
}

///////////////////////////////////////////////////////////////////////////////

void LfoInst::keyOff() // final
{
}

///////////////////////////////////////////////////////////////////////////////

void LfoInst::compute() // final
{
  if (nullptr == _data) {
    _curval = 0.0f;
  } else {
    float SR = getSampleRate();
    float dt = float(_layer->_dspwritecount) / SR;

    _currate = lerp(_data->_minRate, _data->_maxRate, _rateLerp);

    // printf( "lforate<%f>\n", rate );
    _phaseInc = dt * _currate;
    _phase += _phaseInc;
    _curval = _mapper(_phase);
  }
}

} // namespace ork::audio::singularity
