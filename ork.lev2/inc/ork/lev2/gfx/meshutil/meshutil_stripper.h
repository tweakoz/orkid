////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/meshutil/tristripper/tri_stripper.h>

namespace ork { namespace meshutil {

struct TriStripperPrimGroup {
  orkvector<unsigned int> mIndices;
};

class TriStripper {
  triangle_stripper::tri_stripper tristripper;
  orkvector<TriStripperPrimGroup> mStripGroups;
  TriStripperPrimGroup mTriGroup;

public:
  TriStripper(const std::vector<unsigned int>& InTriIndices, int icachesize, int iminstripsize);

  const orkvector<TriStripperPrimGroup>& GetStripGroups(void) const {
    return mStripGroups;
  }

  const orkvector<unsigned int>& GetStripIndices(int igroup) const {
    return mStripGroups[igroup].mIndices;
  }

  const orkvector<unsigned int>& GetTriIndices(void) const {
    return mTriGroup.mIndices;
  }
};

}} // namespace ork::meshutil
