////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined(__linux__)

#include <ork/lev2/drm/drm_types.h>
#include <ork/util/logger.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <sys/select.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::drm {
///////////////////////////////////////////////////////////////////////////////

static logchannel_ptr_t logchan_drm = logger()->configureChannel("DRM", fvec3(0.9, 0.5, 0.2), true);

///////////////////////////////////////////////////////////////////////////////
// Mode implementation
///////////////////////////////////////////////////////////////////////////////

Mode::Mode(uint32_t idx, const drmModeModeInfo& mode_info, const std::string& mode_name)
    : index(idx)
    , width(mode_info.hdisplay)
    , height(mode_info.vdisplay)
    , refresh_rate(mode_info.vrefresh)
    , bit_depth(24)  // Default to 24-bit
    , name(mode_name) {
    memcpy(&drm_mode, &mode_info, sizeof(drmModeModeInfo));
}

///////////////////////////////////////////////////////////////////////////////
// Monitor implementation
///////////////////////////////////////////////////////////////////////////////

Monitor::Monitor(char letter, uint32_t conn_id)
    : device_letter(letter)
    , connector_id(conn_id)
    , connected(false) {
}

const char* Monitor::getConnectorTypeName(uint32_t type) {
    switch (type) {
        case DRM_MODE_CONNECTOR_VGA: return "VGA";
        case DRM_MODE_CONNECTOR_DVII: return "DVI-I";
        case DRM_MODE_CONNECTOR_DVID: return "DVI-D";
        case DRM_MODE_CONNECTOR_DVIA: return "DVI-A";
        case DRM_MODE_CONNECTOR_Composite: return "Composite";
        case DRM_MODE_CONNECTOR_SVIDEO: return "S-Video";
        case DRM_MODE_CONNECTOR_LVDS: return "LVDS";
        case DRM_MODE_CONNECTOR_Component: return "Component";
        case DRM_MODE_CONNECTOR_9PinDIN: return "9-Pin DIN";
        case DRM_MODE_CONNECTOR_DisplayPort: return "DisplayPort";
        case DRM_MODE_CONNECTOR_HDMIA: return "HDMI-A";
        case DRM_MODE_CONNECTOR_HDMIB: return "HDMI-B";
        case DRM_MODE_CONNECTOR_TV: return "TV";
        case DRM_MODE_CONNECTOR_eDP: return "eDP";
        case DRM_MODE_CONNECTOR_VIRTUAL: return "Virtual";
        case DRM_MODE_CONNECTOR_DSI: return "DSI";
        case DRM_MODE_CONNECTOR_DPI: return "DPI";
        case DRM_MODE_CONNECTOR_WRITEBACK: return "Writeback";
        case DRM_MODE_CONNECTOR_SPI: return "SPI";
        case DRM_MODE_CONNECTOR_USB: return "USB-C";
        default: return "Unknown";
    }
}

///////////////////////////////////////////////////////////////////////////////
// DRMContext implementation
///////////////////////////////////////////////////////////////////////////////

bool DRMContext::_isValidEDIDChar(char c) {
    return c >= 32 && c <= 126;  // Printable ASCII
}

std::string DRMContext::_parseEDID(const uint8_t* edid, size_t size) {
    if (!edid || size < 128) {
        return "Unknown";
    }

    // Check descriptor blocks (starts at byte 54)
    for (int k = 54; k < 126; k += 18) {
        // Monitor name descriptor has tag 0xFC
        if (edid[k] == 0x00 && edid[k+1] == 0x00 && edid[k+2] == 0x00 &&
            edid[k+3] == 0xFC && edid[k+4] == 0x00) {

            char name[14] = {0};
            memcpy(name, &edid[k+5], 13);

            // Filter out non-printable characters
            char filtered[14] = {0};
            int filtered_idx = 0;
            bool has_valid_chars = false;

            for (int l = 0; l < 13; l++) {
                if (name[l] == 0) break;

                if (_isValidEDIDChar(name[l])) {
                    filtered[filtered_idx++] = name[l];
                    has_valid_chars = true;
                }
            }

            // Trim trailing whitespace
            for (int l = filtered_idx - 1; l >= 0; l--) {
                if (filtered[l] == ' ') {
                    filtered[l] = 0;
                } else {
                    break;
                }
            }

            if (has_valid_chars && filtered[0] != 0) {
                return std::string(filtered);
            }
            break;
        }
    }

    return "Unknown";
}

