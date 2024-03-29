////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/qtui/qtui.h>
#include <ork/object/Object.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>

#if defined(ORK_CONFIG_IX)
#include <X11/Xlib.h>
#endif

#include <ork/kernel/environment.h>
#include <ork/kernel/opq.h>

////////////////////////////////////////////////////////////////

using namespace ork;
using namespace ork::ent;

////////////////////////////////////////////////////////////////

namespace ork { namespace tool {
int main(int& argc, char** argv);
}} // namespace ork::tool
namespace ork { namespace lev2 {
void ClassInit();
}} // namespace ork::lev2
namespace ork { namespace ent {
void ClassInit();
void Init2();
}} // namespace ork::ent

////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////

namespace ork { namespace tool {
// tokenlist Init(int argc, char** argv);

void init(char** argp) {

  //////////////////////////
  // early init..
  //////////////////////////

  genviron.init_from_envp(argp);

  SetCurrentThreadName("MainThread");

#if defined(ORK_CONFIG_IX)
//	XInitThreads();
#endif

  ork::lev2::ClassInit();
  ork::ent::ClassInit();
  ork::rtti::Class::InitializeClasses();

  ork::lev2::ContextCreationParams CreationParams;
  CreationParams.miNumSharedVerts = 4 << 10;
  ork::lev2::GfxEnv::GetRef().PushCreationParams(CreationParams);

  auto mainthreadopq = ork::opq::mainSerialQueue();
  ork::opq::TrackCurrent ot(mainthreadopq);

  //////////////////////////
  // init
  //////////////////////////

  // return ork::tool::main(argc, argv);
}

}} // namespace ork::tool
