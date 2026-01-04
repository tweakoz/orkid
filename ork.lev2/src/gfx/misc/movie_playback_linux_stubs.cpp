////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// Linux stubs for hardware-accelerated movie playback backends
// VAAPI (AMD) and NVDEC (NVIDIA) are not yet implemented

#if defined(__linux__)

#include <ork/lev2/gfx/util/movie.inl>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
// VAAPI Backend (AMD hardware decode)
// Stub - returns nullptr to trigger fallback to FFmpeg
///////////////////////////////////////////////////////////////////////////////

moviebackendimpl_ptr_t createVAAPIBackend(MoviePlaybackContext* ctx) {
  printf("createVAAPIBackend: VA-API hardware decode not yet implemented, use FFmpeg backend\n");
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// NVDEC Backend (NVIDIA hardware decode)
// Stub - returns nullptr to trigger fallback to FFmpeg
///////////////////////////////////////////////////////////////////////////////

moviebackendimpl_ptr_t createNVDECBackend(MoviePlaybackContext* ctx) {
  printf("createNVDECBackend: NVDEC hardware decode not yet implemented, use FFmpeg backend\n");
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2

#endif // __linux__
