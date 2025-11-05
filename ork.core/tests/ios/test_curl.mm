////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/test/ork_tests.h>
#include <ork/util/logger.h>
#include <curl/curl.h>
#include <string>

using namespace ork;

// Callback function to receive data from curl
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

void runCurlTests(void) {
    auto logchan = logger()->configureChannel("CURLTEST", fvec3(1.0f, 0.5f, 0.5f), true);

    logchan->log("========================================");
    logchan->log("Starting Curl Tests");
    logchan->log("========================================");

    // Initialize curl
    CURL* curl = curl_easy_init();
    if (!curl) {
        logchan->log("ERROR: Failed to initialize curl");
        logchan->log("========================================");
        return;
    }

    logchan->log("");
    logchan->log("--- HTTP GET Test ---");
    logchan->log("Fetching: http://example.com/");

    std::string responseData;
    CURLcode res;

    // Set curl options
    curl_easy_setopt(curl, CURLOPT_URL, "http://example.com/");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    // Perform the request
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        logchan->log("ERROR: curl_easy_perform() failed: %s", curl_easy_strerror(res));
    } else {
        // Get response code
        long response_code;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        logchan->log("Response code: %ld", response_code);

        // Get content type
        char* content_type = nullptr;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
        if (content_type) {
            logchan->log("Content type: %s", content_type);
        }

        // Log response size
        logchan->log("Response size: %zu bytes", responseData.size());

        // Log first 500 characters of response
        logchan->log("");
        logchan->log("--- Response Preview (first 500 chars) ---");
        std::string preview = responseData.substr(0, std::min(size_t(500), responseData.size()));
        logchan->log("%s", preview.c_str());
        if (responseData.size() > 500) {
            logchan->log("... (%zu more bytes)", responseData.size() - 500);
        }
    }

    // Cleanup
    curl_easy_cleanup(curl);

    logchan->log("");
    logchan->log("========================================");
    logchan->log("Curl Tests Complete");
    logchan->log("========================================");
}
