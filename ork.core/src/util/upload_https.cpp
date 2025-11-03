////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/upload.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <curl/curl.h>
#include <rapidjson/document.h>
#include <fstream>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <memory>

namespace ork {

// Maximum concurrent uploads (to avoid overwhelming the server)
const int max_concurrent = 2;

//////////////////////////////////////////////////////////////////////////////
// CURL Callbacks for uploads
//////////////////////////////////////////////////////////////////////////////

// CURL callback for reading data to upload
static size_t read_callback(void* buffer, size_t size, size_t nmemb, void* userp) {
  auto* stream = static_cast<std::ifstream*>(userp);
  size_t buffer_size = size * nmemb;
  
  if (stream->eof()) {
    return 0;
  }
  
  stream->read(static_cast<char*>(buffer), buffer_size);
  size_t bytes_read = stream->gcount();
  
  return bytes_read;
}

// CURL callback for writing response data
static size_t write_response_callback(void* contents, size_t size, size_t nmemb, void* userp) {
  // For uploads, we typically don't need the response body
  // but we need to consume it to avoid CURL errors
  if (userp) {
    // If userp is provided, it's a string* for capturing response
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), size * nmemb);
  }
  return size * nmemb;
}

// CURL callback for upload progress
static int upload_progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
                                    curl_off_t ultotal, curl_off_t ulnow) {
  auto* uploader = static_cast<HttpsUploader*>(clientp);
  
  if (uploader->isCancelled()) {
    return 1;  // Abort transfer
  }
  
  // The HttpsUploader will handle progress internally through a public method
  if (ultotal > 0) {
    uploader->handleUploadProgress(static_cast<size_t>(ulnow), static_cast<size_t>(ultotal));
  }
  
  return 0;  // Continue
}


////////////////////////////////////////////////////////////////////////////////
// HttpsUploader
////////////////////////////////////////////////////////////////////////////////

struct HttpsUploader::Impl {
  CURL* _curl_handle = nullptr;
  struct curl_slist* _headers = nullptr;
  
  // Progress tracking
  size_t _last_uploaded_bytes = 0;
  Timer _upload_timer;
  double _last_upload_rate = 0.0;
  
  // Performance metrics
  Timer _perf_timer;
  std::atomic<size_t> _bytes_this_second{0};
  std::atomic<size_t> _total_bytes_uploaded{0};
  std::atomic<size_t> _pending_bytes{0};  // Bytes currently being uploaded
  std::atomic<int> _active_uploads{0};
  std::atomic<int> _completed_uploads{0};
  std::atomic<int> _failed_uploads{0};
  
  void emitPerfMetrics(logchannel_ptr_t logchan) {
    double elapsed = _perf_timer.SecsSinceStart();
    if (elapsed >= 2.0f) {
      // Calculate bytes per second over the interval
      size_t bytes_this_interval = _bytes_this_second.exchange(0);
      float bytes_per_sec = bytes_this_interval / elapsed;
      
      logchan->log(
          "UL:totMiB<%g> pendMiB<%g> MiB/sec<%g> Active<%d> Completed<%d> Failed<%d>",
          float(_total_bytes_uploaded) / 1048576.0f,
          float(_pending_bytes) / 1048576.0f,
          float(bytes_per_sec) / 1048576.0f,
          int(_active_uploads.load()),
          int(_completed_uploads.load()),
          int(_failed_uploads.load()));
      
      _perf_timer.Start();
    }
  }
  
  ~Impl() {
    if (_curl_handle) {
      curl_easy_cleanup(_curl_handle);
    }
    if (_headers) {
      curl_slist_free_all(_headers);
    }
  }
};

HttpsUploader::HttpsUploader(httpsuploaderconfig_ptr_t config)
    : _config(config) {
  _impl = std::make_unique<Impl>();
  _log_channel = logger()->configureChannel("HTTPSUPLOAD",fvec3(1,.7,1),true);
}

HttpsUploader::~HttpsUploader() = default;

