////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

IRenderable::IRenderable()
    : _modColor(fcolor4::White())
    , _drawDataA(nullptr)
    , _drawDataB(nullptr) {
}
IRenderable::~IRenderable() { // virtual
}
uint32_t IRenderable::ComposeSortKey(const IRenderer* renderer) const { // virtual
  return 0;
}

void IRenderable::setObject(const ork::Object* o) {
  _pickID.set<const ork::Object*>(o);
}
const ork::Object* IRenderable::getObject() const {
  return _pickID.get<const ork::Object*>();
}

const fcolor4& IRenderable::GetModColor() const {
  return _modColor;
}
void IRenderable::SetModColor(const fcolor4& Color) {
  _modColor = Color;
}

void IRenderable::SetMatrix(const fmtx4& mtx) {
  _worldMatrix = mtx;
}
const fmtx4& IRenderable::GetMatrix() const {
  return _worldMatrix;
}

void IRenderable::SetDrawableDataA(const IRenderable::var_t& ap) {
  _drawDataA = ap;
}
const IRenderable::var_t& IRenderable::GetDrawableDataA() const {
  return _drawDataA;
}
void IRenderable::SetDrawableDataB(const IRenderable::var_t& ap) {
  _drawDataB = ap;
}
const IRenderable::var_t& IRenderable::GetDrawableDataB() const {
  return _drawDataB;
}

///////////////////////////////////////////////////////////////////////////////

matrix_lamda_t IRenderable::genMatrixLambda() const {
  return [this]()->fmtx4{ return _worldMatrix; };
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