void DRMContext::_enumerateCard(int drm_fd,
                                const std::string& card_path,
                                char& next_letter,
                                monitor_vect_t& out) {

    drmModeRes* res = drmModeGetResources(drm_fd);
    if (!res) {
        // Not all DRM nodes are KMS-capable (e.g. render-only nodes); skip quietly.
        logchan_drm->log("No DRM resources on %s (skipping)", card_path.c_str());
        return;
    }

    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector* connector = drmModeGetConnector(drm_fd, res->connectors[i]);
        if (!connector) continue;

        bool is_connected = (connector->connection == DRM_MODE_CONNECTED);

        // Skip disconnected monitors
        if (!is_connected) {
            drmModeFreeConnector(connector);
            continue;
        }

        char device_letter = next_letter;
        auto monitor = std::make_shared<Monitor>(device_letter, connector->connector_id);
        monitor->connected = true;
        monitor->card_path = card_path;
        monitor->connector_type = Monitor::getConnectorTypeName(connector->connector_type);

        // Build connector name
        char conn_name[64];
        snprintf(conn_name, sizeof(conn_name), "%s-%d",
                 monitor->connector_type.c_str(), connector->connector_type_id);
        monitor->connector_name = conn_name;

        // Try to get EDID data
        for (int j = 0; j < connector->count_props; j++) {
            drmModePropertyPtr prop = drmModeGetProperty(drm_fd, connector->props[j]);
            if (prop && strcmp(prop->name, "EDID") == 0) {
                drmModePropertyBlobPtr blob = drmModeGetPropertyBlob(drm_fd, connector->prop_values[j]);
                if (blob && blob->length >= 128) {
                    monitor->brand = _parseEDID((uint8_t*)blob->data, blob->length);
                }
                if (blob) drmModeFreePropertyBlob(blob);
            }
            if (prop) drmModeFreeProperty(prop);
        }

        // Enumerate modes
        if (connector->count_modes > 0) {
            for (int j = 0; j < connector->count_modes; j++) {
                char mode_name[8];
                snprintf(mode_name, sizeof(mode_name), "%c%d", device_letter, j);

                auto mode = std::make_shared<Mode>(j, connector->modes[j], mode_name);
                monitor->modes.push_back(mode);
            }
        }

        out.push_back(monitor);
        drmModeFreeConnector(connector);

        next_letter++;
    }

    drmModeFreeResources(res);
}

monitor_vect_t DRMContext::enumerateMonitors(int drm_fd) {
    // Single-fd enumeration (backward-compatible helper).
    monitor_vect_t monitors;
    char next_letter = 'a';
    _enumerateCard(drm_fd, "", next_letter, monitors);
    return monitors;
}

monitor_vect_t DRMContext::enumerateAllMonitors() {
    // Scan every /dev/dri/card* node and merge connected monitors, assigning
    // global device letters in card order. On multi-GPU systems the first
    // openable card is often an onboard VGA DAC, not the GPU driving the real
    // display, so enumerating only the first card hides the actual monitor.
    monitor_vect_t monitors;
    char next_letter = 'a';

    for (int i = 0; i < 10; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/dri/card%d", i);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) continue;
        _enumerateCard(fd, path, next_letter, monitors);
        close(fd);
    }

    return monitors;
}

void DRMContext::printMonitors(const monitor_vect_t& monitors) {
    printf("\nAvailable Displays:\n\n");

    if (monitors.empty()) {
        printf("No connected displays found.\n\n");
        return;
    }

    for (const auto& monitor : monitors) {
        printf("Device %c:\n", monitor->device_letter);
        if (!monitor->card_path.empty()) {
            printf("  Card: %s\n", monitor->card_path.c_str());
        }
        printf("  Connector: %s (%s)\n", monitor->connector_name.c_str(), monitor->connector_type.c_str());
        printf("  Brand: %s\n", monitor->brand.c_str());

        if (!monitor->modes.empty()) {
            printf("  Modes:\n");
            for (const auto& mode : monitor->modes) {
                printf("    %s: %dx%d @ %dHz (%d-bit)\n",
                       mode->name.c_str(), mode->width, mode->height,
                       mode->refresh_rate, mode->bit_depth);
            }
        }
        printf("\n");
    }
}

