////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

IRenderable::IRenderable()
    : _object(0)
    , _modColor(fcolor4::White())
    , _drawDataA(nullptr)
    , _drawDataB(nullptr) {
}
IRenderable::~IRenderable() { // virtual
}
bool IRenderable::CanGroup(const IRenderable* oth) const { // virtual
  return false;
}
uint32_t IRenderable::ComposeSortKey(const IRenderer* renderer) const { // virtual
  return 0;
}

void IRenderable::SetObject(const ork::Object* o) {
  _object = o;
}
const ork::Object* IRenderable::GetObject() const {
  return _object;
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

}} // namespace ork::lev2
