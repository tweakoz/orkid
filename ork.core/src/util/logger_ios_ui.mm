////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#if defined(ORK_IOS)

#include <ork/util/logger.h>
#include <ork/math/cvector3.h>
#import <UIKit/UIKit.h>
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <memory>

// Make ork types visible to Objective-C++
using ork::fvec3;
using ork::LogChannel;

// Forward declarations (must be at global scope)
@class OrkLogChannelViewController;
@class OrkLoggerTabBarController;

// Empty namespace block removed - types moved to after Objective-C declarations

////////////////////////////////////////////////////////////////
// Objective-C++ View Controller for a single log channel
// (Objective-C must be at global scope)
////////////////////////////////////////////////////////////////

@interface OrkLogChannelViewController : UIViewController
@property (nonatomic, strong) UITextView* textView;
@property (nonatomic, strong) NSMutableString* logBuffer;
@property (nonatomic, assign) fvec3 channelColor;
- (void)appendLogLine:(NSString*)line;
- (void)clearLog;
@end

@implementation OrkLogChannelViewController

- (instancetype)init {
  self = [super init];
  if (self) {
    _logBuffer = [[NSMutableString alloc] init];
    _channelColor = fvec3(1.0f, 1.0f, 1.0f);
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Create text view for log output
  _textView = [[UITextView alloc] initWithFrame:self.view.bounds];
  _textView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  _textView.editable = NO;
  _textView.font = [UIFont fontWithName:@"Menlo" size:12.0f];

  // Set background color based on channel color (darkened)
  UIColor* bgColor = [UIColor colorWithRed:_channelColor.x * 0.1f
                                     green:_channelColor.y * 0.1f
                                      blue:_channelColor.z * 0.1f
                                     alpha:1.0f];
  _textView.backgroundColor = bgColor;

  // Set text color based on channel color
  UIColor* textColor = [UIColor colorWithRed:_channelColor.x
                                       green:_channelColor.y
                                        blue:_channelColor.z
                                       alpha:1.0f];
  _textView.textColor = textColor;

  [self.view addSubview:_textView];

  // Apply existing log buffer
  _textView.text = _logBuffer;
}

- (void)appendLogLine:(NSString*)line {
  [_logBuffer appendString:line];
  [_logBuffer appendString:@"\n"];

  // Update UI on main thread
  if (_textView) {
    _textView.text = _logBuffer;

    // Auto-scroll to bottom
    if (_logBuffer.length > 0) {
      NSRange bottom = NSMakeRange(_logBuffer.length - 1, 1);
      [_textView scrollRangeToVisible:bottom];
    }
  }
}

- (void)clearLog {
  [_logBuffer setString:@""];
  if (_textView) {
    _textView.text = @"";
  }
}

@end

////////////////////////////////////////////////////////////////
// Log Channel Manager
////////////////////////////////////////////////////////////////

@interface OrkLogChannelManager : NSObject
@property (nonatomic, assign) void* mainViewController; // Store as void* to avoid ARC issues
@property (nonatomic, strong) NSMutableDictionary<NSString*, OrkLogChannelViewController*>* channelViewControllers;
- (OrkLogChannelViewController*)getOrCreateChannelViewController:(NSString*)channelName withColor:(fvec3)color;
@end

@implementation OrkLogChannelManager

- (instancetype)init {
  self = [super init];
  if (self) {
    _channelViewControllers = [[NSMutableDictionary alloc] init];
    _mainViewController = nil;
  }
  return self;
}

- (OrkLogChannelViewController*)getOrCreateChannelViewController:(NSString*)channelName withColor:(fvec3)color {
  OrkLogChannelViewController* vc = _channelViewControllers[channelName];

  if (!vc) {
    vc = [[OrkLogChannelViewController alloc] init];
    vc.channelColor = color;
    vc.title = channelName;

    // Store in dictionary
    _channelViewControllers[channelName] = vc;

    // Notify main view controller on main thread
    if (_mainViewController) {
      id mainVC = (__bridge id)_mainViewController;
      dispatch_async(dispatch_get_main_queue(), ^{
        UIColor* uiColor = [UIColor colorWithRed:color.x green:color.y blue:color.z alpha:1.0f];

        // Call via NSInvocation to avoid forward declaration issues
        SEL selector = NSSelectorFromString(@"addLogChannelButton:withColor:viewController:");
        if ([mainVC respondsToSelector:selector]) {
          NSMethodSignature *signature = [mainVC methodSignatureForSelector:selector];
          NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:signature];
          [invocation setSelector:selector];
          [invocation setTarget:mainVC];
          // Arguments start at index 2 (0 is self, 1 is _cmd)
          [invocation setArgument:(void*)&channelName atIndex:2];
          [invocation setArgument:(void*)&uiColor atIndex:3];
          [invocation setArgument:(void*)&vc atIndex:4];
          [invocation invoke];
        }
      });
    }
  }

  return vc;
}

