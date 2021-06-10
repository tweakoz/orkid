////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#include "harness.h"
#include <boost/program_options.hpp>
#include <iostream>
#include <ork/lev2/aud/singularity/hud.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorForward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/ui/context.h>
///////////////////////////////////////////////////////////////////////////////
namespace po = boost::program_options;
///////////////////////////////////////////////////////////////////////////////
#if defined(__APPLE__)
namespace ork::lev2 {
extern bool _macosUseHIDPI;
extern int G_MSAASAMPLES;
} // namespace ork::lev2
#endif
