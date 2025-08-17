////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <optional>
#include <string>
#include <map>
#include <chrono>
#include <mutex>

namespace ork {

////////////////////////////////////////////////////////////////

class PasswordProvider {
public:
  // Get password from terminal with optional caching
  static std::optional<std::string> getPassword(
    const std::string& prompt,
    bool allow_cache = true,
    std::chrono::seconds cache_duration = std::chrono::seconds(300) // 5 minutes default
  );

  // Clear all cached passwords
  static void clearCache();

  // Clear specific cached password
  static void clearCachedPassword(const std::string& prompt);

  // Check if we're in an interactive terminal
  static bool isInteractive();

  // Check if a string is the password authentication sentinel
  static bool requiresPasswordAuth(const std::string& value);

private:
  struct CachedPassword {
    std::string password;
    std::chrono::steady_clock::time_point expiry;
  };

  static std::map<std::string, CachedPassword> _password_cache;
  static std::mutex _cache_mutex;

  // Platform-specific secure password reading
  static std::string readSecureInput(const std::string& prompt);

  // Cache management
  static std::optional<std::string> getCachedPassword(const std::string& prompt);
  static void cachePassword(const std::string& prompt, 
                          const std::string& password,
                          std::chrono::seconds duration);
};

////////////////////////////////////////////////////////////////

} // namespace ork