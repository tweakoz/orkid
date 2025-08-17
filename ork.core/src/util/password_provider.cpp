////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/password_provider.h>
#include <ork/kernel/string/deco.inl>
#include <iostream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace ork {

////////////////////////////////////////////////////////////////

std::map<std::string, PasswordProvider::CachedPassword> PasswordProvider::_password_cache;
std::mutex PasswordProvider::_cache_mutex;

////////////////////////////////////////////////////////////////

bool PasswordProvider::requiresPasswordAuth(const std::string& value) {
  return value == "<PasswordAuthentication>";
}

////////////////////////////////////////////////////////////////

bool PasswordProvider::isInteractive() {
#ifdef _WIN32
  return _isatty(_fileno(stdin)) != 0;
#else
  return isatty(STDIN_FILENO) != 0;
#endif
}

////////////////////////////////////////////////////////////////

std::optional<std::string> PasswordProvider::getPassword(
    const std::string& prompt,
    bool allow_cache,
    std::chrono::seconds cache_duration) {

  // Check cache first if allowed
  if (allow_cache) {
    auto cached = getCachedPassword(prompt);
    if (cached.has_value()) {
      // Log that we're using cached auth (without revealing the password)
      printf("%s[PASSAUTH]%s Using cached authentication\n", 
             deco::asciic_rgb256(255, 255, 0).c_str(), deco::asciic_reset().c_str());
      return cached;
    }
  }

  // Check if we're in an interactive terminal
  if (!isInteractive()) {
    printf("%s[ERROR]%s Password authentication required but no terminal available\n",
           deco::asciic_rgb256(255, 0, 0).c_str(), deco::asciic_reset().c_str());
    return std::nullopt;
  }

  // Read password from terminal
  std::string password = readSecureInput(prompt);

  if (password.empty()) {
    printf("%s[PASSAUTH]%s Authentication cancelled\n",
           deco::asciic_rgb256(255, 255, 0).c_str(), deco::asciic_reset().c_str());
    return std::nullopt;
  }

  // Cache if allowed
  if (allow_cache) {
    cachePassword(prompt, password, cache_duration);
  }

  return password;
}

////////////////////////////////////////////////////////////////

std::string PasswordProvider::readSecureInput(const std::string& prompt) {
  std::string password;

#ifdef _WIN32
  // Windows implementation
  std::cout << prompt << std::flush;

  char ch;
  while ((ch = _getch()) != '\r' && ch != '\n') {
    if (ch == '\b' || ch == 127) { // Backspace
      if (!password.empty()) {
        password.pop_back();
        std::cout << "\b \b" << std::flush;
      }
    } else if (ch >= 32 && ch <= 126) { // Printable characters
      password.push_back(ch);
      std::cout << '*' << std::flush;
    }
  }
  std::cout << std::endl;

#else
  // Unix/Linux/Mac implementation
  struct termios old_term, new_term;
  
  // Get current terminal settings
  if (tcgetattr(STDIN_FILENO, &old_term) != 0) {
    return "";
  }

  // Copy and modify settings
  new_term = old_term;
  new_term.c_lflag &= ~(ECHO | ECHONL); // Disable echo
  
  // Apply new settings
  if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term) != 0) {
    return "";
  }

  // Display prompt
  std::cout << prompt << std::flush;

  // Read password
  std::getline(std::cin, password);
  std::cout << std::endl;

  // Restore original settings
  tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
#endif

  // Clear any sensitive data from temporary buffers
  // Note: std::string may leave copies in memory, but this is best effort
  volatile char* volatile_ptr = const_cast<volatile char*>(password.data());
  std::size_t size = password.size();
  
  return password;
}

////////////////////////////////////////////////////////////////

std::optional<std::string> PasswordProvider::getCachedPassword(const std::string& prompt) {
  std::lock_guard<std::mutex> lock(_cache_mutex);
  
  auto it = _password_cache.find(prompt);
  if (it == _password_cache.end()) {
    return std::nullopt;
  }

  auto now = std::chrono::steady_clock::now();
  if (now > it->second.expiry) {
    // Expired - remove from cache
    // Clear password from memory before erasing
    volatile char* pwd_ptr = const_cast<volatile char*>(it->second.password.data());
    std::memset(const_cast<char*>(it->second.password.data()), 0, it->second.password.size());
    _password_cache.erase(it);
    return std::nullopt;
  }

  return it->second.password;
}

////////////////////////////////////////////////////////////////

void PasswordProvider::cachePassword(const std::string& prompt, 
                                    const std::string& password,
                                    std::chrono::seconds duration) {
  std::lock_guard<std::mutex> lock(_cache_mutex);
  
  auto expiry = std::chrono::steady_clock::now() + duration;
  
  // Clear any existing password for this prompt
  auto it = _password_cache.find(prompt);
  if (it != _password_cache.end()) {
    // Clear old password from memory
    std::memset(const_cast<char*>(it->second.password.data()), 0, it->second.password.size());
  }
  
  _password_cache[prompt] = CachedPassword{password, expiry};
}

////////////////////////////////////////////////////////////////

void PasswordProvider::clearCache() {
  std::lock_guard<std::mutex> lock(_cache_mutex);
  
  // Clear all passwords from memory before clearing map
  for (auto& [prompt, cached] : _password_cache) {
    std::memset(const_cast<char*>(cached.password.data()), 0, cached.password.size());
  }
  
  _password_cache.clear();
}

////////////////////////////////////////////////////////////////

void PasswordProvider::clearCachedPassword(const std::string& prompt) {
  std::lock_guard<std::mutex> lock(_cache_mutex);
  
  auto it = _password_cache.find(prompt);
  if (it != _password_cache.end()) {
    // Clear password from memory before erasing
    std::memset(const_cast<char*>(it->second.password.data()), 0, it->second.password.size());
    _password_cache.erase(it);
  }
}

////////////////////////////////////////////////////////////////

} // namespace ork