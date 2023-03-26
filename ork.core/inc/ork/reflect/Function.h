////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::reflect {
///////////////////////////////////////////////////////////////////////////////

using bidi_t = serdes::BidirectionalSerializer;

using voidfn_t = void(*)();

template <typename FunctionType> struct Function {};

///////////////////////////////////////////////////////////////////////////////

template <> struct Function<voidfn_t> {
  
  struct Parameters__ {
    enum { Count = 0 };
    void Apply(bidi_t& bidi, int);
    void Apply(bidi_t& bidi, int) const;
  };

  using ReturnType = void;
  using FunctionType = voidfn_t;
  using Parameters = Parameters__;

  static void invoke(FunctionType f, const Parameters&);
  static void SetParameters(void*);
};

template <typename A0> 
struct Function<void(*)(A0)> {
  
  struct Parameters__ {
    enum { Count = 0 };
    void Apply(bidi_t& bidi, int);
    void Apply(bidi_t& bidi, int) const;
  };

  using ReturnType = void;
  using FunctionType =  void(*)(A0);
  using Parameters = Parameters__;

  static void invoke(FunctionType f, const Parameters&);
  static void SetParameters(void*);
};

template <typename C>
struct Function<void(C::*)()> {
  struct Parameters__ {
    enum { Count = 0 };
    void Apply(bidi_t& bidi, int);
    void Apply(bidi_t& bidi, int) const;
  };

  using GenericFunction__ = Function<void (*)()>;
  using ClassType = C ;
  using FunctionType = void(C::*)();
  using Parameters = Parameters__ ;
  using ReturnType = void;

  static void invoke(ClassType& object, FunctionType f, const Parameters&);
};

///////////////////////////////////////////////////////////////////////////////

/*template <typename R> struct Function<R (*)()> {
  //typedef Function<void (*)()> GenericFunction__;
  typedef R ReturnType;
  typedef R (*FunctionType)();

  struct Parameters__ {
    enum { Count = 0 };
    void Apply(bidi_t& bidi, int);
    void Apply(bidi_t& bidi, int) const;
  };

  using Parameters = Parameters__;

  static R invoke(FunctionType f, const Parameters&);
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

    void Apply(bidi_t& bidi, int parameter_index);
    void Apply(bidi_t& bidi, int parameter_index) const;
  };

  using Parameters = Parameters__ ;

  static R invoke(FunctionType f, const Parameters& params);
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename P0>
    void SetParameters(ReturnType (*)(P0), void* param_data, P0 a0);

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

    void Apply(bidi_t& bidi, int parameter_index);
    void Apply(bidi_t& bidi, int parameter_index) const;

    using Parameters = Parameters__ ;

    static R invoke(FunctionType f, const Parameters& params);
  };

};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename P0,
    typename P1>
    void SetParameters(ReturnType (*)(P0, P1), void* param_data, P0 a0, P1 a1);

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
    void Apply(bidi_t& bidi, int);
    void Apply(bidi_t& bidi, int) const;
  };

  using Parameters = Parameters__ ;

  static R invoke(ClassType& object, FunctionType f, const Parameters&);
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

    void Apply(bidi_t& bidi, int parameter_index);
    void Apply(bidi_t& bidi, int parameter_index) const;
  };

  using Parameters = Parameters__ ;

  static R invoke(ClassType& object, FunctionType f, const Parameters& params);
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename ClassType,
    typename P0>
    void SetParameters(ReturnType (ClassType::*)(P0), void* param_data, P0 a0);

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

    void Apply(bidi_t& bidi, int parameter_index);
    void Apply(bidi_t& bidi, int parameter_index) const;
  };

  using Parameters = Parameters__ ;

  static R invoke(ClassType& object, FunctionType f, const Parameters& params);
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename ClassType,
    typename P0,
    typename P1>
    void SetParameters(ReturnType (ClassType::*)(P0, P1), //
                          void* param_data, P0 a0, P1 a1);

///////////////////////////////////////////////////////////////////////////////

template <typename R, typename C> //
struct Function<R (C::*)() const> {
  typedef Function<void (*)()> GenericFunction__;
  typedef R ReturnType;
  typedef C ClassType;
  typedef R (C::*FunctionType)() const;

  struct Parameters__ {
    enum { Count = 0 };
    void Apply(bidi_t& bidi, int);
    void Apply(bidi_t& bidi, int) const;
  };

  using Parameters = Parameters__ ;

  static R invoke(ClassType& object, FunctionType f, const Parameters&);
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

    void Apply(bidi_t& bidi, int parameter_index);
    void Apply(bidi_t& bidi, int parameter_index) const;
  };

  using Parameters = Parameters__ ;

  static R invoke(ClassType& object, FunctionType f, const Parameters& params);
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename ClassType,
    typename P0>
    void SetParameters(ReturnType (ClassType::*)(P0) const, void* param_data, P0 a0);

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

    void Apply(bidi_t& bidi, int parameter_index);
    void Apply(bidi_t& bidi, int parameter_index) const;
  };

  using Parameters = Parameters__ ;

  static R invoke(ClassType& object, FunctionType f, const Parameters& params);
};

///////////////////////////////////////////////////////////////////////////////

template <
    typename ReturnType, //
    typename ClassType,
    typename P0,
    typename P1>
    void SetParameters(ReturnType (ClassType::*)(P0, P1) const, void* param_data, P0 a0, P1 a1);

///////////////////////////////////////////////////////////////////////////////
*/
} // namespace ork::reflect