bool HttpsUploader::uploadFiles(
    const std::vector<file::Path>& local_files,
    const std::vector<std::string>& remote_paths) {
  
  if (local_files.size() != remote_paths.size()) {
    _log_channel->log("ERROR: Mismatched file and path counts");
    return false;
  }
  
  if (local_files.empty()) {
    return true; // Nothing to upload
  }
  
  // Calculate total bytes to upload upfront
  size_t total_bytes_to_upload = 0;
  for (const auto& path : local_files) {
    if (path.doesPathExist()) {
      File file(path, EFM_READ);
      size_t file_size = 0;
      file.GetLength(file_size);
      file.Close();
      total_bytes_to_upload += file_size;
    }
  }
  
  // Set pending bytes to total upfront
  _impl->_pending_bytes = total_bytes_to_upload;
  _log_channel->log("Starting concurrent upload of %zu chunks (%.2f MiB total)",
                    local_files.size(), 
                    float(total_bytes_to_upload) / 1048576.0f);
  
  // Use CURL multi interface for concurrent uploads
  CURLM* multi_handle = curl_multi_init();
  if (!multi_handle) {
    _log_channel->log("ERROR: Failed to create CURL multi handle");
    _impl->_pending_bytes = 0;  // Clear on error
    return false;
  }
  
  struct UploadContext {
    CURL* curl = nullptr;
    struct curl_slist* headers = nullptr;
    std::ifstream* file_stream = nullptr;
    std::string response;
    file::Path local_path;
    std::string remote_path;
    size_t file_size = 0;
    size_t last_progress = 0;  // Track last reported progress for this upload
    size_t current_progress = 0;  // Current upload progress
    int retry_count = 0;
    long http_code = 0;
    bool completed = false;
    bool success = false;
    HttpsUploader* uploader = nullptr;  // Reference to uploader for progress tracking
  };
  
  std::vector<std::unique_ptr<UploadContext>> contexts;
  contexts.reserve(local_files.size());
  
  size_t next_file_index = 0;
  int active_transfers = 0;
  
  // Helper lambda to setup and add a transfer
  auto add_transfer = [&](size_t file_index) -> bool {
    if (file_index >= local_files.size()) {
      return false;
    }
    
    auto ctx = std::make_unique<UploadContext>();
    ctx->local_path = local_files[file_index];
    ctx->remote_path = remote_paths[file_index];
    ctx->uploader = this;  // Store reference for progress callback
    
    // Check if file exists
    if (!ctx->local_path.doesPathExist()) {
      _log_channel->log("ERROR: File doesn't exist: %s", ctx->local_path.c_str());
      return false;
    }
    
    // Open file
    ctx->file_stream = new std::ifstream(ctx->local_path.c_str(), std::ios::binary);
    if (!ctx->file_stream->is_open()) {
      _log_channel->log("ERROR: Cannot open file: %s", ctx->local_path.c_str());
      delete ctx->file_stream;
      return false;
    }
    
    // Get file size
    ctx->file_stream->seekg(0, std::ios::end);
    ctx->file_size = ctx->file_stream->tellg();
    ctx->file_stream->seekg(0, std::ios::beg);
    
    // Create CURL handle
    ctx->curl = curl_easy_init();
    if (!ctx->curl) {
      delete ctx->file_stream;
      return false;
    }
    
    // Build URL
    std::string protocol = _config->protocol();
    std::string base_path = _config->remote_base_path;
    if (!base_path.empty() && !base_path.starts_with("/")) {
      base_path = "/" + base_path;
    }
    
    std::string full_path = base_path;
    if (!full_path.empty() && !full_path.ends_with("/")) {
      full_path += "/";
    }
    full_path += ctx->remote_path;
    
    std::string full_url = FormatString("%s://%s:%d%s",
      protocol.c_str(),
      _config->host.c_str(),
      _config->port,
      full_path.c_str());
    
    if(1)_log_channel->log("Uploading multi %s to %s", ctx->local_path.c_str(), full_url.c_str());
    
    // Setup CURL options
    curl_easy_setopt(ctx->curl, CURLOPT_URL, full_url.c_str());
    curl_easy_setopt(ctx->curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(ctx->curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(ctx->curl, CURLOPT_READDATA, ctx->file_stream);
    curl_easy_setopt(ctx->curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)ctx->file_size);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, write_response_callback);
    curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, &ctx->response);
    curl_easy_setopt(ctx->curl, CURLOPT_PRIVATE, ctx.get());
    
    // Set up progress callback for real-time byte tracking
    curl_easy_setopt(ctx->curl, CURLOPT_XFERINFOFUNCTION, 
      +[](void* clientp, curl_off_t dltotal, curl_off_t dlnow, 
          curl_off_t ultotal, curl_off_t ulnow) -> int {
        auto* ctx = static_cast<UploadContext*>(clientp);
        if (!ctx || !ctx->uploader) return 0;
        
        // Track bytes uploaded for bandwidth calculation and total
        if (ulnow > ctx->last_progress) {
          size_t bytes_delta = ulnow - ctx->last_progress;
          ctx->uploader->_impl->_bytes_this_second += bytes_delta;
          ctx->uploader->_impl->_total_bytes_uploaded += bytes_delta;
          ctx->uploader->_impl->_pending_bytes -= bytes_delta;  // Decrease pending as bytes are uploaded
          ctx->last_progress = ulnow;
        }
        ctx->current_progress = ulnow;
        return 0;
      });
    curl_easy_setopt(ctx->curl, CURLOPT_XFERINFODATA, ctx.get());
    curl_easy_setopt(ctx->curl, CURLOPT_NOPROGRESS, 0L);
    
    // Disable SSL verification if configured
    if (!_config->verify_ssl) {
      curl_easy_setopt(ctx->curl, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(ctx->curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }
    
    // Setup headers
    ctx->headers = nullptr;
    if (!_config->api_key.empty()) {
      std::string auth_header = "X-API-Key: " + _config->api_key;
      ctx->headers = curl_slist_append(ctx->headers, auth_header.c_str());
    }
    
    ctx->headers = curl_slist_append(ctx->headers, "Content-Type: application/octet-stream");
    curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, ctx->headers);
    
    // Add to multi handle
    CURLMcode mc = curl_multi_add_handle(multi_handle, ctx->curl);
    if (mc != CURLM_OK) {
      _log_channel->log("ERROR: Failed to add handle to multi: %s", curl_multi_strerror(mc));
      curl_easy_cleanup(ctx->curl);
      curl_slist_free_all(ctx->headers);
      delete ctx->file_stream;
      return false;
    }
    
    // Don't add to pending here - already counted upfront
    contexts.push_back(std::move(ctx));
    active_transfers++;
    return true;
  };
  
  // Add initial batch of transfers
  while (active_transfers < max_concurrent && next_file_index < local_files.size()) {
    if (!add_transfer(next_file_index)) {
      // Failed to add transfer, but continue with others
      _log_channel->log("WARNING: Skipping file index %zu", next_file_index);
    }
    next_file_index++;
  }
  
  // Process transfers
  int still_running = 0;
  std::vector<size_t> retry_indices; // Files that need retry due to rate limiting
  Timer upload_timer;
  upload_timer.Start();
  _impl->_perf_timer.Start();
  _impl->_bytes_this_second = 0;
  
  // Track initial active uploads
  _impl->_active_uploads = active_transfers;
  
  do {
    CURLMcode mc = curl_multi_perform(multi_handle, &still_running);
    
    if (mc != CURLM_OK) {
      _log_channel->log("ERROR: curl_multi_perform failed: %s", curl_multi_strerror(mc));
      break;
    }
    
    // Check for completed transfers
    int msgs_left;
    CURLMsg* msg;
    while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
      if (msg->msg == CURLMSG_DONE) {
        CURL* easy = msg->easy_handle;
        UploadContext* ctx = nullptr;
        curl_easy_getinfo(easy, CURLINFO_PRIVATE, &ctx);
        
        if (ctx) {
          // Get HTTP response code
          curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &ctx->http_code);
          
          // Track total bytes uploaded (already tracked in real-time via progress callback)
          // Total bytes already tracked in progress callback
          // Just get the final amount for verification if needed
          double ul_bytes;
          curl_easy_getinfo(easy, CURLINFO_SIZE_UPLOAD, &ul_bytes);
          
          if (msg->data.result == CURLE_OK) {
            if (ctx->http_code >= 200 && ctx->http_code < 300) {
              ctx->success = true;
              ctx->completed = true;
              _impl->_completed_uploads++;
              
              // Calculate upload rate
              double total_time;
              curl_easy_getinfo(easy, CURLINFO_TOTAL_TIME, &total_time);
              double upload_rate = (total_time > 0) ? (ul_bytes / total_time / 1048576.0) : 0;
              
              if(0)_log_channel->log("Upload successful: %s (HTTP %ld) - %.2f MiB in %.2fs (%.2f MiB/s)", 
                               ctx->remote_path.c_str(), ctx->http_code,
                               ul_bytes / 1048576.0, total_time, upload_rate);
            } else if (ctx->http_code == 429 || ctx->http_code == 503) {
              // Rate limit or service unavailable - retry
              if (ctx->retry_count < 3) {
                ctx->retry_count++;
                _log_channel->log("Rate limited (HTTP %ld), will retry %s (attempt %d/3)", 
                                 ctx->http_code, ctx->remote_path.c_str(), ctx->retry_count);
                
                // Don't re-add to pending - it's already counted
                
                // Find this context's index for retry
                for (size_t i = 0; i < contexts.size(); i++) {
                  if (contexts[i].get() == ctx) {
                    retry_indices.push_back(i);
                    break;
                  }
                }
              } else {
                ctx->completed = true;
                _impl->_failed_uploads++;
                _log_channel->log("ERROR: Max retries exceeded for %s", ctx->remote_path.c_str());
              }
            } else {
              ctx->completed = true;
              _impl->_failed_uploads++;
              _log_channel->log("ERROR: Upload failed for %s (HTTP %ld)", 
                               ctx->remote_path.c_str(), ctx->http_code);
            }
          } else {
            ctx->completed = true;
            _impl->_failed_uploads++;
            _log_channel->log("ERROR: CURL error for %s: %s", 
                             ctx->remote_path.c_str(), curl_easy_strerror(msg->data.result));
          }
          
          // Remove from multi handle
          curl_multi_remove_handle(multi_handle, easy);
          active_transfers--;
          
          // Add next file if available
          if (next_file_index < local_files.size()) {
            if (add_transfer(next_file_index)) {
              next_file_index++;
            }
          }
        }
      }
    }
    
    // Handle retries with exponential backoff
    if (!retry_indices.empty() && active_transfers < max_concurrent) {
      // Wait before retrying (exponential backoff)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      
      for (auto it = retry_indices.begin(); it != retry_indices.end(); ) {
        size_t idx = *it;
        auto& ctx = contexts[idx];
        
        // Wait based on retry count (exponential backoff)
        int wait_ms = (1 << ctx->retry_count) * 500; // 1s, 2s, 4s
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
        
        // Reset file stream and progress counters
        ctx->file_stream->clear();
        ctx->file_stream->seekg(0, std::ios::beg);
        ctx->last_progress = 0;  // Reset progress counter for retry
        ctx->current_progress = 0;  // Reset current progress
        
        // Re-add to multi handle
        CURLMcode mc = curl_multi_add_handle(multi_handle, ctx->curl);
        if (mc == CURLM_OK) {
          active_transfers++;
          it = retry_indices.erase(it);
          _log_channel->log("Retrying upload: %s", ctx->remote_path.c_str());
        } else {
          ++it;
        }
      }
    }
    
    // Update active uploads count
    _impl->_active_uploads = active_transfers;
    
    // Emit performance metrics periodically
    _impl->emitPerfMetrics(_log_channel);
    
    if (still_running) {
      // Wait for activity
      int numfds;
      curl_multi_wait(multi_handle, nullptr, 0, 100, &numfds);
    }
    
  } while (still_running > 0 || !retry_indices.empty());
  
  // Cleanup
  bool all_success = true;
  for (auto& ctx : contexts) {
    if (ctx->curl) {
      curl_multi_remove_handle(multi_handle, ctx->curl);
      curl_easy_cleanup(ctx->curl);
    }
    if (ctx->headers) {
      curl_slist_free_all(ctx->headers);
    }
    if (ctx->file_stream) {
      ctx->file_stream->close();
      delete ctx->file_stream;
    }
    if (!ctx->success) {
      all_success = false;
      _log_channel->log("Failed to upload: %s", ctx->local_path.c_str());
    }
  }
  
  curl_multi_cleanup(multi_handle);
  
  // Emit final metrics
  _impl->_active_uploads = 0;
  _impl->_pending_bytes = 0;  // Clear all pending bytes when batch completes
  _impl->emitPerfMetrics(_log_channel);
  
  // Log final summary
  float total_time = upload_timer.SecsSinceStart();
  float total_mb = _impl->_total_bytes_uploaded / 1048576.0f;
  float avg_rate = (total_time > 0) ? (total_mb / total_time) : 0;
  
  _log_channel->log("Upload batch complete: %d/%zu files successful, %.2f MiB in %.2fs (avg %.2f MiB/s)",
                    _impl->_completed_uploads.load(),
                    local_files.size(),
                    total_mb,
                    total_time,
                    avg_rate);
  
  return all_success;
}