@end

////////////////////////////////////////////////////////////////
namespace ork {
////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
// Per-channel data structure (C++ in namespace)
////////////////////////////////////////////////////////////////

struct IOSChannelImpl {
  OrkLogChannelViewController* __unsafe_unretained view_controller = nil;
  std::mutex _mutex;

  ~IOSChannelImpl() {
    view_controller = nil;
  }
};

using ios_channel_impl_ptr_t = std::shared_ptr<IOSChannelImpl>;

////////////////////////////////////////////////////////////////
// iOS Logger UI Manager
////////////////////////////////////////////////////////////////

struct IOSLoggerUI {

  ////////////////////////////////////////////

  IOSLoggerUI() {
    // Create channel manager on main thread
    if ([NSThread isMainThread]) {
      _channel_manager = [[OrkLogChannelManager alloc] init];
    } else {
      dispatch_sync(dispatch_get_main_queue(), ^{
        _channel_manager = [[OrkLogChannelManager alloc] init];
      });
    }
  }

  ////////////////////////////////////////////

  ~IOSLoggerUI() {
    if ([NSThread isMainThread]) {
      _channel_manager = nil;
    } else {
      dispatch_async(dispatch_get_main_queue(), ^{
        _channel_manager = nil;
      });
    }
  }

  ////////////////////////////////////////////

  void setMainViewController(void* mainVC) {
    _channel_manager.mainViewController = mainVC;
  }

  ////////////////////////////////////////////

  void _add_log_line(const LogChannel* channel, const std::string& line) {
    auto impl = _implForChannel(channel);

    // Convert to NSString
    NSString* nsLine = [NSString stringWithUTF8String:line.c_str()];
    NSString* channelName = [NSString stringWithUTF8String:channel->_name.c_str()];

    // Marshal to main thread
    dispatch_async(dispatch_get_main_queue(), ^{
      OrkLogChannelViewController* vc = [_channel_manager getOrCreateChannelViewController:channelName
                                                                                  withColor:channel->_color];
      [vc appendLogLine:nsLine];

      // Highlight the channel button in main view
      if (_channel_manager.mainViewController) {
        id mainVC = (__bridge id)_channel_manager.mainViewController;
        SEL selector = NSSelectorFromString(@"highlightLogChannel:");
        if ([mainVC respondsToSelector:selector]) {
          [mainVC performSelector:selector withObject:channelName];
        }
      }

      // Store reference in impl if not already set
      if (!impl->view_controller) {
        impl->view_controller = vc;
      }
    });
  }

  ////////////////////////////////////////////

  ios_channel_impl_ptr_t _implForChannel(const LogChannel* channel) {
    ios_channel_impl_ptr_t rval;
    OrkAssert(channel != nullptr);
    if (auto as_impl = channel->_backend_impl.tryAs<ios_channel_impl_ptr_t>()) {
      rval = as_impl.value();
    } else {
      rval = channel->_backend_impl.makeShared<IOSChannelImpl>();
    }
    return rval;
  }

