#include <ork/util/hotkey.h>
#include <ork/math/multicurve.h>
#include <ork/asset/Asset.h>
#include <ork/dataflow/dataflow.h>

namespace ork {

void TouchCoreClasses() {
  HotKeyConfiguration::GetClassStatic();
  HotKey::GetClassStatic();
  MultiCurve1D::GetClassStatic();
  asset::Asset::GetClassStatic();
}

} // namespace ork