bool HttpsUploader::uploadFile(
    const file::Path& local_file,
    const std::string& remote_path) {
  
  
  if (_cancelled) {
    _log_channel->log("[ERROR] Upload cancelled");
    return false;
  }
  
  // Check if file exists
  if (!local_file.doesPathExist()) {
    _log_channel->log("[ERROR] Local file doesn't exist: '%s'", local_file.c_str());
    return false;
  }
  
  // Get file size
  File file(local_file, EFM_READ);
  size_t file_size = 0;
  file.GetLength(file_size);
  file.Close();
  
  // Open input file
  std::ifstream input_file(local_file.c_str(), std::ios::binary);
  if (!input_file.is_open()) {
    return false;
  }
  
  // Always create a fresh CURL handle for each upload to ensure thread safety
  CURL* curl = curl_easy_init();
  if (!curl) {
    return false;
  }
  
  // Build full URL - always use HTTPS for port 443
  std::string protocol = _config->protocol();
  
  // Ensure proper path construction
  std::string base_path = _config->remote_base_path;
  if (!base_path.empty() && !base_path.starts_with("/")) {
    base_path = "/" + base_path;
  }
  if (!base_path.empty() && !base_path.ends_with("/")) {
    base_path += "/";
  }
  if (base_path.empty()) {
    base_path = "/";
  }
  
  std::string final_remote_path = remote_path;
  if (final_remote_path.starts_with("/")) {
    final_remote_path = final_remote_path.substr(1);
  }
  
  std::string full_url = FormatString("%s://%s:%d%s%s",
    protocol.c_str(),
    _config->host.c_str(),
    _config->port,
    base_path.c_str(),
    final_remote_path.c_str());
  
  
  // Reset for new request
  curl_easy_reset(curl);
  
  // Set URL
  curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
  
  // Set to PUT method
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_PUT, 1L);
  
  // Set file size
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)file_size);
  
  // Set read callback
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
  curl_easy_setopt(curl, CURLOPT_READDATA, &input_file);
  
  // Set write callback to consume response
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, nullptr);
  
  // Set progress callback
  _impl->_last_uploaded_bytes = 0;
  _impl->_upload_timer.Start();
  _impl->_perf_timer.Start();
  _impl->_active_uploads = 1;
  _impl->_pending_bytes = file_size;  // Track pending bytes for single upload
  
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, upload_progress_callback);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  
  // Set timeout
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, _config->timeout_seconds);
  
  // SSL options
  if (!_config->verify_ssl) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  
  // Build headers - create fresh list for each request to avoid thread safety issues
  struct curl_slist* request_headers = nullptr;
  
  // Add API key if present
  if (!_config->api_key.empty()) {
    std::string api_header = "X-API-Key: " + _config->api_key;
    request_headers = curl_slist_append(request_headers, api_header.c_str());
  }
  
  // Add basic auth if present
  if (!_config->username.empty()) {
    curl_easy_setopt(curl, CURLOPT_USERNAME, _config->username.c_str());
    if (!_config->password.empty()) {
      curl_easy_setopt(curl, CURLOPT_PASSWORD, _config->password.c_str());
    }
  }
  
  // Add custom headers
  for (const auto& [key, value] : _config->custom_headers) {
    std::string header = key + ": " + value;
    request_headers = curl_slist_append(request_headers, header.c_str());
  }
  
  if (request_headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, request_headers);
  }
  
  // Log upload start
  if(0)_log_channel->log("Uploading single %s to %s", local_file.c_str(), full_url.c_str());
  
  // Perform the upload
  CURLcode res = curl_easy_perform(curl);
  
  // Close input file
  input_file.close();
  
  // Free headers
  if (request_headers) {
    curl_slist_free_all(request_headers);
  }
  
  // Check result
  bool success = false;
  if (res == CURLE_OK) {
    // Get HTTP response code
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (response_code >= 200 && response_code < 300) {
        if(0)_log_channel->log("Upload successful: HTTP %ld", response_code);
      success = true;
    } else {
      if(response_code!=429){
        _log_channel->log("[ERROR] Upload failed with HTTP %ld", response_code);
      }
        _log_channel->log("Upload failed: HTTP %ld", response_code);
    }
  } else {
      _log_channel->log("Upload failed: %s", curl_easy_strerror(res));
  }
  
  // Clean up the fresh CURL handle
  curl_easy_cleanup(curl);
  
  // Update metrics
  _impl->_active_uploads = 0;
  _impl->_pending_bytes = 0;  // Clear pending bytes
  if (success) {
    _impl->_completed_uploads++;
  } else {
    _impl->_failed_uploads++;
  }
  
  // Emit final metrics for single upload
  _impl->emitPerfMetrics(_log_channel);
  
  return success;
}

