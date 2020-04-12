////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

IRenderable::IRenderable() {
}

IRenderableDag::IRenderableDag()
    : IRenderable()
    , _object(0)
    , _modColor(fcolor4::White())
    , _drawDataA(nullptr)
    , _drawDataB(nullptr) {
}

void IRenderableDag::SetObject(const ork::Object* o) {
  _object = o;
}
const ork::Object* IRenderableDag::GetObject() const {
  return _object;
}

const fcolor4& IRenderableDag::GetModColor() const {
  return _modColor;
}
void IRenderableDag::SetModColor(const fcolor4& Color) {
  _modColor = Color;
}

void IRenderableDag::SetMatrix(const fmtx4& mtx) {
  _worldMatrix = mtx;
}
const fmtx4& IRenderableDag::GetMatrix() const {
  return _worldMatrix;
}

void IRenderableDag::SetDrawableDataA(const IRenderable::var_t& ap) {
  _drawDataA = ap;
}
const IRenderable::var_t& IRenderableDag::GetDrawableDataA() const {
  return _drawDataA;
}
void IRenderableDag::SetDrawableDataB(const IRenderable::var_t& ap) {
  _drawDataB = ap;
}
const IRenderable::var_t& IRenderableDag::GetDrawableDataB() const {
  return _drawDataB;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
