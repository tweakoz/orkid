////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/logger.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

///////////////////////////////////////////////////////////////////////////////

StderrRedirector::StderrRedirector(LogChannel* logchan)
    : _logchan(logchan) {
}

///////////////////////////////////////////////////////////////////////////////

StderrRedirector::~StderrRedirector() {
  stop();
}

///////////////////////////////////////////////////////////////////////////////

void StderrRedirector::start() {
  // Get or create STDERR channel

  // Create pipe
  if (pipe(_pipe_fd) == -1) {
    return;
  }

  // Make read end non-blocking
  int flags = fcntl(_pipe_fd[0], F_GETFL, 0);
  fcntl(_pipe_fd[0], F_SETFL, flags | O_NONBLOCK);

  // Save original stderr
  _original_stderr = dup(STDERR_FILENO);

  // Redirect stderr to pipe
  dup2(_pipe_fd[1], STDERR_FILENO);

  // Start reader thread
  _running       = true;
  _reader_thread = std::thread([this]() { this->read_loop(); });
}

///////////////////////////////////////////////////////////////////////////////

void StderrRedirector::stop() {
  if (_running) {
    _running = false;

    // Restore original stderr
    dup2(_original_stderr, STDERR_FILENO);
    close(_original_stderr);

    // Close pipe
    close(_pipe_fd[1]);

    if (_reader_thread.joinable()) {
      _reader_thread.join();
    }

    close(_pipe_fd[0]);
  }
}

///////////////////////////////////////////////////////////////////////////////

void StderrRedirector::read_loop() {
  char buffer[4096];
  std::string line_buffer;
  while (_running) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(_pipe_fd[0], &read_fds);

    struct timeval timeout;
    timeout.tv_sec  = 0;
    timeout.tv_usec = 100000; // 100ms

    int ret = ::select(_pipe_fd[0] + 1, &read_fds, nullptr, nullptr, &timeout);

    if (ret > 0 && FD_ISSET(_pipe_fd[0], &read_fds)) {
      ssize_t bytes_read = read(_pipe_fd[0], buffer, sizeof(buffer) - 1);

      if (bytes_read > 0) {
        buffer[bytes_read] = '\0';

        // Process buffer, looking for newlines
        for (ssize_t i = 0; i < bytes_read; ++i) {
          if (buffer[i] == '\n') {
            // Log the complete line
            if (_logchan && !line_buffer.empty()) {
              _logchan->log("%s", line_buffer.c_str());
            }
            line_buffer.clear();
          } else if (buffer[i] != '\r') { // Skip carriage returns
            line_buffer += buffer[i];
          }
        }
      } else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
        break; // Error occurred
      }
    }
  }

  // Log any remaining content
  if (!line_buffer.empty() && _logchan) {
    _logchan->log("%s", line_buffer.c_str());
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork