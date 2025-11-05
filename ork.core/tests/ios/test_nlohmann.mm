////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/util/logger.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using namespace ork;
using json = nlohmann::json;

void runNlohmannTests(void) {
    auto logchan = logger()->configureChannel("Nlohmann", fvec3(0.2f, 0.8f, 1.0f), true);

    logchan->log("========================================");
    logchan->log("Starting Nlohmann JSON Tests");
    logchan->log("========================================");

    // Test 1: Parse JSON string
    logchan->log("");
    logchan->log("--- JSON Parsing Test ---");

    std::string jsonString = R"({
        "name": "Orkid Engine",
        "version": 2023,
        "active": true,
        "modules": ["core", "lev2", "ecs"],
        "config": {
            "debug": false,
            "threads": 8,
            "memory_mb": 512
        },
        "pi": 3.14159,
        "null_value": null
    })";

    logchan->log("Input JSON:");
    logchan->log("%s", jsonString.c_str());

    json doc;
    try {
        doc = json::parse(jsonString);
        logchan->log("✓ JSON parsed successfully");
    } catch (const json::parse_error& e) {
        logchan->log("✗ Parse error: %s", e.what());
        return;
    }

    // Test 2: Read values
    logchan->log("");
    logchan->log("--- Reading JSON Values ---");

    if (doc.contains("name") && doc["name"].is_string()) {
        logchan->log("name: \"%s\"", doc["name"].get<std::string>().c_str());
    }

    if (doc.contains("version") && doc["version"].is_number_integer()) {
        logchan->log("version: %d", doc["version"].get<int>());
    }

    if (doc.contains("active") && doc["active"].is_boolean()) {
        logchan->log("active: %s", doc["active"].get<bool>() ? "true" : "false");
    }

    if (doc.contains("pi") && doc["pi"].is_number()) {
        logchan->log("pi: %.5f", doc["pi"].get<double>());
    }

    if (doc.contains("null_value") && doc["null_value"].is_null()) {
        logchan->log("null_value: null");
    }

    // Test 3: Read array
    logchan->log("");
    logchan->log("--- Reading Array ---");

    if (doc.contains("modules") && doc["modules"].is_array()) {
        const auto& modules = doc["modules"];
        logchan->log("modules array has %zu elements:", modules.size());
        for (size_t i = 0; i < modules.size(); i++) {
            if (modules[i].is_string()) {
                logchan->log("  [%zu]: \"%s\"", i, modules[i].get<std::string>().c_str());
            }
        }
    }

    // Test 4: Read nested object
    logchan->log("");
    logchan->log("--- Reading Nested Object ---");

    if (doc.contains("config") && doc["config"].is_object()) {
        const auto& config = doc["config"];
        logchan->log("config object:");

        if (config.contains("debug") && config["debug"].is_boolean()) {
            logchan->log("  debug: %s", config["debug"].get<bool>() ? "true" : "false");
        }
        if (config.contains("threads") && config["threads"].is_number_integer()) {
            logchan->log("  threads: %d", config["threads"].get<int>());
        }
        if (config.contains("memory_mb") && config["memory_mb"].is_number_integer()) {
            logchan->log("  memory_mb: %d", config["memory_mb"].get<int>());
        }
    }

    // Test 5: Create JSON document
    logchan->log("");
    logchan->log("--- Creating JSON Document ---");

    json outDoc;
    outDoc["test_name"] = "Nlohmann JSON iOS Test";
    outDoc["timestamp"] = 1699142400;
    outDoc["success"] = true;
    outDoc["ratio"] = 0.95;

    // Add array
    outDoc["tests"] = json::array({"math", "dataflow", "kernel", "curl", "lz4", "rapidjson"});

    // Add nested object
    json metrics;
    metrics["cpu_usage"] = 45.2;
    metrics["memory_mb"] = 256;
    metrics["fps"] = 60;
    outDoc["metrics"] = metrics;

    // Add null value
    outDoc["optional_data"] = nullptr;

    logchan->log("✓ JSON document created");

    // Test 6: Serialize to compact JSON
    logchan->log("");
    logchan->log("--- Compact JSON Serialization ---");

    std::string compactJson = outDoc.dump();
    logchan->log("Compact JSON (%zu bytes):", compactJson.size());
    logchan->log("%s", compactJson.c_str());

    // Test 7: Serialize to pretty JSON
    logchan->log("");
    logchan->log("--- Pretty JSON Serialization ---");

    std::string prettyJson = outDoc.dump(2); // 2 space indent
    logchan->log("Pretty JSON (%zu bytes):", prettyJson.size());
    logchan->log("%s", prettyJson.c_str());

    // Test 8: Modify document
    logchan->log("");
    logchan->log("--- Modifying JSON Document ---");

    outDoc["success"] = false;
    logchan->log("Changed 'success' to false");

    outDoc["tests"].push_back("nlohmann");
    logchan->log("Added 'nlohmann' to tests array");

    outDoc["modified"] = true;
    logchan->log("Added 'modified' field");

    logchan->log("Modified JSON: %s", outDoc.dump().c_str());

    // Test 9: Parse error handling
    logchan->log("");
    logchan->log("--- Error Handling Test ---");

    std::string invalidJson = "{\"invalid\": }";
    try {
        json errorDoc = json::parse(invalidJson);
        logchan->log("✗ Failed to detect parse error");
    } catch (const json::parse_error& e) {
        logchan->log("✓ Correctly detected parse error:");
        logchan->log("  Error: %s", e.what());
        logchan->log("  Position: byte %d", e.byte);
    }

    // Test 10: Complex nested structure
    logchan->log("");
    logchan->log("--- Complex Nested Structure ---");

    json gameState;
    gameState["game_state"]["player"]["name"] = "Player1";
    gameState["game_state"]["player"]["level"] = 42;
    gameState["game_state"]["player"]["health"] = 85.5;

    gameState["game_state"]["player"]["position"]["x"] = 128.3;
    gameState["game_state"]["player"]["position"]["y"] = 256.7;
    gameState["game_state"]["player"]["position"]["z"] = -10.5;

    json inventory = json::array();
    json item1;
    item1["id"] = 1001;
    item1["name"] = "Health Potion";
    item1["quantity"] = 5;
    inventory.push_back(item1);

    json item2;
    item2["id"] = 2003;
    item2["name"] = "Magic Sword";
    item2["quantity"] = 1;
    inventory.push_back(item2);

    gameState["game_state"]["player"]["inventory"] = inventory;

    logchan->log("Complex game state structure:");
    logchan->log("%s", gameState.dump(2).c_str());

    // Test 11: Type conversions
    logchan->log("");
    logchan->log("--- Type Conversions ---");

    json typeTests;
    typeTests["int_value"] = 42;
    typeTests["float_value"] = 3.14f;
    typeTests["double_value"] = 2.71828;
    typeTests["bool_value"] = true;
    typeTests["string_value"] = "test";

    logchan->log("int: %d", typeTests["int_value"].get<int>());
    logchan->log("float: %.2f", typeTests["float_value"].get<float>());
    logchan->log("double: %.5f", typeTests["double_value"].get<double>());
    logchan->log("bool: %s", typeTests["bool_value"].get<bool>() ? "true" : "false");
    logchan->log("string: \"%s\"", typeTests["string_value"].get<std::string>().c_str());

    // Test 12: Array operations
    logchan->log("");
    logchan->log("--- Array Operations ---");

    json numbers = json::array({1, 2, 3, 4, 5});
    logchan->log("Original array: %s", numbers.dump().c_str());

    numbers.push_back(6);
    logchan->log("After push_back(6): %s", numbers.dump().c_str());

    numbers.erase(0); // Remove first element
    logchan->log("After erase(0): %s", numbers.dump().c_str());

    logchan->log("Array size: %zu", numbers.size());
    logchan->log("Array empty? %s", numbers.empty() ? "yes" : "no");

    // Test 13: Object iteration
    logchan->log("");
    logchan->log("--- Object Iteration ---");

    json obj;
    obj["alpha"] = 1;
    obj["beta"] = 2;
    obj["gamma"] = 3;

    logchan->log("Iterating object:");
    for (auto& [key, value] : obj.items()) {
        logchan->log("  %s: %d", key.c_str(), value.get<int>());
    }

    // Test 14: Contains and type checking
    logchan->log("");
    logchan->log("--- Type Checking ---");

    json mixed;
    mixed["number"] = 42;
    mixed["text"] = "hello";
    mixed["flag"] = true;
    mixed["data"] = nullptr;
    mixed["list"] = json::array({1, 2, 3});
    mixed["dict"] = json::object({{"key", "value"}});

    logchan->log("Type checks:");
    logchan->log("  number is integer: %s", mixed["number"].is_number_integer() ? "yes" : "no");
    logchan->log("  text is string: %s", mixed["text"].is_string() ? "yes" : "no");
    logchan->log("  flag is boolean: %s", mixed["flag"].is_boolean() ? "yes" : "no");
    logchan->log("  data is null: %s", mixed["data"].is_null() ? "yes" : "no");
    logchan->log("  list is array: %s", mixed["list"].is_array() ? "yes" : "no");
    logchan->log("  dict is object: %s", mixed["dict"].is_object() ? "yes" : "no");

    logchan->log("");
    logchan->log("========================================");
    logchan->log("Nlohmann JSON Tests Complete");
    logchan->log("========================================");
}
