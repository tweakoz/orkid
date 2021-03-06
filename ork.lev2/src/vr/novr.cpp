#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/glheaders.h> // todo abstract somehow ?
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/vr/vr.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#if !defined(ENABLE_OPENVR)
////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orkidvr {
////////////////////////////////////////////////////////////////////////////////
static ork::LockedResource<VrTrackingNotificationReceiver_set> gnotifset;
void addVrTrackingNotificationReceiver(VrTrackingNotificationReceiver_ptr_t recvr) {
  gnotifset.atomicOp([&](VrTrackingNotificationReceiver_set& notifset) { notifset.insert(recvr); });
}
void removeVrTrackingNotificationReceiver(VrTrackingNotificationReceiver_ptr_t recvr) {
  gnotifset.atomicOp([&](VrTrackingNotificationReceiver_set& notifset) {
    auto it = notifset.find(recvr);
    OrkAssert(it != notifset.end());
    notifset.erase(it);
  });
}
////////////////////////////////////////////////////////////////////////////////
NoVrDevice::NoVrDevice() {
  auto handgroup = lev2::InputManager::instance()->inputGroup("hands");
  _qtmousesubsc  = msgrouter::channel("qtmousepos")->subscribe([this](msgrouter::content_t c) { _qtmousepos = c.Get<fvec2>(); });

  _active       = true;
  _qtkbdownsubs = msgrouter::channel("qtkeyboard.down")->subscribe([this, handgroup](msgrouter::content_t c) {
    int key = c.Get<int>();
    switch (key) {
      case 'w':
        handgroup->setChannel("left.trigger").as<bool>(true);
        break;
      case 'a':
        handgroup->setChannel("left.thumb").as<bool>(true);
        break;
      case 's':
        break;
      case 'd':
        handgroup->setChannel("right.thumb").as<bool>(true);
        break;
    }
  });
  _qtkbupsubs   = msgrouter::channel("qtkeyboard.up")->subscribe([this, handgroup](msgrouter::content_t c) {
    int key = c.Get<int>();
    switch (key) {
      case 'w':
        handgroup->setChannel("left.trigger").as<bool>(false);
        break;
      case 'a':
        handgroup->setChannel("left.thumb").as<bool>(false);
        break;
      case 's':
        break;
      case 'd':
        handgroup->setChannel("right.thumb").as<bool>(false);
        break;
    }
  });

  _posemap["projl"].Perspective(45, 16.0 / 9.0, .1, 100000);
  _posemap["projr"].Perspective(45, 16.0 / 9.0, .1, 100000);
  _posemap["projc"].Perspective(45, 16.0 / 9.0, .1, 100000);
  _posemap["eyel"] = fmtx4::Identity();
  _posemap["eyer"] = fmtx4::Identity();
}
NoVrDevice::~NoVrDevice() {
}
////////////////////////////////////////////////////////////////////////////////
void NoVrDevice::_updatePoses(RenderContextFrameData& RCFD) {
  auto mpos = _qtmousepos;
  float r   = mpos.Mag();
  float z   = 1.0f - r;
  auto v3   = fvec3(-mpos.x, -mpos.y, z).Normal();
  fmtx4 w;
  w.LookAt(fvec3(0, 0, 0), v3, fvec3(0, 1, 0));
  _posemap["hmd"] = w;
  // printf("v3<%g %g %g>\n", v3.x, v3.y, v3.z);
  auto& CPD    = RCFD.topCPD();
  auto rt      = CPD._irendertarget;
  float aspect = float(rt->width()) / float(rt->height());
  _posemap["projl"].Perspective(45, aspect, .1, 100000);
  _posemap["projr"].Perspective(45, aspect, .1, 100000);
  _posemap["projc"].Perspective(45, aspect, .1, 100000);
  _updatePosesCommon();
}
////////////////////////////////////////////////////////////////////////////////
void NoVrDevice::_processControllerEvents() {
  auto handgroup = lev2::InputManager::instance()->inputGroup("hands");
  bool curthumbL = handgroup->tryAs<bool>("left.thumb").value();
  bool curthumbR = handgroup->tryAs<bool>("right.thumb").value();
  ///////////////////////////////////////////////////////////
}
////////////////////////////////////////////////////////////////////////////////
NoVrDevice& concrete_get() {
  static NoVrDevice _device;
  return _device;
}
Device& device() {
  return concrete_get();
}
////////////////////////////////////////////////////////////////////////////////
void gpuUpdate(RenderContextFrameData& RCFD) {
  auto& mgr = concrete_get();
  mgr._processControllerEvents();
  mgr._updatePoses(RCFD);
}

void composite(Context* targ, Texture* twoeyetex) {
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orkidvr
////////////////////////////////////////////////////////////////////////////////
#endif // #if !defined(ENABLE_VR)
