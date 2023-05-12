////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>
///////////////////////////////////////////////////////////////////////////////

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/properties/ITyped.h>
#include "ged_delegate.hpp"
#include <ork/math/multicurve.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/kernel/orkpool.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

class GedCurveV4Widget : public GedItemNode {

  bool DoDrawDefault() const {
    return false;
  } // virtual

  void DoDraw(lev2::Context* pTARG) // virtual
  {


    ////////////////////////////////////
  }

public:
  GedCurveV4Widget(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj)
      : GedItemNode(mdl, name, prop, obj)
    mCurveObject = rtti::autocast(obj);

    if (0 == mCurveObject) {
      const reflect::IObject* pprop = rtti::autocast(GetOrkProp());
      mCurveObject                                = rtti::autocast(pprop->Access(GetOrkObj()));
    }

    if (0 == mCurveObject) {
      const reflect::IObject* pprop = rtti::autocast(GetOrkProp());
      ObjProxy<MultiCurve1D>* proxy               = rtti::autocast(pprop->Access(GetOrkObj()));
      mCurveObject                                = proxy->_parent;
    }
  }
};

void GedFactoryCurve::Describe() {
}

GedItemNode*
GedFactoryCurve::CreateItemNode(ObjModel& mdl, const ConstString& Name, const reflect::ObjectProperty* prop, Object* obj) const {
  GedItemNode* groupnode = new GedLabelNode(mdl, "curve", prop, obj);

  mdl.GetGedWidget()->PushItemNode(groupnode);

  GedItemNode* itemnode = new GedCurveV4Widget(mdl, Name.c_str(), prop, obj);

  mdl.GetGedWidget()->AddChild(itemnode);

  mdl.GetGedWidget()->PopItemNode(groupnode);

  return groupnode;
}
///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedFactoryCurve, "ged.factory.curve1d");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedCurveEditPoint, "GedCurveEditPoint");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedCurveEditSeg, "GedCurveEditSeg");