void DRMContext::listMonitorsAndExit() {
    auto monitors = enumerateAllMonitors();
    printMonitors(monitors);
    exit(0);
}

void DRMContext::pageFlipHandler(int fd, unsigned int sequence,
                                 unsigned int tv_sec, unsigned int tv_usec,
                                 void* user_data) {
    DRMContext* drm = static_cast<DRMContext*>(user_data);
    drm->flipPending = false;
}

void DRMContext::emergencyCleanup(DRMContext* drm) {
    if (!drm) return;

    // Restore saved CRTC state
    if (drm->saved_crtc) {
        drmModeSetCrtc(drm->drm_fd, drm->saved_crtc->crtc_id, drm->saved_crtc->buffer_id,
                       drm->saved_crtc->x, drm->saved_crtc->y,
                       &drm->connector_id, 1, &drm->saved_crtc->mode);
    }

    // Cleanup framebuffers
    for (uint32_t i = 0; i < SWAP_CHAIN_SIZE; i++) {
        if (drm->fb_ids[i]) {
            drmModeRmFB(drm->drm_fd, drm->fb_ids[i]);
        }
    }

    if (drm->drm_fd >= 0) {
        close(drm->drm_fd);
    }
}

void DRMContext::waitForVblank() {
    static drmEventContext evctx = {};
    static bool initialized = false;
    static int frame_count = 0;

    if (!initialized) {
        evctx.version = DRM_EVENT_CONTEXT_VERSION;
        evctx.page_flip_handler = DRMContext::pageFlipHandler;
        initialized = true;
    }

    // Timing measurement
    auto start_time = std::chrono::high_resolution_clock::now();

    // Maximum wait time: 100ms (should be ~16ms at 60Hz)
    int max_iterations = 100;
    int iterations = 0;

    while (flipPending && iterations < max_iterations) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(drm_fd, &fds);

        struct timeval timeout = {0, 1000};  // 1ms timeout per iteration
        int ret = ::select(drm_fd + 1, &fds, nullptr, nullptr, &timeout);

        if (ret > 0) {
            drmHandleEvent(drm_fd, &evctx);
        }

        iterations++;
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    float wait_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();

    // Print vblank wait time every 10 frames
    if (frame_count % 10 == 0 && frame_count < 50) {
        printf("Vblank wait: %.2fms (%d iterations)\n", wait_ms, iterations);
        fflush(stdout);
    }
    frame_count++;

    if (flipPending) {
        logchan_drm->log("WARNING: vblank wait timed out after %dms - page flip may have failed", max_iterations);
        flipPending = false;  // Force reset to prevent infinite hang
    }
}

