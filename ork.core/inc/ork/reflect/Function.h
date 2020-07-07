////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::reflect {
///////////////////////////////////////////////////////////////////////////////

using bidi_t = serdes::BidirectionalSerializer;

template <typename FunctionType> struct Function {};

///////////////////////////////////////////////////////////////////////////////

template <> struct Function<void (*)()> {
  typedef void ReturnType;
  typedef void (*FunctionType)();

  struct Parameters__ {
    enum { Count = 0 };
    void Apply(bidi_t& bidi, int) {
      bidi.Fail();
    };
    void Apply(bidi_t& bidi, int) const {
      bidi.Fail();
    }
  };

  typedef Parameters__ Parameters;

  static void invoke(FunctionType f, const Parameters&) {
    return (*f)();
  }

  static void SetParameters(void*) {
  }
};

///////////////////////////////////////////////////////////////////////////////

template <typename R> struct Function<R (*)()> {
  typedef Function<void (*)()> GenericFunction__;
  typedef R ReturnType;
  typedef R (*FunctionType)();

  struct Parameters__ {
    enum { Count = 0 };
    void Apply(bidi_t& bidi, int) {
      bidi.Fail();
    };
    void Apply(bidi_t& bidi, int) const {
      bidi.Fail();
    }
  };

  typedef typename GenericFunction__::Parameters__ Parameters;

  static R invoke(FunctionType f, const Parameters&) {
    return (*f)();
  }
};

///////////////////////////////////////////////////////////////////////////////
template <
    typename R, //
    typename P0>
struct Function<R (*)(P0)> {
  typedef Function<R (*)()> ParentFunction__;
  typedef Function<void (*)(P0)> GenericFunction__;
  typedef R ReturnType;
  typedef R (*FunctionType)(P0);

  struct Parameters__ : public ParentFunction__::Parameters {
    P0 mParam_00;

    enum { Count = 1 + ParentFunction__::Parameters::Count };

