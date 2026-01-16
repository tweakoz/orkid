////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/lev2/gfx/shmtexture.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_txishm = logger()->configureChannel("VKTXISHM", fvec3(0.2, 0.8, 0.5), true);
///////////////////////////////////////////////////////////////////////////////

bool VkTextureInterface::initFromShm(texture_ptr_t tex, std::shared_ptr<ShmTexConsumer> consumer) {
  if (!consumer || !consumer->isConnected()) {
    return false;
  }

  // Try to acquire a new frame
  ShmTexConsumer::FrameData frame;
  if (!consumer->tryGetFrame(frame)) {
    return false;  // No new frame available
  }

  if(0)logchan_txishm->log("initFromShm: got frame seq<%zu> %dx%d fmt<%d>",
                      frame.sequence, frame.width, frame.height, (int)frame.format);

  // Build TextureInitData from the frame
  TextureInitData tid;
  tid._w = frame.width;
  tid._h = frame.height;
  tid._d = 1;
  tid._src_format = frame.format;
  tid._dst_format = frame.format;
  tid._autogenmips = false;
  tid._data = frame.pixels;
  tid._allow_async = false;  // Synchronous upload - data must be copied before releaseFrame()

  // Initialize/update the texture
  initTextureFromData(tex.get(), tid);

  // Release the frame back to SHM
  consumer->releaseFrame();

  return true;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