DRMContext::DRMContext(char deviceLetter, int modeIndex) {
    logchan_drm->log("Creating DRM context for device %c, mode %d", deviceLetter, modeIndex);

    // Enumerate monitors across ALL cards so 'deviceLetter' maps consistently
    // with what the user saw from the monitor listing.
    drm_fd = -1;
    auto monitors = enumerateAllMonitors();

    // Find requested monitor
    monitor_ptr_t selected_monitor = nullptr;
    for (const auto& mon : monitors) {
        if (mon->device_letter == deviceLetter) {
            selected_monitor = mon;
            break;
        }
    }

    if (!selected_monitor) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                "Device '%c' not found. Available devices:", deviceLetter);
        logchan_drm->log("ERROR: %s", error_msg);
        printMonitors(monitors);
        throw std::runtime_error(error_msg);
    }

    // Open the specific card that owns the selected monitor (NOT just the first
    // openable card - that may be an onboard VGA DAC on a multi-GPU box).
    drm_fd = open(selected_monitor->card_path.c_str(), O_RDWR | O_CLOEXEC);
    if (drm_fd < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                "Failed to open DRM device '%s' for device '%c'",
                selected_monitor->card_path.c_str(), deviceLetter);
        logchan_drm->log("ERROR: %s", error_msg);
        throw std::runtime_error(error_msg);
    }
    logchan_drm->log("Opened DRM device: %s (device %c)",
                     selected_monitor->card_path.c_str(), deviceLetter);

    if (!selected_monitor->connected) {
        close(drm_fd);
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                "Device '%c' (%s) is not connected.",
                deviceLetter, selected_monitor->connector_name.c_str());
        logchan_drm->log("ERROR: %s", error_msg);
        throw std::runtime_error(error_msg);
    }

    if (selected_monitor->modes.empty()) {
        close(drm_fd);
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                "Device '%c' has no available modes.", deviceLetter);
        logchan_drm->log("ERROR: %s", error_msg);
        throw std::runtime_error(error_msg);
    }

    // Validate mode index
    if (modeIndex < 0 || modeIndex >= (int)selected_monitor->modes.size()) {
        close(drm_fd);
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                "Mode %d not available for device '%c' (has %zu modes).",
                modeIndex, deviceLetter, selected_monitor->modes.size());
        logchan_drm->log("ERROR: %s", error_msg);
        throw std::runtime_error(error_msg);
    }

    // Get selected mode
    auto selected_mode = selected_monitor->modes[modeIndex];
    connector_id = selected_monitor->connector_id;
    memcpy(&mode, &selected_mode->drm_mode, sizeof(drmModeModeInfo));
    imageExtent.width = selected_mode->width;
    imageExtent.height = selected_mode->height;

    logchan_drm->log("Using Device %c (%s - %s): %dx%d @ %dHz",
                     deviceLetter,
                     selected_monitor->connector_name.c_str(),
                     selected_monitor->brand.c_str(),
                     selected_mode->width,
                     selected_mode->height,
                     selected_mode->refresh_rate);

    // Get DRM resources
    drmModeRes* res = drmModeGetResources(drm_fd);
    if (!res) {
        close(drm_fd);
        throw std::runtime_error("Failed to get DRM resources");
    }

    // Get connector
    drmModeConnector* connector = drmModeGetConnector(drm_fd, connector_id);
    if (!connector) {
        drmModeFreeResources(res);
        close(drm_fd);
        throw std::runtime_error("Failed to get connector");
    }

    // Find encoder and CRTC
    drmModeEncoder* encoder = nullptr;
    if (connector->encoder_id) {
        encoder = drmModeGetEncoder(drm_fd, connector->encoder_id);
        if (encoder) {
            crtc_id = encoder->crtc_id;
        }
    }

    if (!crtc_id) {
        for (int i = 0; i < res->count_crtcs; i++) {
            crtc_id = res->crtcs[i];
            break;
        }
    }

    if (!crtc_id) {
        if (encoder) drmModeFreeEncoder(encoder);
        drmModeFreeConnector(connector);
        drmModeFreeResources(res);
        close(drm_fd);
        throw std::runtime_error("Failed to find CRTC");
    }

    // Save current CRTC
    saved_crtc = drmModeGetCrtc(drm_fd, crtc_id);

    if (encoder) drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(res);

    logchan_drm->log("DRM context created successfully");
}

void DRMContext::restoreCrtc() {
    if (drm_fd < 0)
        return;
    if (saved_crtc and saved_crtc->buffer_id) {
        // a real framebuffer was on screen before us (e.g. a console) — put it back
        drmModeSetCrtc(drm_fd, saved_crtc->crtc_id, saved_crtc->buffer_id,
                       saved_crtc->x, saved_crtc->y,
                       &connector_id, 1, &saved_crtc->mode);
    } else if (crtc_id) {
        // nothing was displaying before us — disable the CRTC so the monitor goes
        // black instead of scanning out our soon-to-be-freed framebuffer
        drmModeSetCrtc(drm_fd, crtc_id, 0, 0, 0, nullptr, 0, nullptr);
    }
    if (saved_crtc) {
        drmModeFreeCrtc(saved_crtc);
        saved_crtc = nullptr;
    }
    crtc_id = 0; // idempotence: second call no-ops
}

DRMContext::~DRMContext() {
    logchan_drm->log("Destroying DRM context");

    // Restore DRM (no-op if _runloopEnd already did it)
    restoreCrtc();

    // Cleanup framebuffers and dmabufs
    for (uint32_t i = 0; i < SWAP_CHAIN_SIZE; i++) {
        if (fb_ids[i]) drmModeRmFB(drm_fd, fb_ids[i]);
        if (dmabuf_fds[i] >= 0) close(dmabuf_fds[i]);
    }

    if (drm_fd >= 0) close(drm_fd);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::drm
///////////////////////////////////////////////////////////////////////////////

#endif // __linux__