bool HttpsUploader::testConnection() {
  // TODO: Test HTTPS connection
  return true;
}

bool HttpsUploader::remoteFileExists(const std::string& remote_path) {
  // Use HEAD request to check if file exists
  CURL* curl = curl_easy_init();
  if (!curl) {
    return false;
  }
  
  // Build full URL - same logic as uploadFile
  std::string protocol = _config->protocol();
  
  // Ensure proper path construction
  std::string base_path = _config->remote_base_path;
  if (!base_path.empty() && !base_path.starts_with("/")) {
    base_path = "/" + base_path;
  }
  if (!base_path.empty() && !base_path.ends_with("/")) {
    base_path += "/";
  }
  if (base_path.empty()) {
    base_path = "/";
  }
  
  std::string final_remote_path = remote_path;
  if (final_remote_path.starts_with("/")) {
    final_remote_path = final_remote_path.substr(1);
  }
  
  std::string full_url = FormatString("%s://%s:%d%s%s",
    protocol.c_str(),
    _config->host.c_str(),
    _config->port,
    base_path.c_str(),
    final_remote_path.c_str());
  
  // Reset for new request
  curl_easy_reset(curl);
  
  // Set URL
  curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
  
  // Use HEAD method
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
  curl_easy_setopt(curl, CURLOPT_HEADER, 0L);
  
  // SSL options
  if (!_config->verify_ssl) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  
  // Build headers
  struct curl_slist* headers = nullptr;
  
  // Add API key if present
  if (!_config->api_key.empty()) {
    std::string api_header = "X-API-Key: " + _config->api_key;
    headers = curl_slist_append(headers, api_header.c_str());
  }
  
  // Add basic auth if present
  if (!_config->username.empty()) {
    curl_easy_setopt(curl, CURLOPT_USERNAME, _config->username.c_str());
    if (!_config->password.empty()) {
      curl_easy_setopt(curl, CURLOPT_PASSWORD, _config->password.c_str());
    }
  }
  
  if (headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
  
  // Perform the request
  CURLcode res = curl_easy_perform(curl);
  
  // Free headers
  if (headers) {
    curl_slist_free_all(headers);
  }
  
  // Check result
  bool exists = false;
  if (res == CURLE_OK) {
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    exists = (response_code == 200);
  }
  
  // Clean up CURL handle
  curl_easy_cleanup(curl);
  
  return exists;
}

bool HttpsUploader::deleteRemoteFile(const std::string& remote_path) {
  // TODO: Delete via DELETE request
  return false;
}

std::vector<std::string> HttpsUploader::listRemoteDirectory(const std::string& path) {
  // Use /api/list endpoint to get all files
  std::vector<std::string> files;
  
  CURL* curl = curl_easy_init();
  if (!curl) {
    return files;
  }
  
  // Build API URL
  std::string protocol = _config->protocol();
  std::string full_url = FormatString("%s://%s:%d/api/list",
    protocol.c_str(),
    _config->host.c_str(),
    _config->port);
  
  // Reset for new request
  curl_easy_reset(curl);
  
  // Set URL
  curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
  
  // Response buffer
  std::string response_buffer;
  
  // Set write callback to capture response
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, 
    +[](void* contents, size_t size, size_t nmemb, std::string* userp) -> size_t {
      size_t total_size = size * nmemb;
      userp->append((char*)contents, total_size);
      return total_size;
    });
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);
  
  // SSL options
  if (!_config->verify_ssl) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  
  // Build headers
  struct curl_slist* headers = nullptr;
  
  // Add API key if present
  if (!_config->api_key.empty()) {
    std::string api_header = "X-API-Key: " + _config->api_key;
    headers = curl_slist_append(headers, api_header.c_str());
  }
  
  if (headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
  
  // Perform the request
  CURLcode res = curl_easy_perform(curl);
  
  // Free headers
  if (headers) {
    curl_slist_free_all(headers);
  }
  
  // Parse response if successful
  if (res == CURLE_OK) {
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    if (response_code == 200 && !response_buffer.empty()) {
      // Parse JSON response
      rapidjson::Document doc;
      doc.Parse(response_buffer.c_str());
      
      if (!doc.HasParseError() && doc.IsObject() && doc.HasMember("files")) {
        const auto& files_array = doc["files"];
        if (files_array.IsArray()) {
          for (rapidjson::SizeType i = 0; i < files_array.Size(); i++) {
            if (files_array[i].IsObject() && files_array[i].HasMember("name")) {
              const auto& name = files_array[i]["name"];
              if (name.IsString()) {
                files.push_back(name.GetString());
              }
            }
          }
        }
      }
    }
  }
  
  // Clean up CURL handle
  curl_easy_cleanup(curl);
  
  return files;
}

