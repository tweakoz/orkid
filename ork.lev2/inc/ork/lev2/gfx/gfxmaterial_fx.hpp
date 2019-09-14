////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _GFX_GFXMATERIAL_FX_HPP_
#define	_GFX_GFXMATERIAL_FX_HPP_

#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/rendercontext.h>

namespace ork {

namespace lev2 {

template <> GfxMaterialFxParamArtist<int>::GfxMaterialFxParamArtist(GfxMaterialFx *parent)
	: GfxMaterialFxParam<int>(parent)
	, mValue(0)
{
}

template <> GfxMaterialFxParamArtist<fmtx4>::GfxMaterialFxParamArtist(GfxMaterialFx *parent)
	: GfxMaterialFxParam<fmtx4>(parent)
	, mValue()
{
}

template <> GfxMaterialFxParamArtist<fmtx3>::GfxMaterialFxParamArtist(GfxMaterialFx *parent)
	: GfxMaterialFxParam<fmtx3>(parent)
	, mValue()
{
}

template <> GfxMaterialFxParamArtist<std::string>::GfxMaterialFxParamArtist(GfxMaterialFx *parent)
	: GfxMaterialFxParam<std::string>(parent)
	, mValue("")
{
}

template <typename T> GfxMaterialFxParamArtist<T>::GfxMaterialFxParamArtist(GfxMaterialFx *parent)
	: GfxMaterialFxParam<T>(parent)
	, mValue( 0 )
{
}

///////////////////////////////////////////////////////////////////////////////

} }

#endif
