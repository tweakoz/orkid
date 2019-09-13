#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/glheaders.h> // todo abstract somehow ?
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/openvr/openvr.h>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vr {
////////////////////////////////////////////////////////////////////////////////

Device::Device()
    : _width(1024)
    , _height(1024)
    , _active(false)
    , _hmdinputgroup(*lev2::InputManager::inputGroup("hmd")) {

  auto handgroup = lev2::InputManager::inputGroup("hands");
  handgroup->setChannel("left.button1").as<bool>(false);
  handgroup->setChannel("left.button2").as<bool>(false);
  handgroup->setChannel("left.trigger").as<bool>(false);
  handgroup->setChannel("left.thumb").as<bool>(false);

  handgroup->setChannel("right.button1").as<bool>(false);
  handgroup->setChannel("right.button2").as<bool>(false);
  handgroup->setChannel("right.trigger").as<bool>(false);
  handgroup->setChannel("right.thumb").as<bool>(false);
}

////////////////////////////////////////////////////////////////////////////////

Device::~Device() {}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vr
////////////////////////////////////////////////////////////////////////////////
