////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/file/file.h>
///////////////////////////////////////////////////////////////////////////////
//#include <ork/kernel/serialize/serialize.h>
#include <ork/kernel/string/ConstString.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cmatrix3.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/orklut.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/prop.h>
#include <ork/util/Context.hpp>
#include <ork/kernel/any.h>
///////////////////////////////////////////////////////////////////////////////
#if !defined(NITRO) && !defined(WII)
//# include <ork/lev2/gfx/gfxmaterial_fx.h> // For lev2::GfxMaterialFxParamBase
#endif
///////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
namespace ork {
//////////////////////////////////////////////////////////////

template class orklut<ConstString,any4>;
//template class orklut<ConstString,anyp>;
template class orklut<ConstString,any16>;
template class orklut<ConstString,any64>;
template class orklut<ConstString,ConstString>;
template class orklut<int,int>;
template class orklut<u64,int>;
template class orklut<float,float>;
template class orklut<ork::PoolString,int>;
template class orklut<ork::PoolString,U32>;
template class orklut<ork::PoolString, bool>;
template class orklut<ork::PoolString,ork::PoolString>;
//template class orklut<ork::PoolString,ork::CObject*>;
//template class orklut<ork::PoolString,ork::CClass*>;
template class orklut<ork::PoolString,ork::Object*>;
template class orklut<Char4,ork::Object*>;
template class orklut<Char8,ork::Object*>;

template class orklut<u64,bool>;
template class orklut<std::string,int>;

#if defined( ORK_CONFIG_EDITORBUILD ) || defined(WII) || defined(_XBOX)
#if defined(WII)
//template class CMapContainerProp<int, int>;
#endif
//template class orklut<ork::PoolString,int>;
template class orklut<ork::PoolString,CMatrix4>;
template class orklut<std::string,std::string>;

template class orklut<ork::CReal, ork::PoolString>;
#endif

//////////////////////////////////////////////////////////////
namespace util {
template <> PropSetContext *Context<PropSetContext>::sCurrentContext = NULL;
template class Context<PropSetContext>;
}
//////////////////////////////////////////////////////////////
}
//////////////////////////////////////////////////////////////
