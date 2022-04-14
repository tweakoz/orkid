////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/properties/registerX.inl>
#include <ork/object/ObjectClass.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/varmap.inl>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/string_generator.hpp>
#include <ork/application/application.h>

#include <ork/math/TransformNode.h>

#include <ork/lev2/imgui/imgui.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
#include <ork/lev2/imgui/imgui_impl_opengl3.h>
#include <ork/lev2/imgui/ImGuizmo.h>

namespace ork::editor {

struct EditorContext {

  EditorContext(const lev2::AcquiredRenderDrawBuffer& rdb)
      : _rdb(rdb) {
  }

  inline lev2::CameraData* getCamera() {
    return (lev2::CameraData*)_rdb._DB->cameraData("spawncam"_pool);
  }

  const lev2::AcquiredRenderDrawBuffer& _rdb;

  mutable int _depth = -1;
  mutable varmap::VarMap _varmap;
};

using prophandler_t = std::function<void(
    const EditorContext& edctx, //
    object_ptr_t obj,           //
    const reflect::ObjectProperty* prop)>;

}; // namespace ork::editor

namespace ork::editor::imgui {

inline void ObjectEditor(const EditorContext& ctx, object_ptr_t obj);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"

////////////////////////////////////////////////////////////////////////////////

struct InputTextCallback_UserData {
  std::string* Str;
  ImGuiInputTextCallback ChainCallback;
  void* ChainCallbackUserData;
};

inline int InputTextCallback(ImGuiInputTextCallbackData* data) {
  InputTextCallback_UserData* user_data = (InputTextCallback_UserData*)data->UserData;
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
    // Resize string callback
    // If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back to what we want.
    std::string* str = user_data->Str;
    IM_ASSERT(data->Buf == str->c_str());
    str->resize(data->BufTextLen);
    data->Buf = (char*)str->c_str();
  } else if (user_data->ChainCallback) {
    // Forward to user callback, if any
    data->UserData = user_data->ChainCallbackUserData;
    return user_data->ChainCallback(data);
  }
  return 0;
}

