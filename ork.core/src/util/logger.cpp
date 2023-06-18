#include <ork/util/logger.h>

namespace ork {

    bool _ENABLE_LOGGING = true;

    LogChannel::LogChannel(std::string named, ork::fvec3 color, bool enabled){
      _enabled = enabled;
      _color = color;
      _name = named;
      _c1_prefix = ork::deco::asciic_rgb(color);
      _reset = ork::deco::asciic_reset();
    }

    void LogChannel::log_valist(const char *pMsgFormat, va_list args) const {
      char buf[1024];
      vsnprintf_s(buf, sizeof(buf), pMsgFormat, args);
      printf( "%s[%s]\t%s%s\n", _c1_prefix.c_str(), _name.c_str(), buf, _reset.c_str() );
    }
    void LogChannel::log(const char *pMsgFormat, ...) {
      if(_ENABLE_LOGGING and _enabled){
        va_list args;
        va_start(args, pMsgFormat);
        log_valist(pMsgFormat, args);
        va_end(args);
      }
    }

    void LogChannel::log_begin_valist(const char *pMsgFormat, va_list args) const {
      char buf[1024];
      vsnprintf_s(buf, sizeof(buf), pMsgFormat, args);
      printf( "%s[%s]\t%s%s", _c1_prefix.c_str(), _name.c_str(), buf, _reset.c_str() );
    }
    void LogChannel::log_begin(const char *pMsgFormat, ...) {
      if(_ENABLE_LOGGING and _enabled){
        va_list args;
        va_start(args, pMsgFormat);
        log_begin_valist(pMsgFormat, args);
        va_end(args);
      }
    }

    void LogChannel::log_continue_valist(const char *pMsgFormat, va_list args) const {
      char buf[1024];
      vsnprintf_s(buf, sizeof(buf), pMsgFormat, args);
      printf( "%s%s%s", _c1_prefix.c_str(), buf, _reset.c_str() );
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