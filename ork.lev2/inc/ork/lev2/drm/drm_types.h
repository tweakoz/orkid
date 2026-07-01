////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#if defined(__linux__)

#include <ork/orkconfig.h>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

extern "C" {
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::drm {
///////////////////////////////////////////////////////////////////////////////

// Forward declarations
struct Mode;
struct Monitor;
struct DRMContext;

// Type aliases (Orkid convention)
using mode_ptr_t = std::shared_ptr<Mode>;
using mode_constptr_t = std::shared_ptr<const Mode>;
using mode_vect_t = std::vector<mode_ptr_t>;

using monitor_ptr_t = std::shared_ptr<Monitor>;
using monitor_constptr_t = std::shared_ptr<const Monitor>;
using monitor_vect_t = std::vector<monitor_ptr_t>;

using drm_context_ptr_t = std::shared_ptr<DRMContext>;
using drm_context_rawptr_t = DRMContext*;

///////////////////////////////////////////////////////////////////////////////
// Display mode information
///////////////////////////////////////////////////////////////////////////////
struct Mode {
    uint32_t index;              // Mode index within this monitor
    uint32_t width;              // Horizontal resolution
    uint32_t height;             // Vertical resolution
    uint32_t refresh_rate;       // Refresh rate in Hz
    uint32_t bit_depth;          // Bits per pixel
    std::string name;            // Mode name (e.g., "a0", "b1")
    drmModeModeInfo drm_mode;    // Full DRM mode info

    Mode(uint32_t idx, const drmModeModeInfo& mode_info, const std::string& mode_name);
};

///////////////////////////////////////////////////////////////////////////////
// Monitor/Display information
///////////////////////////////////////////////////////////////////////////////
struct Monitor {
    char device_letter;          // 'a', 'b', 'c', etc.
    uint32_t connector_id;       // DRM connector ID
    std::string connector_type;  // "HDMI-A", "DP", "DVI-D", etc.
    std::string connector_name;  // "HDMI-A-1", "DP-2", etc.
    std::string brand;           // Monitor brand/model from EDID
    bool connected;              // Is monitor connected?
    mode_vect_t modes;           // Available modes
    std::string card_path;       // Owning DRM card node (e.g. "/dev/dri/card3")

    Monitor(char letter, uint32_t conn_id);

    // Get connector type name from DRM connector type
    static const char* getConnectorTypeName(uint32_t type);
};

///////////////////////////////////////////////////////////////////////////////
// DRM Context - manages display output via Direct Rendering Manager
///////////////////////////////////////////////////////////////////////////////
struct DRMContext {
    static constexpr uint32_t SWAP_CHAIN_SIZE = 3;

    // DRM state
    int drm_fd = -1;
    uint32_t connector_id = 0;
    uint32_t crtc_id = 0;
    drmModeModeInfo mode;
    drmModeCrtc* saved_crtc = nullptr;

    // Swap chain framebuffers (triple buffering)
    uint32_t fb_ids[SWAP_CHAIN_SIZE] = {0};
    int dmabuf_fds[SWAP_CHAIN_SIZE] = {-1, -1, -1};

    // Page flip state
    uint32_t displayingImage = 0;
    bool flipPending = false;
    bool firstFrame = true;

    VkExtent2D imageExtent = {0, 0};

    // Methods
    DRMContext(char deviceLetter, int modeIndex);
    ~DRMContext();

    void waitForVblank();

    // Static helpers
    static monitor_vect_t enumerateMonitors(int drm_fd);
    // Enumerate connected monitors across ALL /dev/dri/card* nodes, assigning
    // global device letters ('a', 'b', ...) in card order. This is the correct
    // entry point on multi-GPU systems where the first openable card may be an
    // onboard VGA DAC rather than the GPU driving the real display.
    static monitor_vect_t enumerateAllMonitors();
    static void printMonitors(const monitor_vect_t& monitors);
    static void listMonitorsAndExit();
    static void pageFlipHandler(int fd, unsigned int sequence,
                               unsigned int tv_sec, unsigned int tv_usec,
                               void* user_data);
    static void emergencyCleanup(DRMContext* drm);

private:
    // Enumerate connected connectors on a single already-open card fd, appending
    // to 'out'. 'card_path' stamps each monitor's owning card; 'next_letter' is
    // advanced so device letters stay globally unique across cards.
    static void _enumerateCard(int drm_fd,
                               const std::string& card_path,
                               char& next_letter,
                               monitor_vect_t& out);
    static std::string _parseEDID(const uint8_t* edid_data, size_t size);
    static bool _isValidEDIDChar(char c);
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::drm
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
