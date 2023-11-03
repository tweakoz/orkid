
#pragma once

#include <ork/kernel/string/deco.inl>
#include <ork/kernel/mutex.h>
#include <ork/file/file.h>

namespace ork {

  extern bool _ENABLE_LOGGING;

  struct LogChannel{

    LogChannel(std::string named, ork::fvec3 color,bool enabled=true);
    void log_valist(const char *pMsgFormat, va_list args) const;
    void log(const char *pMsgFormat, ...);
    void log_begin_valist(const char *pMsgFormat, va_list args) const;
    void log_begin(const char *pMsgFormat, ...);
    void log_continue_valist(const char *pMsgFormat, va_list args) const;
    void log_continue(const char *pMsgFormat, ...) const;

    ork::fvec3 _color;
    std::string _name;
    std::string _c1_prefix;
    std::string _reset;
    bool _enabled;
    file_ptr_t _file; // if not null, log to file

  };

  using logchannel_ptr_t = std::shared_ptr<LogChannel>;

  struct Logger {

    logchannel_ptr_t createChannel(std::string named, ork::fvec3 color, bool enabled=true);
    logchannel_ptr_t getChannel(std::string named) const;

		using channel_map_t = std::map<std::string,logchannel_ptr_t>;
		ork::LockedResource<channel_map_t> _channels;
  };

  using logger_ptr_t = std::shared_ptr<Logger>;

  logger_ptr_t logger();
  logchannel_ptr_t logchannel(const std::string& named);
  logchannel_ptr_t logerrchannel();

  /////////////////////////////////////////////////////////////////////
}
