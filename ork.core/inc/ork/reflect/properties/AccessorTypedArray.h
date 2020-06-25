////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/properties/ITypedArray.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename T> class AccessorTypedArray : public ITypedArray<T> {
public:
  AccessorTypedArray(
      void (Object::*getter)(T&, size_t) const,
      void (Object::*setter)(const T&, size_t),
      size_t (Object::*counter)() const,
      void (Object::*resizer)(size_t) = 0);

private:
  void Get(T&, const Object*, size_t) const override;
  void Set(const T&, Object*, size_t) const override;
  size_t Count(const Object*) const override;
  bool Resize(Object* obj, size_t size) const override;

  void (Object::*mGetter)(T&, size_t) const;
  void (Object::*mSetter)(const T&, size_t);
  size_t (Object::*mCounter)() const;
  void (Object::*mResizer)(size_t);
};

}} // namespace ork::reflect
