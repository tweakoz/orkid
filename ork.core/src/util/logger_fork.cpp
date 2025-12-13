////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/logger.h>
#include <vector>

namespace ork {

////////////////////////////////////////////////////////////////
// ForkBackend Implementation
//
// Forwards log calls to multiple child backends.
// Sets chan->_backend_impl to each child's impl before calling
// so child functions can access their impl.
////////////////////////////////////////////////////////////////

struct ForkBackendImpl {
  std::vector<logger_backend_ptr_t> _children;

  void addChild(logger_backend_ptr_t child) {
    _children.push_back(child);
  }
};

using fork_backend_impl_ptr_t = std::shared_ptr<ForkBackendImpl>;

////////////////////////////////////////////////////////////////
// Fork backend function pointers
////////////////////////////////////////////////////////////////

static void forkLogFn(const LogChannel* chan, const std::string& str) {
  auto impl = chan->_logger->_backend->_impl.tryAs<fork_backend_impl_ptr_t>();
  if (impl) {
    for (auto& child : impl.value()->_children) {
      if (child->_add_log_line) {
        // Set channel's backend_impl to child's impl for the duration of the call
        const_cast<LogChannel*>(chan)->_backend_impl = child->_impl;
        child->_add_log_line(chan, str);
      }
    }
  }
}

static void forkBeginLogFn(const LogChannel* chan, const std::string& str) {
  auto impl = chan->_logger->_backend->_impl.tryAs<fork_backend_impl_ptr_t>();
  if (impl) {
    for (auto& child : impl.value()->_children) {
      if (child->_begin_log_line) {
        const_cast<LogChannel*>(chan)->_backend_impl = child->_impl;
        child->_begin_log_line(chan, str);
      }
    }
  }
}

static void forkContinueLogFn(const LogChannel* chan, const std::string& str) {
  auto impl = chan->_logger->_backend->_impl.tryAs<fork_backend_impl_ptr_t>();
  if (impl) {
    for (auto& child : impl.value()->_children) {
      if (child->_continue_log_line) {
        const_cast<LogChannel*>(chan)->_backend_impl = child->_impl;
        child->_continue_log_line(chan, str);
      }
    }
  }
}

static void forkEndLogFn(const LogChannel* chan, const std::string& str) {
  auto impl = chan->_logger->_backend->_impl.tryAs<fork_backend_impl_ptr_t>();
  if (impl) {
    for (auto& child : impl.value()->_children) {
      if (child->_end_log_line) {
        const_cast<LogChannel*>(chan)->_backend_impl = child->_impl;
        child->_end_log_line(chan, str);
      }
    }
  }
}

static void forkWarnFn(const LogChannel* chan, const std::string& str) {
  auto impl = chan->_logger->_backend->_impl.tryAs<fork_backend_impl_ptr_t>();
  if (impl) {
    for (auto& child : impl.value()->_children) {
      if (child->_warn) {
        const_cast<LogChannel*>(chan)->_backend_impl = child->_impl;
        child->_warn(chan, str);
      }
    }
  }
}

static void forkErrorFn(const LogChannel* chan, const std::string& str) {
  auto impl = chan->_logger->_backend->_impl.tryAs<fork_backend_impl_ptr_t>();
  if (impl) {
    for (auto& child : impl.value()->_children) {
      if (child->_error) {
        const_cast<LogChannel*>(chan)->_backend_impl = child->_impl;
        child->_error(chan, str);
      }
    }
  }
}

static void forkStatusFn(const LogChannel* chan, std::string subchannel, const std::string& str) {
  auto impl = chan->_logger->_backend->_impl.tryAs<fork_backend_impl_ptr_t>();
  if (impl) {
    for (auto& child : impl.value()->_children) {
      if (child->_status) {
        const_cast<LogChannel*>(chan)->_backend_impl = child->_impl;
        child->_status(chan, subchannel, str);
      }
    }
  }
}

static void forkPerfItemFn(const LogChannel* chan, std::string name, svar64_t& data) {
  auto impl = chan->_logger->_backend->_impl.tryAs<fork_backend_impl_ptr_t>();
  if (impl) {
    for (auto& child : impl.value()->_children) {
      if (child->_on_perf_item) {
        const_cast<LogChannel*>(chan)->_backend_impl = child->_impl;
        child->_on_perf_item(chan, name, data);
      }
    }
  }
}

////////////////////////////////////////////////////////////////
// Factory functions
////////////////////////////////////////////////////////////////

logger_backend_ptr_t createForkBackend(std::vector<logger_backend_ptr_t> children) {
  auto backend = std::make_shared<LoggerBackend>();
  auto impl = std::make_shared<ForkBackendImpl>();

  for (auto& child : children) {
    impl->addChild(child);
  }

  backend->_impl = impl;
  backend->_add_log_line = forkLogFn;
  backend->_begin_log_line = forkBeginLogFn;
  backend->_continue_log_line = forkContinueLogFn;
  backend->_end_log_line = forkEndLogFn;
  backend->_warn = forkWarnFn;
  backend->_error = forkErrorFn;
  backend->_status = forkStatusFn;
  backend->_on_perf_item = forkPerfItemFn;

  return backend;
}

logger_backend_ptr_t createForkBackend() {
  return createForkBackend({});
}

void forkBackendAddChild(logger_backend_ptr_t fork_backend, logger_backend_ptr_t child) {
  auto impl = fork_backend->_impl.tryAs<fork_backend_impl_ptr_t>();
  if (impl) {
    impl.value()->addChild(child);
  }
}

////////////////////////////////////////////////////////////////
} // namespace ork
