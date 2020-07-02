#include <ork/util/hotkey.h>
#include <ork/math/multicurve.h>

namespace ork {

void TouchCoreClasses() {
  HotKeyConfiguration::GetClassStatic();
  HotKey::GetClassStatic();
  MultiCurve1D::GetClassStatic();
}

} // namespace ork