inline bool InputStdString(
    const char* label,
    std::string* str,
    ImGuiInputTextFlags flags       = 0,
    ImGuiInputTextCallback callback = NULL,
    void* user_data                 = NULL) {
  IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
  flags |= ImGuiInputTextFlags_CallbackResize;

  InputTextCallback_UserData cb_user_data;
  cb_user_data.Str                   = str;
  cb_user_data.ChainCallback         = callback;
  cb_user_data.ChainCallbackUserData = user_data;
  return ImGui::InputText(label, (char*)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
}

////////////////////////////////////////////////////////////////////////////////

inline void DirectTypedProp_float(object_ptr_t obj, const reflect::DirectTyped<float>* prop) {
  auto name = prop->_name;
  auto memb = prop->_member;

  float& the_float = (obj.get()->*memb);

  float range_min = 0.0;
  float range_max = 1.0;

  auto anno_min = prop->annotation("editor.range.min");
  auto anno_max = prop->annotation("editor.range.max");
  auto anno_rng = prop->annotation("editor.range");

  if (auto as_f = anno_min.tryAs<float>()) {
    range_min = as_f.value();
  }
  if (auto as_f = anno_max.tryAs<float>()) {
    range_max = as_f.value();
  }
  if (auto as_f = anno_rng.tryAs<float_range>()) {
    range_min = as_f.value()._min;
    range_max = as_f.value()._max;
  }

  float power = 1.0f;

  ImGui::SliderFloat(name.c_str(), &the_float, range_min, range_max, "%.3f", power);
}

////////////////////////////////////////////////////////////////////////////////

inline void DirectTypedProp_int(object_ptr_t obj, const reflect::DirectTyped<int>* prop) {
  auto name = prop->_name;
  auto memb = prop->_member;

  int& the_int = (obj.get()->*memb);

  int range_min = 0;
  int range_max = 1;

  auto anno_min = prop->annotation("editor.range.min");
  auto anno_max = prop->annotation("editor.range.max");

  if (auto as_f = anno_min.tryAs<int>()) {
    range_min = as_f.value();
  }
  if (auto as_f = anno_max.tryAs<int>()) {
    range_max = as_f.value();
  }

  ImGui::SliderInt(name.c_str(), &the_int, range_min, range_max);
}

////////////////////////////////////////////////////////////////////////////////

inline void DirectTypedProp_bool(object_ptr_t obj, const reflect::DirectTyped<bool>* prop) {
  auto name      = prop->_name;
  auto memb      = prop->_member;
  bool& the_bool = (obj.get()->*memb);
  ImGui::PushItemWidth(-1);
  // int total_w = ImGui::GetContentRegionAvail().x;
  // ImGui::Text(name.c_str());
  // ImGui::SameLine(total_w);
  // ImGui::SetNextItemWidth(total_w);
  // ImGui::SomeWidget().
  // ImGui::Checkbox(nullptr,&the_bool);
  ImGui::Checkbox(name.c_str(), &the_bool);
  // ImGui::CheckboxFlags(name.c_str(), &the_bool, ImGuiCheckboxFlags_AlignRight);
  ImGui::PopItemWidth();
}

////////////////////////////////////////////////////////////////////////////////
// enumerate instantiable classes
////////////////////////////////////////////////////////////////////////////////

struct FactoryItem {
  object::ObjectClass* _clazz = nullptr;
  rtti::shared_factory_t _factory;
};
inline void buildFactoryList(rtti::Class* root, std::vector<FactoryItem>& factories) {
  if (root->hasSharedFactory()) {
    auto pobjclass = dynamic_cast<object::ObjectClass*>(root);
    auto instanno  = pobjclass->Description().classAnnotation("editor.instantiable");

    bool bok2add = instanno.isA<bool>() ? instanno.get<bool>() : true;

    if (bok2add) {
      FactoryItem item;
      item._clazz   = pobjclass;
      item._factory = pobjclass->sharedFactory();
      factories.push_back(item);
    }
  }

  rtti::Class* child = root->FirstChild();
  if (child)
    do {
      buildFactoryList(child, factories);
      child = child->NextSibling();
    } while (child != root->FirstChild());
}

////////////////////////////////////////////////////////////////////////////////

inline void DirectTypedProp_obj(
    const EditorContext& edctx, //
    object_ptr_t obj,           //
    const reflect::DirectTyped<object_ptr_t>* prop) {
  auto name = prop->_name;
  auto memb = prop->_member;

  if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {

    auto anno_factorybase = prop->annotation("editor.factory.classbase");

    if (auto as_fbase = anno_factorybase.tryAs<object::class_ptr_t>()) {
      auto objectclass = as_fbase.value();
      if (objectclass) {

        std::vector<FactoryItem> factories;
        buildFactoryList(objectclass, factories);

        if (factories.size()) {

          auto buttonstr = FormatString("FACTORY(%s)", name.c_str());

          if (ImGui::BeginPopup("my_factory_popup")) {

            for (auto f : factories) {

              auto c  = f._clazz;
              auto& n = c->Name();

              if (ImGui::Selectable(n.c_str())) {

                //////////////////////////////////////
                // todo: wrap for update thread safety
                //////////////////////////////////////

                auto op = [=]() {
                  opq::updateSerialQueue()->invokeHook("ged-pre-lockgfx", obj);
                  lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
                  object_ptr_t& the_subobject = (obj.get()->*memb);
                  auto factory                = f._factory;
                  auto new_object             = factory();
                  opq::updateSerialQueue()->invokeHook("ged-pre-edit", obj);
                  the_subobject = dynamic_pointer_cast<Object>(new_object);
                  opq::updateSerialQueue()->invokeHook("ged-post-edit", obj);
                  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
                  opq::updateSerialQueue()->invokeHook("ged-post-unlockgfx", obj);
                };

                opq::updateSerialQueue()->enqueue(op);
              }
            }

            ImGui::EndPopup();
          }

          if (ImGui::Button(buttonstr.c_str())) {

            ImGui::OpenPopup("my_factory_popup");
          }
        }
      }
    }

    object_ptr_t& the_subobject = (obj.get()->*memb);
    ObjectEditor(edctx, the_subobject);

    ImGui::TreePop();
  }
}

////////////////////////////////////////////////////////////////////////////////

inline void DirectTypedProp_objb(
    const EditorContext& edctx, //
    object_ptr_t obj,           //
    const reflect::DirectObjectBase* prop) {

  auto name = prop->_name;
  // auto memb = prop->_member;

  if (ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {

    auto anno_factorybase = prop->annotation("editor.factory.classbase");

    if (auto as_fbase = anno_factorybase.tryAs<object::class_ptr_t>()) {
      auto objectclass = as_fbase.value();
      if (objectclass) {

        std::vector<FactoryItem> factories;
        buildFactoryList(objectclass, factories);

        if (factories.size()) {

          auto buttonstr = FormatString("FACTORY(%s)", name.c_str());

          if (ImGui::BeginPopup("my_factory_popup")) {

            for (auto f : factories) {

              auto c  = f._clazz;
              auto& n = c->Name();

              if (ImGui::Selectable(n.c_str())) {

                //////////////////////////////////////
                // todo: wrap for update thread safety
                //////////////////////////////////////

                auto op = [=]() {
                  opq::updateSerialQueue()->invokeHook("ged-pre-lockgfx", obj);
                  lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
                  object_ptr_t the_subobject = prop->getObject(obj);
                  auto factory               = f._factory;
                  auto new_object            = factory();
                  opq::updateSerialQueue()->invokeHook("ged-pre-edit", obj);
                  the_subobject = dynamic_pointer_cast<Object>(new_object);
                  opq::updateSerialQueue()->invokeHook("ged-post-edit", obj);
                  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
                  opq::updateSerialQueue()->invokeHook("ged-post-unlockgfx", obj);
                };

                opq::updateSerialQueue()->enqueue(op);
              }
            }

            ImGui::EndPopup();
          }

          if (ImGui::Button(buttonstr.c_str())) {

            ImGui::OpenPopup("my_factory_popup");
          }
        }
      }
    }

    object_ptr_t the_subobject = prop->getObject(obj);
    ObjectEditor(edctx, the_subobject);

    ImGui::TreePop();
  }
}

////////////////////////////////////////////////////////////////////////////////

inline void DirectTypedProp_str(object_ptr_t obj, const reflect::DirectTyped<std::string>* prop) {
  auto name = prop->_name;
  auto memb = prop->_member;

  std::string& the_string = (obj.get()->*memb);

  InputStdString(name.c_str(), &the_string);
}

////////////////////////////////////////////////////////////////////////////////

inline void DirectTypedProp_fvec2(object_ptr_t obj, const reflect::DirectTyped<fvec2>* prop) {
  auto name = prop->_name;
  auto memb = prop->_member;

  fvec2& the_fvec2 = (obj.get()->*memb);

  ImGui::InputFloat2(name.c_str(), the_fvec2.asArray());
}

////////////////////////////////////////////////////////////////////////////////

inline void DirectTypedProp_fvec3(object_ptr_t obj, const reflect::DirectTyped<fvec3>* prop) {
  auto name = prop->_name;
  auto memb = prop->_member;

  fvec3& the_fvec3 = (obj.get()->*memb);

  auto anno_edtype = prop->annotation("editor.type");

  bool use_color = false;

  if (auto as_str = anno_edtype.tryAs<ConstString>()) {
    auto type = as_str.value();
    if (type == "color") {
      use_color = true;
    }
  }

  if (use_color)
    ImGui::ColorEdit3(name.c_str(), the_fvec3.asArray());
  else
    ImGui::InputFloat3(name.c_str(), the_fvec3.asArray());
}

////////////////////////////////////////////////////////////////////////////////

inline void DirectTypedProp_fvec4(object_ptr_t obj, const reflect::DirectTyped<fvec4>* prop) {
  auto name = prop->_name;
  auto memb = prop->_member;

  fvec4& the_fvec4 = (obj.get()->*memb);

  auto anno_edtype = prop->annotation("editor.type");

  bool use_color = false;

  if (auto as_str = anno_edtype.tryAs<ConstString>()) {
    auto type = as_str.value();
    if (type == "color") {
      use_color = true;
    }
  }

  if (use_color)
    ImGui::ColorEdit4(name.c_str(), the_fvec4.asArray());
  else
    ImGui::InputFloat4(name.c_str(), the_fvec4.asArray());
}

///////////////////////////////////////////////////////////////////////////////

template <typename MapType> //
inline void DirectObjectMapPropUI(
    const EditorContext& edctx, //
    object_ptr_t obj,           //
    const reflect::ObjectProperty* prop) {
  auto typed_prop     = (const reflect::DirectObjectMap<MapType>*)prop;
  auto name           = typed_prop->_name;
  auto memb           = typed_prop->_member;
  const auto& the_map = (obj.get()->*memb);
  for (auto item : the_map) {
    const auto& key = item.first;
    auto val        = dynamic_pointer_cast<ork::Object>(item.second);
    if (ImGui::TreeNodeEx(key.c_str()), ImGuiTreeNodeFlags_DefaultOpen) {
      ObjectEditor(edctx, val);
      ImGui::TreePop();
    }
  }
}

inline void DirectTransformPropUI( //
    const EditorContext& edctx,    //
    xfnode_ptr_t xfn) {

  using namespace ork;

  edctx._varmap.makeValueForKey<xfnode_ptr_t>("manip_target") = xfn;

  decompxf_ptr_t the_transform = xfn->_transform;

  static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
  static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
  static bool useSnap(false);
  static ork::fvec3 snap;

  edctx._varmap.makeValueForKey<ImGuizmo::OPERATION>("manip.op") = mCurrentGizmoOperation;
  edctx._varmap.makeValueForKey<ImGuizmo::MODE>("manip.mode")    = mCurrentGizmoMode;
  edctx._varmap.makeValueForKey<bool>("manip.use_snap")          = useSnap;
  edctx._varmap.makeValueForKey<ork::fvec3>("manip.snapval")     = snap;

  if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {

    if (ImGui::IsKeyPressed(90))
      mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(69))
      mCurrentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(82)) // r Key
      mCurrentGizmoOperation = ImGuizmo::SCALE;
    if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
      mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
      mCurrentGizmoOperation = ImGuizmo::ROTATE;
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
      mCurrentGizmoOperation = ImGuizmo::SCALE;
    ImGui::InputFloat3("Tr", the_transform->_translation.asArray());
    ImGui::InputFloat4("Rt", the_transform->_rotation.asArray());
    ImGui::InputFloat("Sc", &the_transform->_uniformScale);
    if (mCurrentGizmoOperation != ImGuizmo::SCALE) {
      if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
        mCurrentGizmoMode = ImGuizmo::LOCAL;
      ImGui::SameLine();
      if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
        mCurrentGizmoMode = ImGuizmo::WORLD;
    }
    if (ImGui::IsKeyPressed(83))
      useSnap = !useSnap;
    ImGui::Checkbox("", &useSnap);
    ImGui::SameLine();
    static ork::fvec3 config_snaptrans = false;
    static ork::fvec3 config_snaprot   = false;
    static ork::fvec3 config_snapscale = false;
    switch (mCurrentGizmoOperation) {
      case ImGuizmo::TRANSLATE:
        snap = config_snaptrans;
        ImGui::InputFloat3("Snap", &snap.x);
        break;
      case ImGuizmo::ROTATE:
        snap = config_snaprot;
        ImGui::InputFloat("Angle Snap", &snap.x);
        break;
      case ImGuizmo::SCALE:
        snap = config_snapscale;
        ImGui::InputFloat("Scale Snap", &snap.x);
        break;
      default:
        break;
    }
    ImGui::TreePop();
  }
}