  ////////////////////////////////////////////

  bool isReady() const {
    return _channel_manager != nil;
  }

  ////////////////////////////////////////////

  OrkLogChannelManager* __strong _channel_manager;
  LoggerBackend* _backend = nullptr;
};

////////////////////////////////////////////////////////////////
// Singleton and function pointers
////////////////////////////////////////////////////////////////

static std::shared_ptr<IOSLoggerUI> ios_logger_ui() {
  static std::shared_ptr<IOSLoggerUI> instance = std::make_shared<IOSLoggerUI>();
  return instance->isReady() ? instance : nullptr;
}

////////////////////////////////////////////////////////////////

static void IOSAddLogFn(const LogChannel* channel, const std::string& str) {
  auto ui = ios_logger_ui();
  if (ui) {
    std::lock_guard<std::mutex> lock(ui->_backend->_mutex);
    ui->_add_log_line(channel, str);
  }
}

////////////////////////////////////////////////////////////////

static void IOSBeginLogFn(const LogChannel* channel, const std::string& str) {
  IOSAddLogFn(channel, str);
}

////////////////////////////////////////////////////////////////

static void IOSContinueLogFn(const LogChannel* channel, const std::string& str) {
  IOSAddLogFn(channel, str);
}

////////////////////////////////////////////////////////////////

static void IOSEndLogFn(const LogChannel* channel, const std::string& str) {
  IOSAddLogFn(channel, str);
}

////////////////////////////////////////////////////////////////

static void IOSWarnFn(const LogChannel* channel, const std::string& str) {
  std::string warn_str = "⚠️ WARNING: " + str;
  IOSAddLogFn(channel, warn_str);
}

////////////////////////////////////////////////////////////////

static void IOSErrorFn(const LogChannel* channel, const std::string& str) {
  std::string error_str = "❌ ERROR: " + str;
  IOSAddLogFn(channel, error_str);
}

////////////////////////////////////////////////////////////////

static void IOSStatusFn(const LogChannel* channel, std::string subchannel, const std::string& str) {
  std::string status_str = "[" + subchannel + "] " + str;
  IOSAddLogFn(channel, status_str);
}

////////////////////////////////////////////////////////////////

static void IOSPerfItemFn(const LogChannel* channel, std::string perfitem, svar64_t& dd) {
  std::string perf_str = "[PERF:" + perfitem + "] ";

  if (auto as_int = dd.tryAs<int>()) {
    perf_str += std::to_string(as_int.value());
  } else if (auto as_double = dd.tryAs<double>()) {
    perf_str += std::to_string(as_double.value());
  } else if (auto as_float = dd.tryAs<float>()) {
    perf_str += std::to_string(as_float.value());
  } else if (auto as_string = dd.tryAs<std::string>()) {
    perf_str += as_string.value();
  }

  IOSAddLogFn(channel, perf_str);
}

////////////////////////////////////////////////////////////////

void installIOSUIToBackend(LoggerBackend* backend) {
  std::lock_guard<std::mutex> lock(backend->_mutex);
  auto ui = ios_logger_ui();
  if (ui) {
    ui->_backend = backend;
    backend->_add_log_line      = IOSAddLogFn;
    backend->_begin_log_line    = IOSBeginLogFn;
    backend->_continue_log_line = IOSContinueLogFn;
    backend->_end_log_line      = IOSEndLogFn;
    backend->_warn              = IOSWarnFn;
    backend->_error             = IOSErrorFn;
    backend->_status            = IOSStatusFn;
    backend->_on_perf_item      = IOSPerfItemFn;
  }
}

////////////////////////////////////////////////////////////////

void setIOSUIMainViewController(void* mainVC) {
  auto ui = ios_logger_ui();
  if (ui) {
    ui->setMainViewController(mainVC);
  }
}

////////////////////////////////////////////////////////////////
} // namespace ork
#endif // ORK_IOS
