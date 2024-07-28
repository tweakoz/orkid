////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synth.h>
#include <ork/reflect/properties/registerX.inl>
ImplementReflectionX(ork::audio::singularity::FunData, "SynFun");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
void FunData::describeX(class_t* clazz) {
  clazz->directProperty("a", &FunData::_a);
  clazz->directProperty("b", &FunData::_b);
  clazz->directProperty("op", &FunData::_op);
}

///////////////////////////////////////////////////////////////////////////////

ControllerInst* FunData::instantiate(layer_ptr_t l) const // final
{
  return new FunInst(this, l);
}

///////////////////////////////////////////////////////////////////////////////

controllerdata_ptr_t FunData::clone() const {
  auto rval = std::make_shared<FunData>();
  rval->_a  = _a;
  rval->_b  = _b;
  rval->_op = _op;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

FunInst::FunInst(const FunData* data, layer_ptr_t l)
    : ControllerInst(l)
    , _data(data) {
  _a  = [] -> float { return 0.0f; };
  _b  = [] -> float { return 0.0f; };
  _op = [] -> float { return 0.0f; };
}

///////////////////////////////////////////////////////////////////////////////

void FunInst::compute() // final
{
  _value.x = _op();
}

///////////////////////////////////////////////////////////////////////////////

struct Lowpass {
  Lowpass()
      : a(0.99f)
      , b(1.f - a)
      , z(0) {
  }
  float compute(float in) {
    z = (in * b) + (z * a);
    return z;
  }

  float a, b, z;
};

void FunInst::keyOn(const KeyOnInfo& KOI) // final
{
  auto l = KOI._layer;

  if (nullptr == _data)
    return;

  _a  = l->getController(_data->_a);
  _b  = l->getController(_data->_b);
  _op = [] -> float { return 0.0f; };

  const auto& op = _data->_op;
  if (0)
    return;
  else if (op == "a+b")
    _op = [=]() -> float {
      float a = this->_a();
      float b = this->_b();
      return (a + b);
    };
  else if (op == "a-b")
    _op = [=]() -> float {
      float a    = this->_a();
      float b    = this->_b();
      float rval = a - b;
      return rval;
    };
  else if (op == "a*b")
    _op = [=]() -> float {
      float a    = this->_a();
      float b    = this->_b();
      float rval = a * b;
      // printf("a<%g> b<%g> a*b<%g>\n", a, b, rval);
      return rval;
    };
  else if (op == "a*a*b")
    _op = [=]() -> float {
      float a    = this->_a();
      float b    = this->_b();
      float rval = a * a * b;
      // printf("a<%g> b<%g> a*b<%g>\n", a, b, rval);
      return rval;
    };
  else if (op == "a*a*b*b")
    _op = [=]() -> float {
      float a    = this->_a();
      float b    = this->_b();
      float rval = a * a * b * b;
      // printf("a<%g> b<%g> a*b<%g>\n", a, b, rval);
      return rval;
    };
  else if (op == "(a+b)/2")
    _op = [=]() -> float {
      float a = this->_a();
      float b = this->_b();
      return (a + b) * 0.5f;
    };
  else if (op == "a/2+b")
    _op = [=]() -> float {
      float a = this->_a();
      float b = this->_b();
      return (a * 0.5f) + b;
    };
  else if (op == "Quantize B To A")
    _op = [=]() -> float {
      float a       = fabs(this->_a());
      int inumsteps = int(a * 15.0f) + 1;

      float b = this->_b();

      if (a == 1)
        return clip_float(b, -1, 1);

      int ib = b * float(inumsteps);

      float bb = float(ib) / float(inumsteps);

      return clip_float(bb, -1, 1); /// inva;
    };
  else if (op == "lowpass(f=a,b)") {
    auto lowpass = new Lowpass;
    _op          = [=]() -> float {
      float a   = this->_a();
      float b   = this->_b();
      float out = lowpass->compute(b);
      return out;
    };
  } else if (op == "Sample B on A") {
    auto state1 = new float(0.0f);
    auto state2 = new float(0.0f);
    _op         = [=]() -> float {
      float a = this->_a();
      float b = this->_b();
      if (a > 0.5f and (*state2) < 0.5f)
        (*state1) = b;
      (*state2) = a;
      return (*state1);
    };
  } else if (op == "Track B While A") {
    auto state1 = new float(0.0f);
    _op         = [=]() -> float {
      float a = this->_a();
      float b = this->_b();
      if (a > 0.5f)
        (*state1) = b;
      return (*state1);
    };
  } else if (op == "a OR b") {
    auto state1 = new float(0.0f);
    _op         = [=]() -> float {
      bool a = (this->_a() > 0.5f);
      bool b = (this->_b() > 0.5f);
      return float(a or b);
    };
  } else if (op == "max(a,b)") {
    auto state1 = new float(0.0f);
    _op         = [=]() -> float {
      float a = this->_a();
      float b = this->_b();
      return (a > b) ? a : b;
    };
  } else if (op == "min(a,b)") {
    auto state1 = new float(0.0f);
    _op         = [=]() -> float {
      float a = this->_a();
      float b = this->_b();
      return (a < b) ? a : b;
    };
  } else if (op == "(a+2b)/3") {
    auto state1 = new float(0.0f);
    _op         = [=]() -> float {
      float a = this->_a();
      float b = this->_b();
      return (a + 2.0f * b) * 0.3333333f;
    };
  } else if (op == "|a-b|") {
    auto state1 = new float(0.0f);
    _op         = [=]() -> float {
      float a = this->_a();
      float b = this->_b();
      return fabs(a - b);
    };
  }
}
void FunInst::keyOff() // final
{
}

} // namespace ork::audio::singularity
