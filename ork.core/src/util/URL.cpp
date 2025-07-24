////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/URL.h>
#include <ork/kernel/string/deco.inl>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace ork {

////////////////////////////////////////////////////////////////////////////////

URL::URL(const std::string& url_string) {
  parse(url_string);
}

////////////////////////////////////////////////////////////////////////////////

void URL::parse(const std::string& url_string) {
  if (url_string.empty()) return;
  
  std::string remaining = url_string;
  
  // Parse scheme
  size_t scheme_end = remaining.find("://");
  if (scheme_end != std::string::npos) {
    _scheme = remaining.substr(0, scheme_end);
    remaining = remaining.substr(scheme_end + 3);
  }
  
  // Parse fragment
  size_t fragment_pos = remaining.find('#');
  if (fragment_pos != std::string::npos) {
    _fragment = remaining.substr(fragment_pos + 1);
    remaining = remaining.substr(0, fragment_pos);
  }
  
  // Parse query
  size_t query_pos = remaining.find('?');
  if (query_pos != std::string::npos) {
    _query = remaining.substr(query_pos + 1);
    remaining = remaining.substr(0, query_pos);
  }
  
  // Parse authority (userinfo@host:port) and path
  if (!_scheme.empty()) {
    // We have a scheme, so parse authority
    size_t path_start = remaining.find('/');
    std::string authority = (path_start != std::string::npos) 
                          ? remaining.substr(0, path_start)
                          : remaining;
    
    if (path_start != std::string::npos) {
      _path = remaining.substr(path_start);
    }
    
    // Parse userinfo
    size_t at_pos = authority.find('@');
    if (at_pos != std::string::npos) {
      _userinfo = authority.substr(0, at_pos);
      authority = authority.substr(at_pos + 1);
    }
    
    // Parse host and port
    size_t colon_pos = authority.rfind(':');
    if (colon_pos != std::string::npos) {
      std::string port_str = authority.substr(colon_pos + 1);
      bool all_digits = !port_str.empty() && 
                       std::all_of(port_str.begin(), port_str.end(), ::isdigit);
      if (all_digits) {
        _host = authority.substr(0, colon_pos);
        _port = std::stoi(port_str);
      } else {
        _host = authority;  // Colon was part of IPv6 address
      }
    } else {
      _host = authority;
    }
  } else {
    // No scheme, treat as path
    _path = remaining;
  }
}

////////////////////////////////////////////////////////////////////////////////

URL URL::withQuery(const std::string& key, const std::string& value) const {
  URL result = *this;
  
  // Build new query string
  std::string new_param = encodeQueryValue(key) + "=" + encodeQueryValue(value);
  
  if (_query.empty()) {
    result._query = new_param;
  } else {
    result._query = _query + "&" + new_param;
  }
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////

URL URL::withScheme(const std::string& scheme) const {
  URL result = *this;
  result._scheme = scheme;
  return result;
}

////////////////////////////////////////////////////////////////////////////////

URL URL::withHost(const std::string& host) const {
  URL result = *this;
  result._host = host;
  return result;
}

////////////////////////////////////////////////////////////////////////////////

URL URL::withPort(int port) const {
  URL result = *this;
  result._port = port;
  return result;
}

////////////////////////////////////////////////////////////////////////////////

URL URL::withPath(const std::string& path) const {
  URL result = *this;
  result._path = path;
  return result;
}

////////////////////////////////////////////////////////////////////////////////

URL URL::operator/(const std::string& segment) const {
  URL result = *this;
  
  if (_path.empty() || _path == "/") {
    result._path = "/" + encodePath(segment);
  } else if (_path.back() == '/') {
    result._path = _path + encodePath(segment);
  } else {
    result._path = _path + "/" + encodePath(segment);
  }
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////

URL URL::parent() const {
  URL result = *this;
  
  if (_path.empty() || _path == "/") {
    return result;
  }
  
  size_t last_slash = _path.rfind('/');
  if (last_slash != std::string::npos) {
    if (last_slash == 0) {
      result._path = "/";
    } else {
      result._path = _path.substr(0, last_slash);
    }
  }
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////

std::string URL::toString() const {
  std::stringstream ss;
  
  if (!_scheme.empty()) {
    ss << _scheme << "://";
  }
  
  if (!_userinfo.empty()) {
    ss << _userinfo << "@";
  }
  
  if (!_host.empty()) {
    ss << _host;
    if (_port > 0) {
      ss << ":" << _port;
    }
  }
  
  if (!_path.empty()) {
    ss << _path;
  } else if (!_host.empty()) {
    ss << "/";
  }
  
  if (!_query.empty()) {
    ss << "?" << _query;
  }
  
  if (!_fragment.empty()) {
    ss << "#" << _fragment;
  }
  
  return ss.str();
}

////////////////////////////////////////////////////////////////////////////////

bool URL::isValid() const {
  // Basic validation
  if (_scheme.empty() && _host.empty() && _path.empty()) {
    return false;
  }
  
  // If we have a host, we should have a scheme
  if (!_host.empty() && _scheme.empty()) {
    return false;
  }
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////

bool URL::isAbsolute() const {
  return !_scheme.empty() && !_host.empty();
}

//////////////////////////////////////////////////////////////////////////////
// Static encoding/decoding functions
//////////////////////////////////////////////////////////////////////////////

bool URL::isHexChar(char c) {
  return (c >= '0' && c <= '9') || 
         (c >= 'A' && c <= 'F') || 
         (c >= 'a' && c <= 'f');
}

////////////////////////////////////////////////////////////////////////////////

char URL::fromHex(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  return 0;
}

////////////////////////////////////////////////////////////////////////////////

std::string URL::encode(const std::string& str) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;
  
  for (char c : str) {
    // Keep alphanumeric and other safe characters
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
    } else {
      // Percent encode everything else
      escaped << std::uppercase;
      escaped << '%' << std::setw(2) << int((unsigned char)c);
      escaped << std::nouppercase;
    }
  }
  
  return escaped.str();
}

////////////////////////////////////////////////////////////////////////////////

std::string URL::decode(const std::string& str) {
  std::string result;
  
  for (size_t i = 0; i < str.length(); ++i) {
    if (str[i] == '%' && i + 2 < str.length() &&
        isHexChar(str[i + 1]) && isHexChar(str[i + 2])) {
      char ch = fromHex(str[i + 1]) * 16 + fromHex(str[i + 2]);
      result += ch;
      i += 2;
    } else if (str[i] == '+') {
      result += ' ';
    } else {
      result += str[i];
    }
  }
  
  return result;
}

////////////////////////////////////////////////////////////////////////////////

std::string URL::encodePath(const std::string& path) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;
  
  for (char c : path) {
    // Keep alphanumeric and path-safe characters
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
      escaped << c;
    } else {
      escaped << std::uppercase;
      escaped << '%' << std::setw(2) << int((unsigned char)c);
      escaped << std::nouppercase;
    }
  }
  
  return escaped.str();
}

////////////////////////////////////////////////////////////////////////////////

std::string URL::encodeQueryValue(const std::string& value) {
  std::ostringstream escaped;
  escaped.fill('0');
  escaped << std::hex;
  
  for (char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
    } else if (c == ' ') {
      escaped << '+';  // Space becomes + in query values
    } else {
      escaped << std::uppercase;
      escaped << '%' << std::setw(2) << int((unsigned char)c);
      escaped << std::nouppercase;
    }
  }
  
  return escaped.str();
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork