#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/util/triple_buffer.h>

using namespace ork;
using namespace ork::lev2;

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SignalScope::setRect(int iX, int iY, int iW, int iH, bool snap) {
  _hudpanel->_uipanel->SetRect(iX, iY, iW, iH);
  if (snap)
    _hudpanel->_uipanel->snap();
}
///////////////////////////////////////////////////////////////////////////////
void ScopeSource::connect(scopesink_ptr_t sink) {
  _sinks.insert(sink);
}
///////////////////////////////////////////////////////////////////////////////
void ScopeSource::disconnect(scopesink_ptr_t sink) {
  auto it = _sinks.find(sink);
  if (it != _sinks.end()) {
    _sinks.erase(it);
  }
}
///////////////////////////////////////////////////////////////////////////////
void ScopeSource::updateController(const ControllerInst* controller) {
  _controller = controller;
  for (auto s : _sinks) {
    s->sourceUpdated(this);
  }
  _controller = nullptr;
}
///////////////////////////////////////////////////////////////////////////////
void ScopeSource::notifySinksUpdated() {
  for (auto s : _sinks) {
    s->sourceUpdated(this);
  }
}
///////////////////////////////////////////////////////////////////////////////
void ScopeSource::notifySinksKeyOn(KeyOnInfo& koi) {
  for (auto s : _sinks) {
    s->sourceKeyOn(this, koi);
  }
}
///////////////////////////////////////////////////////////////////////////////
void ScopeSource::notifySinksKeyOff() {
  for (auto s : _sinks) {
    s->sourceKeyOff(this);
  }
}
///////////////////////////////////////////////////////////////////////////////
void ScopeSource::updateMono(int numframes, const float* mono, bool notifysinks) {
  OrkAssert(numframes <= koscopelength);
  float* dest = _scopebuffer._samples;
  if ((_writehead + numframes) > koscopelength) {
    int num2write = koscopelength - _writehead;

    printf(
        "_writehead<%d> numframes<%d> wpn<%d> n2w<%d>\n", //
        _writehead,
        numframes,
        (_writehead + numframes),
        num2write);

    OrkAssert(_writehead + num2write <= koscopelength);
    memcpy(dest + _writehead, mono, num2write * sizeof(float));
    numframes -= num2write;
    mono += num2write;
    _writehead = 0;
  }
  OrkAssert(_writehead + numframes <= koscopelength);
  memcpy(dest + _writehead, mono, numframes * sizeof(float));
  _writehead = (_writehead + numframes) % koscopelength;
  if (notifysinks)
    notifySinksUpdated();
}
///////////////////////////////////////////////////////////////////////////////
void ScopeSource::updateStereo(
    int numframes, //
    const float* left,
    const float* right,
    bool notifysinks) {
  OrkAssert(numframes <= koscopelength);
  float* dest = _scopebuffer._samples;
  if ((_writehead + numframes) > koscopelength) {
    int num2write = koscopelength - _writehead;
    printf(
        "_writehead<%d> numframes<%d> wpn<%d> n2w<%d>\n", //
        _writehead,
        numframes,
        (_writehead + numframes),
        num2write);

    OrkAssert(_writehead + num2write <= koscopelength);
    for (int i = 0; i < num2write; i++) {
      dest[i + _writehead] = (left[i] + right[i]) * 0.5f;
    }
    numframes -= num2write;
    left += num2write;
    right += num2write;
    _writehead = 0;
  }
  OrkAssert(_writehead + numframes <= koscopelength);
  for (int i = 0; i < numframes; i++) {
    dest[i + _writehead] = (left[i] + right[i]) * 0.5f;
  }
  _writehead = (_writehead + numframes) % koscopelength;
  if (notifysinks)
    notifySinksUpdated();
}
///////////////////////////////////////////////////////////////////////////////
void ScopeSink::sourceUpdated(const ScopeSource* src) {
  if (_onupdate)
    _onupdate(src);
}
///////////////////////////////////////////////////////////////////////////////
void ScopeSink::sourceKeyOn(const ScopeSource* src, KeyOnInfo& koi) {
  if (_onkeyon)
    _onkeyon(src, koi);
}
///////////////////////////////////////////////////////////////////////////////
void ScopeSink::sourceKeyOff(const ScopeSource* src) {
  if (_onkeyoff)
    _onkeyoff(src);
}
///////////////////////////////////////////////////////////////////////////////
ScopeBuffer::ScopeBuffer(int tbufindex)
    : _tbindex(tbufindex) {
  for (int i = 0; i < koscopelength; i++) {
    _samples[i] = 0.0f;
  }
} ///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