void HttpsUploader::setCustomHeaders(const std::map<std::string, std::string>& headers) {
  _config->custom_headers = headers;
}

void HttpsUploader::setEndpointUrl(const URL& url) {
  _config->host = url._host;
  _config->port = url._port;
  _config->remote_base_path = url._path;
}

void HttpsUploader::handleUploadProgress(size_t uploaded, size_t total) {
  // Update bytes tracking for single file uploads
  if (uploaded > _impl->_last_uploaded_bytes) {
    size_t bytes_delta = uploaded - _impl->_last_uploaded_bytes;
    _impl->_bytes_this_second += bytes_delta;
    _impl->_total_bytes_uploaded += bytes_delta;
    _impl->_pending_bytes -= bytes_delta;  // Decrease pending as bytes are uploaded
    _impl->_last_uploaded_bytes = uploaded;
    
    // Update rate tracking periodically
    double elapsed = _impl->_upload_timer.SecsSinceStart();
    if (elapsed > 4.0) {
      _impl->_last_upload_rate = bytes_delta / elapsed;
      _impl->_upload_timer.Start();
    }
  }
  
  // Emit performance metrics periodically
  _impl->emitPerfMetrics(_log_channel);
  
  // Call the progress callback
  if (_progress_callback) {
    _progress_callback(uploaded, total);
  }
}
////////////////////////////////////////////////////////////////////////////////
// HttpsUploader - MD5 and duplicate check methods
////////////////////////////////////////////////////////////////////////////////

