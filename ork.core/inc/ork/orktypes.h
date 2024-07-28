////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkconfig.h>
#include <stdint.h>
#include <functional>
#include <memory>

typedef double f64;
typedef double F64;
typedef float f32;
typedef float F32;
typedef int32_t FX32, fx32;
typedef char* STRING;
typedef unsigned char* ADDRESS;

namespace ork {

template <typename T>      //
struct use_custom_serdes { //
  static constexpr bool enable = false;
};

template <typename T> typename std::enable_if<std::is_enum<T>::value, void>::type bar2(T t) {
}

typedef std::function<void()> void_lambda_t;

template <typename T> T minimum(T a, T b) {
  return (a < b) ? a : b;
}
template <typename T> T maximum(T a, T b) {
  return (a > b) ? a : b;
}

} // namespace ork

static const int fx32_SHIFT = 12;
static const int fx64_SHIFT = 24;

#include <stdint.h>
typedef uint32_t u32, U32;
typedef int32_t s32, S32, LONG;
typedef uint16_t u16, U16;
typedef int16_t s16, S16, SHORT;
typedef uint8_t u8, U8;
typedef int8_t s8, S8;
typedef int32_t FIX32;
typedef uint64_t u64, U64;
typedef int64_t s64, S64;
typedef int64_t fx64, FX64;

#include <ork/math/cfloat.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

struct StringPoolContext;

///////////////////////////////////////////////////////////////////////////////

struct const_string {
  const char* mpstr;

  const_string(const char* pstr = 0)
      : mpstr(pstr) {
  }

  bool operator==(const const_string& other) const;

  const char* c_str() const {
    return mpstr;
  }
};

///////////////////////////////////////////////////////////////////////////////
namespace object {
struct ObjectClass;
using class_ptr_t = ObjectClass*;
} // namespace object
namespace rtti {
struct ICastable;
using castable_ptr_t         = std::shared_ptr<ICastable>;
using castable_constptr_t    = std::shared_ptr<const ICastable>;
using castable_rawptr_t      = ICastable*;
using castable_rawconstptr_t = const ICastable*;

class Category;

} // namespace rtti

struct Object;
using object_ptr_t         = std::shared_ptr<Object>;
using object_constptr_t    = std::shared_ptr<const Object>;
using object_rawptr_t      = Object*;
using object_rawconstptr_t = const Object*;

///////////////////////////////////////////////////////////////////////////////

namespace asset {
struct Asset;
struct AssetSet;
struct AssetLoader;

using assetset_ptr_t   = std::shared_ptr<AssetSet>;
using asset_ptr_t      = std::shared_ptr<Asset>;
using asset_constptr_t = std::shared_ptr<const Asset>;
using loader_ptr_t     = std::shared_ptr<AssetLoader>;

} // namespace asset

///////////////////////////////////////////////////////////////////////////////

using FileH      = size_t;
using FileStampH = size_t; // (Y6M4D5:H5M6S6) (15:17) Base Year 2000 6 bits for year goes to 2063
using LibraryH   = size_t;
using FunctionH  = size_t;


class File;
class FileDev;
class FileProgressWatcher;

using file_ptr_t = std::shared_ptr<File>;
using filedev_ptr_t = std::shared_ptr<FileDev>;
using fileprogresswatcher_ptr_t = std::shared_ptr<FileProgressWatcher>;

struct Future;
using future_ptr_t = std::shared_ptr<Future>;

} // namespace ork

///////////////////////////////////////////////////////////////////////////////

constexpr double PI                = 3.14159265; //std::numbers::pi;
constexpr double PI2               = PI * 2.0;
constexpr double PI1               = PI;
constexpr double PI_DIV_2          = (PI / 2.0);
constexpr double PI_DIV_3          = (PI / 3.0);
constexpr double PI_DIV_4          = (PI / 4.0);
constexpr double PI_DIV_5          = (PI / 5.0);
constexpr double PI_DIV_6          = (PI / 6.0);
constexpr double NEG_PI_DIV_2      = -PI_DIV_2;
constexpr double INV_TWO_PI        = (1.0 / PI2);
constexpr double THREE_PI_OVER_TWO = (PI * 1.5);

constexpr double DTOR    = (PI / 180.0); // convert degrees to radians
constexpr double RTOD    = (180.0 / PI); // convert radians to degrees
constexpr double EPSILON = 0.0001;

///////////////////////////////////////////////////////////////////////////////
// C++ Only Types
///////////////////////////////////////////////////////////////////////////////

inline bool AreBitsEnabled(U32 uval, U32 bittest) {
  return (bittest == (uval & bittest));
}

struct SRect {
  int miX, miY, miX2, miY2, miW, miH;

  SRect(int x = 0, int y = 0, int x2 = 0, int y2 = 0)
      : miX(x)
      , miY(y)
      , miX2(x2)
      , miY2(y2)
      , miW(x2 - x)
      , miH(y2 - y) {
  }
};

using stringpoolctx_ptr_t = std::shared_ptr<ork::StringPoolContext>;
