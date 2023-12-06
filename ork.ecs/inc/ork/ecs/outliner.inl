////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/ecs/entity.h>
#include <ork/ecs/archetype.h>

using namespace ork::lev2::editor;

namespace ork::ecs {

void Outliner(
    const EditorContext& editorctx, //
    EcsEditor& ecseditor) {

  auto scene = ecseditor._scene;

  auto& varmap = editorctx._varmap;

  ImGui::Begin("Outliner");

  if (ImGui::TreeNode("SceneObjects")) {

    ///////////////////////////////////////////
    // split scene objects into spawndatas and archetypes
    ///////////////////////////////////////////

    std::vector<ork::ecs::spawndata_ptr_t> spawndatas;
    std::vector<ork::ecs::archetype_ptr_t> archetypes;

    for (auto item : scene->_sceneObjects) {
      auto so = item.second;
      if (auto as_arch = std::dynamic_pointer_cast<ork::ecs::Archetype>(so)) {
        archetypes.push_back(as_arch);
      } else if (auto as_ent = std::dynamic_pointer_cast<ork::ecs::SpawnData>(so)) {
        spawndatas.push_back(as_ent);
      }
    }

    ///////////////////////////////////////////
    // render / selection
    ///////////////////////////////////////////

    static int selected = -1;
    int j               = 0;

    // Spawners

    if (ImGui::TreeNodeEx("Spawners", ImGuiTreeNodeFlags_DefaultOpen)) {

      for (int i = 0; i < spawndatas.size(); i++) {
        auto e    = spawndatas[i];
        auto name = e->GetName().c_str();
        ImGui::PushID(j);
        if (ImGui::Selectable(name, (j == selected))) {
          ecseditor._currentobject = e;
          selected=j;
        }
        j++;
        ImGui::PopID();
      }

      ImGui::TreePop();
    }

    // Archetypes/ComponentDatas

    if (ImGui::TreeNodeEx("Archetypes", ImGuiTreeNodeFlags_DefaultOpen)) {
      
      for (int i = 0; i < archetypes.size(); i++) {
        auto a    = archetypes[i];
        auto name = a->GetName().c_str();
        ImGui::PushID(j);
        if (ImGui::Selectable(name, (j == selected))) {
          ecseditor._currentobject = a;
          selected=j;
        }
        ImGui::PopID();
        j++;

        if (ImGui::TreeNodeEx("Components", ImGuiTreeNodeFlags_DefaultOpen)) {

          for (auto citem : a->componentdata()) {
            auto cdata       = citem.second;
            auto as_obj      = std::dynamic_pointer_cast<const ork::Object>(cdata);
            auto as_nonconst = std::const_pointer_cast<ork::Object>(as_obj);

            auto clazz     = as_nonconst->objectClass();
            auto clazzname = clazz->Name();

		        ImGui::PushID(j);
            if (ImGui::Selectable(clazzname.c_str(), (j == selected))) {
              ecseditor._currentobject = as_nonconst;
		          selected=j;
            }
		        ImGui::PopID();
            j++;
          }
          ImGui::TreePop();
        }
      } // for( int i=0; i<archetypes.size(); i++ ){
      ImGui::TreePop();
    } // if (ImGui::TreeNodeEx("Archetypes",ImGuiTreeNodeFlags_DefaultOpen))

    ImGui::TreePop();
  }
  ImGui::End();
}

} // namespace ork::ecs
