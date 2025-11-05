////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/util/logger.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/config.h>
#include <ork/file/path.h>
#import <Foundation/Foundation.h>

using namespace ork;
using namespace ork::asset::catalog;

void runCatalogTests(void) {
    auto logchan = logger()->configureChannel("CATALOG", fvec3(0.5f, 0.8f, 0.9f), true);

    logchan->log("========================================");
    logchan->log("Starting Catalog Tests (iOS)");
    logchan->log("========================================");

    try {
        // Get app bundle path using NSBundle
        logchan->log("");
        logchan->log("--- Locating Bundled Resources ---");

        NSBundle* mainBundle = [NSBundle mainBundle];
        NSString* bundlePath = [mainBundle bundlePath];
        std::string bundle_str = [bundlePath UTF8String];

        logchan->log("App bundle path: %s", bundle_str.c_str());

        // Construct paths to bundled manifests
        file::Path manifests_dir = file::Path(bundle_str) / "manifests";
        file::Path config_path = manifests_dir / "config.json";

        logchan->log("Manifests directory: %s", manifests_dir.c_str());
        logchan->log("Config path: %s", config_path.c_str());

        // Verify paths exist
        if (!manifests_dir.doesPathExist()) {
            logchan->log("ERROR: Manifests directory not found!");
            logchan->log("Make sure manifests are bundled in app");
            logchan->log("========================================");
            return;
        }

        if (!config_path.doesPathExist()) {
            logchan->log("ERROR: config.json not found!");
            logchan->log("========================================");
            return;
        }

        logchan->log("✓ Bundled resources located");

        // Load config explicitly
        logchan->log("");
        logchan->log("--- Loading Config ---");

        auto config_space = std::make_shared<AssetConfigSpace>();
        auto config = AssetConfigSpace::loadConfigFromDisk(config_space, config_path);

        if (!config) {
            logchan->log("ERROR: Failed to load config.json");
            logchan->log("========================================");
            return;
        }

        config_space->_configs["default"] = config;
        logchan->log("✓ Config loaded successfully");

        // Create catalog with config
        logchan->log("");
        logchan->log("--- Creating Catalog ---");

        auto catalog = std::make_shared<AssetCatalog>(config_space);
        catalog->setConfigSpace(config_space);

        logchan->log("✓ Catalog created");

        // Load manifests from bundled directory
        logchan->log("");
        logchan->log("--- Loading Manifests ---");

        catalog->loadManifestsFromPath(manifests_dir);

        logchan->log("✓ Manifests loaded");

        // Query available namespaces
        logchan->log("");
        logchan->log("--- Querying Catalog ---");

        auto namespaces = catalog->listNamespaces("*");
        logchan->log("Found %zu namespaces:", namespaces.size());

        for (const auto& ns : namespaces) {
            logchan->log("  - %s", ns.c_str());

            // Count assets in this namespace
            auto assets = catalog->listAssetsInNamespace(ns);
            logchan->log("    (%zu assets)", assets.size());
        }

        // List all assets (sample)
        logchan->log("");
        logchan->log("--- Sample Asset List ---");

        auto all_assets = catalog->listAssets("*");
        size_t sample_size = std::min(size_t(10), all_assets.size());

        logchan->log("Total assets: %zu (showing first %zu):", all_assets.size(), sample_size);
        for (size_t i = 0; i < sample_size; i++) {
            logchan->log("  %zu. %s", i + 1, all_assets[i].c_str());
        }

        if (all_assets.size() > sample_size) {
            logchan->log("  ... and %zu more", all_assets.size() - sample_size);
        }

        logchan->log("");
        logchan->log("✓ Catalog initialization successful!");

    } catch (const std::exception& e) {
        logchan->log("ERROR: Exception during catalog test: %s", e.what());
    } catch (...) {
        logchan->log("ERROR: Unknown exception during catalog test");
    }

    logchan->log("");
    logchan->log("========================================");
    logchan->log("Catalog Tests Complete");
    logchan->log("========================================");
}
