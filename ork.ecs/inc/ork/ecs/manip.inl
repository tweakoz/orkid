////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/imgui/ImGuizmo.h>

bool EditTransform(const ork::lev2::CameraData& camera, ork::fmtx4& matrix) {
  static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
  static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
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
  float matrixTranslation[3], matrixRotation[3], matrixScale[3];
  ImGuizmo::DecomposeMatrixToComponents(matrix.asArray(), matrixTranslation, matrixRotation, matrixScale);
  ImGui::InputFloat3("Tr", matrixTranslation);
  ImGui::InputFloat3("Rt", matrixRotation);
  ImGui::InputFloat3("Sc", matrixScale);
  ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix.asArray());

  if (mCurrentGizmoOperation != ImGuizmo::SCALE) {
    if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
      mCurrentGizmoMode = ImGuizmo::LOCAL;
    ImGui::SameLine();
    if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
      mCurrentGizmoMode = ImGuizmo::WORLD;
  }
  static bool useSnap(false);
  if (ImGui::IsKeyPressed(83))
    useSnap = !useSnap;
  ImGui::Checkbox("", &useSnap);
  ImGui::SameLine();
  ork::fvec3 snap;
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

  ImGuiIO& io = ImGui::GetIO();
  ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

  float aspect = float(io.DisplaySize.x) / float(io.DisplaySize.y);

  auto cammtx = camera.computeMatrices(aspect);

  ImGuizmo::Manipulate(
      cammtx._vmatrix.asArray(), //
      cammtx._pmatrix.asArray(), //
      mCurrentGizmoOperation,    //
      mCurrentGizmoMode,         //
      matrix.asArray(),
      NULL, //
      useSnap ? &snap.x : NULL);

  return ImGuizmo::IsOver();
}