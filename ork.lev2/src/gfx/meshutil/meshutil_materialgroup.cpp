////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/file/chunkfile.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/meshutil/clusterizer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

void MaterialGroup::ComputeVtxStreamFormat() {
  meVtxFormat = lev2::EVtxStreamFormat::V12N12B12T16;
}

///////////////////////////////////////////////////////////////////////////////

void MaterialGroup::Parse(const MaterialInfo& colmat) {
  meMaterialClass = EMATCLASS_STANDARD;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