    void Apply(bidi_t& bidi, int parameter_index) {
      if (parameter_index == Count - 1)
        ; // Serialize<P0>(&mParam_00, &mParam_00, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }

    void Apply(bidi_t& bidi, int parameter_index) const {
      if (parameter_index == Count - 1)
        ; // Serialize<P0>(&mParam_00, NULL, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }
  };

  typedef typename GenericFunction__::Parameters__ Parameters;

  static R invoke(FunctionType f, const Parameters& params) {
    return (*f)(params.mParam_00);
  }
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename P0>
inline void SetParameters(ReturnType (*)(P0), void* param_data, P0 a0) {
  typedef typename Function<ReturnType (*)(P0)>::Parameters Parameters;
  Parameters* params = static_cast<Parameters*>(param_data);
  params->mParam_00  = a0;
}

///////////////////////////////////////////////////////////////////////////////

template <
    typename R, //
    typename P0,
    typename P1>
struct Function<R (*)(P0, P1)> {
  typedef Function<R (*)(P0)> ParentFunction__;
  typedef Function<void (*)(P0, P1)> GenericFunction__;
  typedef R ReturnType;
  typedef R (*FunctionType)(P0, P1);

  struct Parameters__ : public ParentFunction__::Parameters {
    P1 mParam_01;

    enum { Count = 1 + ParentFunction__::Parameters::Count };

    void Apply(bidi_t& bidi, int parameter_index) {
      if (parameter_index == Count - 1)
        ; // Serialize<P1>(&mParam_01, &mParam_01, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }

    void Apply(bidi_t& bidi, int parameter_index) const {
      if (parameter_index == Count - 1)
        ; // Serialize<P1>(&mParam_01, NULL, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }
  };

  typedef typename GenericFunction__::Parameters__ Parameters;

  static R invoke(FunctionType f, const Parameters& params) {
    return (*f)(params.mParam_00, params.mParam_01);
  }
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename P0,
    typename P1>
inline void SetParameters(ReturnType (*)(P0, P1), void* param_data, P0 a0, P1 a1) {
  typedef typename Function<ReturnType (*)(P0, P1)>::Parameters Parameters;
  Parameters* params = static_cast<Parameters*>(param_data);
  params->mParam_00  = a0;
  params->mParam_01  = a1;
}

///////////////////////////////////////////////////////////////////////////////

template <
    typename R, //
    typename C>
struct Function<R (C::*)()> {
  typedef Function<void (*)()> GenericFunction__;
  typedef R ReturnType;
  typedef C ClassType;
  typedef R (C::*FunctionType)();

  struct Parameters__ {
    enum { Count = 0 };
    void Apply(bidi_t& bidi, int) {
      bidi.Fail();
    };
    void Apply(bidi_t& bidi, int) const {
      bidi.Fail();
    }
  };

  typedef typename GenericFunction__::Parameters__ Parameters;

  static R invoke(ClassType& object, FunctionType f, const Parameters&) {
    return (object.*f)();
  }
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename R, //
    typename C,
    typename P0>
struct Function<R (C::*)(P0)> {
  typedef Function<R (C::*)()> ParentFunction__;
  typedef Function<void (*)(P0)> GenericFunction__;
  typedef R ReturnType;
  typedef C ClassType;
  typedef R (C::*FunctionType)(P0);

  struct Parameters__ : public ParentFunction__::Parameters {
    P0 mParam_00;

    enum { Count = 1 + ParentFunction__::Parameters::Count };

    void Apply(bidi_t& bidi, int parameter_index) {
      if (parameter_index == Count - 1)
        ; // Serialize<P0>(&mParam_00, &mParam_00, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }

    void Apply(bidi_t& bidi, int parameter_index) const {
      if (parameter_index == Count - 1)
        ; // Serialize<P0>(&mParam_00, NULL, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }
  };

  typedef typename GenericFunction__::Parameters__ Parameters;

  static R invoke(ClassType& object, FunctionType f, const Parameters& params) {
    return (object.*f)(params.mParam_00);
  }
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename ClassType,
    typename P0>
inline void SetParameters(ReturnType (ClassType::*)(P0), void* param_data, P0 a0) {
  typedef typename Function<ReturnType (ClassType::*)(P0)>::Parameters Parameters;
  Parameters* params = static_cast<Parameters*>(param_data);
  params->mParam_00  = a0;
}

///////////////////////////////////////////////////////////////////////////////

template <
    typename R, //
    typename C,
    typename P0,
    typename P1>
struct Function<R (C::*)(P0, P1)> {
  typedef Function<R (C::*)(P0)> ParentFunction__;
  typedef Function<void (*)(P0, P1)> GenericFunction__;
  typedef R ReturnType;
  typedef C ClassType;
  typedef R (C::*FunctionType)(P0, P1);

  struct Parameters__ : public ParentFunction__::Parameters {
    P1 mParam_01;

    enum { Count = 1 + ParentFunction__::Parameters::Count };

    void Apply(bidi_t& bidi, int parameter_index) {
      if (parameter_index == Count - 1)
        ; // Serialize<P1>(&mParam_01, &mParam_01, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }

    void Apply(bidi_t& bidi, int parameter_index) const {
      if (parameter_index == Count - 1)
        ; // Serialize<P1>(&mParam_01, NULL, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }
  };

  typedef typename GenericFunction__::Parameters__ Parameters;

  static R invoke(ClassType& object, FunctionType f, const Parameters& params) {
    return (object.*f)(params.mParam_00, params.mParam_01);
  }
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename ClassType,
    typename P0,
    typename P1>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1), void* param_data, P0 a0, P1 a1) {
  typedef typename Function<ReturnType (ClassType::*)(P0, P1)>::Parameters Parameters;
  Parameters* params = static_cast<Parameters*>(param_data);
  params->mParam_00  = a0;
  params->mParam_01  = a1;
}

///////////////////////////////////////////////////////////////////////////////

template <typename R, typename C> //
struct Function<R (C::*)() const> {
  typedef Function<void (*)()> GenericFunction__;
  typedef R ReturnType;
  typedef C ClassType;
  typedef R (C::*FunctionType)() const;

  struct Parameters__ {
    enum { Count = 0 };
    void Apply(bidi_t& bidi, int) {
      bidi.Fail();
    };
    void Apply(bidi_t& bidi, int) const {
      bidi.Fail();
    }
  };

  typedef typename GenericFunction__::Parameters__ Parameters;

  static R invoke(ClassType& object, FunctionType f, const Parameters&) {
    return (object.*f)();
  }
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename R, //
    typename C,
    typename P0>
struct Function<R (C::*)(P0) const> {
  typedef Function<R (C::*)()> ParentFunction__;
  typedef Function<void (*)(P0)> GenericFunction__;
  typedef R ReturnType;
  typedef C ClassType;
  typedef R (C::*FunctionType)(P0) const;

  struct Parameters__ : public ParentFunction__::Parameters {
    P0 mParam_00;

    enum { Count = 1 + ParentFunction__::Parameters::Count };

    void Apply(bidi_t& bidi, int parameter_index) {
      if (parameter_index == Count - 1)
        ; // Serialize<P0>(&mParam_00, &mParam_00, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }

    void Apply(bidi_t& bidi, int parameter_index) const {
      if (parameter_index == Count - 1)
        ; // Serialize<P0>(&mParam_00, NULL, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }
  };

  typedef typename GenericFunction__::Parameters__ Parameters;

  static R invoke(ClassType& object, FunctionType f, const Parameters& params) {
    return (object.*f)(params.mParam_00);
  }
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename ClassType,
    typename P0>
inline void SetParameters(ReturnType (ClassType::*)(P0) const, void* param_data, P0 a0) {
  typedef typename Function<ReturnType (ClassType::*)(P0) const>::Parameters Parameters;
  Parameters* params = static_cast<Parameters*>(param_data);
  params->mParam_00  = a0;
}

///////////////////////////////////////////////////////////////////////////////

template <
    typename R, //
    typename C,
    typename P0,
    typename P1>
struct Function<R (C::*)(P0, P1) const> {
  typedef Function<R (C::*)(P0)> ParentFunction__;
  typedef Function<void (*)(P0, P1)> GenericFunction__;
  typedef R ReturnType;
  typedef C ClassType;
  typedef R (C::*FunctionType)(P0, P1) const;

  struct Parameters__ : public ParentFunction__::Parameters {
    P1 mParam_01;

    enum { Count = 1 + ParentFunction__::Parameters::Count };

    void Apply(bidi_t& bidi, int parameter_index) {
      if (parameter_index == Count - 1)
        ; // Serialize<P1>(&mParam_01, &mParam_01, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }

    void Apply(bidi_t& bidi, int parameter_index) const {
      if (parameter_index == Count - 1)
        ; // Serialize<P1>(&mParam_01, NULL, bidi);
      else
        ; // ParentFunction__::Parameters::Apply(bidi, parameter_index);
    }
  };

  typedef typename GenericFunction__::Parameters__ Parameters;

  static R invoke(ClassType& object, FunctionType f, const Parameters& params) {
    return (object.*f)(params.mParam_00, params.mParam_01);
  }
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename ClassType,
    typename P0,
    typename P1>
inline void SetParameters(ReturnType (ClassType::*)(P0, P1) const, void* param_data, P0 a0, P1 a1) {
  typedef typename Function<ReturnType (ClassType::*)(P0, P1) const>::Parameters Parameters;
  Parameters* params = static_cast<Parameters*>(param_data);
  params->mParam_00  = a0;
  params->mParam_01  = a1;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::reflect
