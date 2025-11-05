////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/util/logger.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/error/en.h>
#include <string>

using namespace ork;
using namespace rapidjson;

void runRapidJSONTests(void) {
    auto logchan = logger()->configureChannel("RapidJSON", fvec3(1.0f, 0.8f, 0.2f), true);

    logchan->log("========================================");
    logchan->log("Starting RapidJSON Tests");
    logchan->log("========================================");

    // Test 1: Parse JSON string
    logchan->log("");
    logchan->log("--- JSON Parsing Test ---");

    const char* jsonString = R"({
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
    logchan->log("%s", jsonString);

    Document doc;
    doc.Parse(jsonString);

    if (doc.HasParseError()) {
        logchan->log("✗ Parse error: %s (offset %zu)",
                     GetParseError_En(doc.GetParseError()),
                     doc.GetErrorOffset());
        return;
    }

    logchan->log("✓ JSON parsed successfully");

    // Test 2: Read values
    logchan->log("");
    logchan->log("--- Reading JSON Values ---");

    if (doc.HasMember("name") && doc["name"].IsString()) {
        logchan->log("name: \"%s\"", doc["name"].GetString());
    }

    if (doc.HasMember("version") && doc["version"].IsInt()) {
        logchan->log("version: %d", doc["version"].GetInt());
    }

    if (doc.HasMember("active") && doc["active"].IsBool()) {
        logchan->log("active: %s", doc["active"].GetBool() ? "true" : "false");
    }

    if (doc.HasMember("pi") && doc["pi"].IsNumber()) {
        logchan->log("pi: %.5f", doc["pi"].GetDouble());
    }

    if (doc.HasMember("null_value") && doc["null_value"].IsNull()) {
        logchan->log("null_value: null");
    }

    // Test 3: Read array
    logchan->log("");
    logchan->log("--- Reading Array ---");

    if (doc.HasMember("modules") && doc["modules"].IsArray()) {
        const Value& modules = doc["modules"];
        logchan->log("modules array has %d elements:", modules.Size());
        for (SizeType i = 0; i < modules.Size(); i++) {
            if (modules[i].IsString()) {
                logchan->log("  [%d]: \"%s\"", i, modules[i].GetString());
            }
        }
    }

    // Test 4: Read nested object
    logchan->log("");
    logchan->log("--- Reading Nested Object ---");

    if (doc.HasMember("config") && doc["config"].IsObject()) {
        const Value& config = doc["config"];
        logchan->log("config object:");

        if (config.HasMember("debug") && config["debug"].IsBool()) {
            logchan->log("  debug: %s", config["debug"].GetBool() ? "true" : "false");
        }
        if (config.HasMember("threads") && config["threads"].IsInt()) {
            logchan->log("  threads: %d", config["threads"].GetInt());
        }
        if (config.HasMember("memory_mb") && config["memory_mb"].IsInt()) {
            logchan->log("  memory_mb: %d", config["memory_mb"].GetInt());
        }
    }

    // Test 5: Create JSON document
    logchan->log("");
    logchan->log("--- Creating JSON Document ---");

    Document outDoc;
    outDoc.SetObject();
    Document::AllocatorType& allocator = outDoc.GetAllocator();

    // Add various types
    outDoc.AddMember("test_name", "RapidJSON iOS Test", allocator);
    outDoc.AddMember("timestamp", 1699142400, allocator);
    outDoc.AddMember("success", true, allocator);
    outDoc.AddMember("ratio", 0.95, allocator);

    // Add array
    Value testArray(kArrayType);
    testArray.PushBack("math", allocator);
    testArray.PushBack("dataflow", allocator);
    testArray.PushBack("kernel", allocator);
    testArray.PushBack("curl", allocator);
    testArray.PushBack("lz4", allocator);
    outDoc.AddMember("tests", testArray, allocator);

    // Add nested object
    Value metrics(kObjectType);
    metrics.AddMember("cpu_usage", 45.2, allocator);
    metrics.AddMember("memory_mb", 256, allocator);
    metrics.AddMember("fps", 60, allocator);
    outDoc.AddMember("metrics", metrics, allocator);

    // Add null value
    outDoc.AddMember("optional_data", Value(kNullType), allocator);

    logchan->log("✓ JSON document created");

    // Test 6: Serialize to compact JSON
    logchan->log("");
    logchan->log("--- Compact JSON Serialization ---");

    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    outDoc.Accept(writer);

    std::string compactJson = buffer.GetString();
    logchan->log("Compact JSON (%zu bytes):", compactJson.size());
    logchan->log("%s", compactJson.c_str());

    // Test 7: Serialize to pretty JSON
    logchan->log("");
    logchan->log("--- Pretty JSON Serialization ---");

    StringBuffer prettyBuffer;
    PrettyWriter<StringBuffer> prettyWriter(prettyBuffer);
    outDoc.Accept(prettyWriter);

    std::string prettyJson = prettyBuffer.GetString();
    logchan->log("Pretty JSON (%zu bytes):", prettyJson.size());
    logchan->log("%s", prettyJson.c_str());

    // Test 8: Modify document
    logchan->log("");
    logchan->log("--- Modifying JSON Document ---");

    if (outDoc.HasMember("success")) {
        outDoc["success"].SetBool(false);
        logchan->log("Changed 'success' to false");
    }

    if (outDoc.HasMember("tests") && outDoc["tests"].IsArray()) {
        outDoc["tests"].PushBack("rapidjson", allocator);
        logchan->log("Added 'rapidjson' to tests array");
    }

    outDoc.AddMember("modified", true, allocator);
    logchan->log("Added 'modified' field");

    // Serialize modified document
    StringBuffer modBuffer;
    Writer<StringBuffer> modWriter(modBuffer);
    outDoc.Accept(modWriter);
    logchan->log("Modified JSON: %s", modBuffer.GetString());

    // Test 9: Parse error handling
    logchan->log("");
    logchan->log("--- Error Handling Test ---");

    const char* invalidJson = "{\"invalid\": }";
    Document errorDoc;
    errorDoc.Parse(invalidJson);

    if (errorDoc.HasParseError()) {
        logchan->log("✓ Correctly detected parse error:");
        logchan->log("  Error: %s", GetParseError_En(errorDoc.GetParseError()));
        logchan->log("  Offset: %zu", errorDoc.GetErrorOffset());
    } else {
        logchan->log("✗ Failed to detect parse error");
    }

    // Test 10: Complex nested structure
    logchan->log("");
    logchan->log("--- Complex Nested Structure ---");

    Document complexDoc;
    complexDoc.SetObject();
    Document::AllocatorType& alloc = complexDoc.GetAllocator();

    // Create game state structure
    Value gameState(kObjectType);

    Value player(kObjectType);
    player.AddMember("name", "Player1", alloc);
    player.AddMember("level", 42, alloc);
    player.AddMember("health", 85.5, alloc);

    Value position(kObjectType);
    position.AddMember("x", 128.3, alloc);
    position.AddMember("y", 256.7, alloc);
    position.AddMember("z", -10.5, alloc);
    player.AddMember("position", position, alloc);

    Value inventory(kArrayType);
    Value item1(kObjectType);
    item1.AddMember("id", 1001, alloc);
    item1.AddMember("name", "Health Potion", alloc);
    item1.AddMember("quantity", 5, alloc);
    inventory.PushBack(item1, alloc);

    Value item2(kObjectType);
    item2.AddMember("id", 2003, alloc);
    item2.AddMember("name", "Magic Sword", alloc);
    item2.AddMember("quantity", 1, alloc);
    inventory.PushBack(item2, alloc);

    player.AddMember("inventory", inventory, alloc);
    gameState.AddMember("player", player, alloc);

    complexDoc.AddMember("game_state", gameState, alloc);

    // Pretty print complex structure
    StringBuffer complexBuffer;
    PrettyWriter<StringBuffer> complexWriter(complexBuffer);
    complexDoc.Accept(complexWriter);

    logchan->log("Complex game state structure:");
    logchan->log("%s", complexBuffer.GetString());

    logchan->log("");
    logchan->log("========================================");
    logchan->log("RapidJSON Tests Complete");
    logchan->log("========================================");
}
