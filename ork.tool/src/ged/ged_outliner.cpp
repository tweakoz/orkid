////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/qtui/qtui.hpp>
#include "ged_delegate.hpp"
#include <pkg/ent/scene.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////
template class GedSimpleNode<GedIoDriver<PoolString>, PoolString>;
///////////////////////////////////////////////////////////////////////////////

class GedOutlinerWidget : public GedItemNode {
  static const int kh        = 48;
  static const int kpoolsize = 32;
  ork::ent::SceneData* mpSceneData;

  void DoDraw(lev2::Context* pTARG) // virtual
  {
    GetSkin()->DrawBgBox(this, miX, miY + 2, miW, kh - 3, GedSkin::ESTYLE_BACKGROUND_1);

    if (mpSceneData) {
      static const int knodesize = 16;

      const orkmap<PoolString, ent::SceneObject*>& sceneObjs = mpSceneData->GetSceneObjects();

      int iy = (miY + 2) + (kh - 3);

      for (orkmap<PoolString, ent::SceneObject*>::const_iterator it = sceneObjs.begin(); it != sceneObjs.end(); it++) {
        const PoolString& psname = it->first;
        const char* pccname      = psname.c_str();

        GetSkin()->DrawText(this, miX + 4, miY + iy, pccname);

        iy += knodesize;
      }
    }
  }

  int CalcHeight(void) {
    return kh;
  } // virtual

public:
  GedOutlinerWidget(ObjModel& mdl, const char* name, const reflect::ObjectProperty* prop, ork::Object* obj)
      : GedItemNode(mdl, name, prop, obj)
      , mpSceneData(0) {
    if (prop) {
      const reflect::IObject* pprop = rtti::autocast(GetOrkProp());
      mpSceneData                   = rtti::autocast(pprop->Access(GetOrkObj()));
    } else {
      mpSceneData = rtti::autocast(obj);
    }
  }
  bool DoDrawDefault() const // virtual
  {
    return false;
  }
};

void GedFactoryOutliner::Describe() {
}

GedItemNode* GedFactoryOutliner::CreateItemNode(
    ObjModel& mdl, //
    const ConstString& Name,
    const reflect::ObjectProperty* prop,
    Object* obj) const {
  GedItemNode* groupnode = new GedLabelNode(mdl, Name.c_str(), prop, obj);

  mdl.GetGedWidget()->PushItemNode(groupnode);

  GedItemNode* itemnode = new GedOutlinerWidget(mdl, Name.c_str(), prop, obj);

  mdl.GetGedWidget()->AddChild(itemnode);

  mdl.GetGedWidget()->PopItemNode(groupnode);

  return groupnode;
}

///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedFactoryOutliner, "ged.factory.outliner");