////////////////////////////////////////////////////////////////////////////////

inline void VisitClass(const EditorContext& edctx, object_ptr_t obj, object::ObjectClass* clazz) {

  edctx._depth++;

  auto clazzname = clazz->Name();

  const auto& desc  = clazz->Description();
  const auto& props = desc.properties();

  ///////////////////////////////////////////////////////////////////////

  auto visit_property = [&](const reflect::ObjectProperty* prop) {
    auto as_direct_int   = dynamic_cast<const reflect::DirectTyped<int>*>(prop);
    auto as_direct_float = dynamic_cast<const reflect::DirectTyped<float>*>(prop);
    auto as_direct_fvec2 = dynamic_cast<const reflect::DirectTyped<fvec2>*>(prop);
    auto as_direct_fvec3 = dynamic_cast<const reflect::DirectTyped<fvec3>*>(prop);
    auto as_direct_fvec4 = dynamic_cast<const reflect::DirectTyped<fvec4>*>(prop);
    auto as_direct_obj   = dynamic_cast<const reflect::DirectTyped<object_ptr_t>*>(prop);
    auto as_direct_objb  = dynamic_cast<const reflect::DirectObjectBase*>(prop);
    auto as_direct_str   = dynamic_cast<const reflect::DirectTyped<std::string>*>(prop);
    auto as_direct_bool  = dynamic_cast<const reflect::DirectTyped<bool>*>(prop);

    auto anno_handler = prop->annotation("editor.prop.handler");

    if (auto as_cb = anno_handler.tryAs<prophandler_t>()) {
      auto cb = as_cb.value();
      cb(edctx, obj, prop);
    } else if (as_direct_bool)
      DirectTypedProp_bool(obj, as_direct_bool);
    else if (as_direct_int)
      DirectTypedProp_int(obj, as_direct_int);
    else if (as_direct_float)
      DirectTypedProp_float(obj, as_direct_float);
    else if (as_direct_fvec2)
      DirectTypedProp_fvec2(obj, as_direct_fvec2);
    else if (as_direct_fvec3)
      DirectTypedProp_fvec3(obj, as_direct_fvec3);
    else if (as_direct_fvec4)
      DirectTypedProp_fvec4(obj, as_direct_fvec4);
    else if (as_direct_obj)
      DirectTypedProp_obj(edctx, obj, as_direct_obj);
    else if (as_direct_objb)
      DirectTypedProp_objb(edctx, obj, as_direct_objb);
    else if (as_direct_str)
      DirectTypedProp_str(obj, as_direct_str);
  };

  ///////////////////////////////////////////////////////////////////////

  auto str = FormatString("Object: class<%s>", clazzname.c_str());
  if (edctx._depth > 0) {
    str = FormatString("class<%s>", clazzname.c_str());
  }

  if (ImGui::TreeNodeEx(str.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {

    auto uuid         = obj->_uuid;
    std::string uuids = boost::uuids::to_string(uuid);

    if (edctx._depth == 0) {
      str = FormatString("UUID: %s", uuids.c_str());
      ImGui::Text(str.c_str());
    }

    auto anno_groups = clazz->annotation("editor.groups");

    if (auto as_g = anno_groups.tryAs<reflect::group_list_ptr_t>()) {

      auto grplist = as_g.value();

      const auto& groups = grplist->_groups;

      for (auto g : groups) {
        std::string g_name  = g._name;
        std::string g_plist = g._proplist;

        if (ImGui::TreeNodeEx(g_name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {

          auto split = SplitString(g_plist, ' ');

          for (auto pname : split) {
            auto it = props.find(pname.c_str());
            if (it != props.end()) {
              const reflect::ObjectProperty* prop = it->second;
              visit_property(prop);
            }
          }

          ImGui::TreePop();
        }
      }
    } else {
      for (auto item : props) {
        ConstString name                    = item.first;
        const reflect::ObjectProperty* prop = item.second;
        visit_property(prop);
      }
    }

    auto parclazz = (object::ObjectClass*)clazz->Parent();
    if (parclazz) {
      const auto& par_desc  = parclazz->Description();
      const auto& par_props = par_desc.properties();
      if (par_props.size()) {
        VisitClass(edctx, obj, parclazz);
      }
    }

    ImGui::TreePop();
  }

  edctx._depth--;
}

////////////////////////////////////////////////////////////////////////////////

inline void ObjectEditor(const EditorContext& edctx, object_ptr_t obj) {
  ImGui::Begin("ObjectEditor");
  if (obj) {
    VisitClass(edctx, obj, obj->objectClass());
  }
  ImGui::End();
};

#pragma GCC diagnostic pop

} // namespace ork::editor::imgui