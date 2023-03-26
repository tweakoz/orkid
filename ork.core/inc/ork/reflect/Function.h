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

  static void invoke(std::shared_ptr<ClassType> object, //
                     FunctionType f, //
                     const Parameters&);
};

} // namespace ork::reflect
