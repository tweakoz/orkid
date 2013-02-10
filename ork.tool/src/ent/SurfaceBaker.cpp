////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/opq.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>
#include <ork/gfx/camera.h>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <pkg/ent/drawable.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <pkg/ent/Lighting.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <orktool/qtui/qtui_tool.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <ork/reflect/enum_serializer.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/meshutil/meshutil_fixedgrid.h>
#include <ork/math/audiomath.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/kernel/orkpool.h>
#include <ork/file/chunkfile.h>

#include "SurfaceBaker.h"
#include "SurfaceBaker.moc"
//

