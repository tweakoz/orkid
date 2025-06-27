#include <ork/util/logger.h>
#include <ork/kernel/environment.h>

namespace ork {

    bool _ENABLE_LOGGING = true;

    struct GlobalLogFileManager {

        void writeLog(const std::string& channel, const std::string& message) {
            _globalFile.atomicOp([&](file_ptr_t& file) {
                if (file) {
                    file->Write(message.c_str(), message.length());
                }
            });
        }

        void setLogFile(const std::string& path) {
            _logFilePath = path;
            _globalFile.atomicOp([&](file_ptr_t& file) {
                file = std::make_shared<File>(path.c_str(), EFM_WRITE);
                printf("Global logfile<%s>\n", path.c_str());
            });
        }

        bool isEnabled() const {
            return _globalFile.atomicCopy() != nullptr;
        }
        
        LockedResource<file_ptr_t> _globalFile;
        std::string _logFilePath;
    };

    using globallogmgr_ptr_t = std::shared_ptr<GlobalLogFileManager>;

    static globallogmgr_ptr_t getGlobalLogManager() {
        static auto gGlobalLogManager = std::make_shared<GlobalLogFileManager>();
        return gGlobalLogManager;
    }

    // Wrapper functions
    void setGlobalLogFile(const std::string& path) {
        getGlobalLogManager()->setLogFile(path);
    }

    void writeToGlobalLog(const std::string& channel, const std::string& message) {
        getGlobalLogManager()->writeLog(channel, message);
    }

    bool isGlobalLogEnabled() {
        return getGlobalLogManager()->isEnabled();
    }

    LogChannel::LogChannel(std::string named, ork::fvec3 color, bool enabled){
      _enabled = enabled;
      _color = color;
      _name = named;
      _c1_prefix = ork::deco::asciic_rgb(color);
      _reset = ork::deco::asciic_reset();

      // Set up format strategy (common for all cases)
      _formatStrategy = [this](const char* format, va_list args) -> std::string {
          char buf[1024];
          vsnprintf_s(buf, sizeof(buf), format, args);
          return FormatString("%s[%s]\t%s%s", 
              _c1_prefix.c_str(), _name.c_str(), buf, _reset.c_str());
      };

      auto envvar = FormatString("ORKID_LOGFILE_%s",named.c_str());
      if (genviron.has(envvar)) {
        std::string logfilename;
        genviron.get(envvar,logfilename);
        _file = std::make_shared<File>(logfilename.c_str(),EFM_WRITE);
        _writeStrategy = [this](const std::string& str) {
            _file->Write(str.c_str(), str.length());
        };
        printf( "logfilename<%s>\n", logfilename.c_str() );
        enabled = true;
      }
      else {
        envvar = "ORKID_LOGFILE";
        if (genviron.has(envvar)) {
          std::string logfilename;
          genviron.get(envvar, logfilename);
          setGlobalLogFile(logfilename);
          _writeStrategy = [this](const std::string& str) {
              writeToGlobalLog(_name, str);
          };
          enabled = true;
        }
        else {
          _writeStrategy = [](const std::string& str) {
              printf("%s", str.c_str());
          };
        }
      }
    }

    void LogChannel::_log_internal(const char* format, va_list args, bool add_newline) const {
        if (_ENABLE_LOGGING && _enabled) {
            auto str = _formatStrategy(format, args);
            if (add_newline) str += "\n";
            _writeStrategy(str);
        }
    }

    void LogChannel::log_valist(const char *pMsgFormat, va_list args) const {
        _log_internal(pMsgFormat, args, true);
    }

    void LogChannel::log_begin_valist(const char *pMsgFormat, va_list args) const {
        _log_internal(pMsgFormat, args, false);
    }

    void LogChannel::log_continue_valist(const char *pMsgFormat, va_list args) const {
        if (_ENABLE_LOGGING && _enabled) {
            char buf[1024];
            vsnprintf_s(buf, sizeof(buf), pMsgFormat, args);
            auto str = FormatString("%s%s%s", _c1_prefix.c_str(), buf, _reset.c_str());
            _writeStrategy(str);
        }
    }

    void LogChannel::log(const char *pMsgFormat, ...) {
      if(_ENABLE_LOGGING and _enabled){
        va_list args;
        va_start(args, pMsgFormat);
        log_valist(pMsgFormat, args);
        va_end(args);
      }
    }

    void LogChannel::log_begin(const char *pMsgFormat, ...) {
      if(_ENABLE_LOGGING and _enabled){
        va_list args;
        va_start(args, pMsgFormat);
        log_begin_valist(pMsgFormat, args);
        va_end(args);
      }
    }

    void LogChannel::log_continue(const char *pMsgFormat, ...) const {
      if(_ENABLE_LOGGING and _enabled){
        va_list args;
        va_start(args, pMsgFormat);
        log_continue_valist(pMsgFormat, args);
        va_end(args);
      }
    }

    logchannel_ptr_t Logger::createChannel(std::string named, ork::fvec3 color,bool enabled){
      auto channel = std::make_shared<LogChannel>(named,color,enabled);
      _channels.atomicOp([named,channel](channel_map_t& unlocked){
      	unlocked[named]=channel;
      });
      return channel;
    }
    logchannel_ptr_t Logger::getChannel(std::string named) const {
    	logchannel_ptr_t rval;
      _channels.atomicOp([named,&rval](const channel_map_t& unlocked){
      	auto it = unlocked.find(named);
      	if(it!=unlocked.end()){
      		rval = it->second;
      	}
      });
      return rval;
    }

	  logger_ptr_t logger(){
	  	static logger_ptr_t logger = std::make_shared<Logger>();
	  	return logger;
	  }
	  logchannel_ptr_t logchannel(const std::string& named){
	  	auto the_logger = logger();
	  	auto chan = the_logger->getChannel(named);
	  	return chan;
	  }
    logchannel_ptr_t logerrchannel(){
      logchannel_ptr_t errchan = logger()->getChannel("ERROR");
      if(nullptr==errchan){
        errchan = logger()->createChannel("ERROR",fvec3(1,0,0));
      }
      return errchan;
    }

}