std::string HttpsUploader::calculateFileMD5(const file::Path& file_path) {
  // Use system md5sum command for now (cross-platform later)
  std::string cmd = FormatString("md5sum '%s' | cut -d' ' -f1", file_path.c_str());
  
  FILE* pipe = popen(cmd.c_str(), "r");
  if (!pipe) {
    return "";
  }
  
  char buffer[128];
  std::string result;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    result += buffer;
  }
  pclose(pipe);
  
  // Remove trailing whitespace
  result.erase(result.find_last_not_of(" \n\r\t") + 1);
  return result;
}

bool HttpsUploader::remoteFileMatchesLocal(const file::Path& local_file, const std::string& remote_path) {
  // Calculate local file MD5
  std::string local_md5 = calculateFileMD5(local_file);
  if (local_md5.empty()) {
    return false;
  }
  
  // Check remote file info via API
  CURL* curl = curl_easy_init();
  if (!curl) {
    return false;
  }
  
  // Build API URL
  std::string protocol = _config->protocol();
  std::string api_url = FormatString("%s://%s:%d/api/fileinfo/%s",
    protocol.c_str(),
    _config->host.c_str(),
    _config->port,
    remote_path.c_str());
  
  // Response buffer
  std::string response_data;
  
  // Set CURL options
  curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_response_callback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
  
  // SSL options
  if (!_config->verify_ssl) {
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
  }
  
  // Headers
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, ("X-API-Key: " + _config->api_key).c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  
  // Perform request
  CURLcode res = curl_easy_perform(curl);
  
  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  
  if (res != CURLE_OK || http_code != 200) {
    return false;
  }
  
  // Parse JSON response
  try {
    rapidjson::Document doc;
    doc.Parse(response_data.c_str());
    
    if (!doc.IsObject() || !doc.HasMember("md5") || !doc.HasMember("exists")) {
      return false;
    }
    
    std::string remote_md5 = doc["md5"].GetString();
    bool exists = doc["exists"].GetBool();
    
    // Compare MD5 hashes
    bool matches = exists && (local_md5 == remote_md5);
    
    return matches;
    
  } catch (const std::exception& e) {
    return false;
  }
}

} //  namespace ork {
