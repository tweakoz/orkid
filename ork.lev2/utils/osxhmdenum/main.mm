#include "osxhmdenum.inl"

@interface AppDelegate ()
@end

@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // Insert code here to initialize your application
}
- (void)applicationWillTerminate:(NSNotification *)aNotification {
    // Insert code here to tear down your application
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}
@end

////////////////////////////////////////////////////////////////////////////////
int main(int argc, const char * argv[])
{
  ///////////////////////////////////////
  // get HMD device and create a metal session with it
  ///////////////////////////////////////

  auto NSHMDDevice = NSClassFromString(@"NSHMDDevice");
  auto NSHMDMetalSession = NSClassFromString(@"NSHMDMetalSession");
  auto NSSLSHMD = NSClassFromString(@"NSSLSHMD");
  //objc_list_selectors(NSHMDDevice);
  //objc_list_selectors(NSHMDMetalSession);
  //objc_list_selectors(NSSLSHMD);

  printf("NSHMDDevice<%p>\n", NSHMDDevice);
  printf("NSHMDMetalSession<%p>\n", NSHMDMetalSession);
  printf("NSSLSHMD<%p>\n", NSSLSHMD);
  //objc_list_selectors(NSHMDDevice);

  auto hmddevicearray = objc_cast<NSArray>([NSHMDDevice performSelector: selector("devices")]);
  //objc_list_selectors(hmddevicearray);
  printf("hmddevicearray<%p:%s>\n", hmddevicearray, objc_typename(hmddevicearray).c_str());
  int num_hmddevices = int64_t([hmddevicearray performSelector: selector("count")]);
  printf("num_hmddevices<%d>\n", num_hmddevices);
  auto hmddevice = [hmddevicearray objectAtIndex:0];
  printf("hmddevice<%p:%s>\n", hmddevice,objc_typename(hmddevice).c_str());
  NSError* error = nullptr;
  auto mtlsesh = [[NSHMDMetalSession alloc] initWithDevice: hmddevice error: & error];
  printf("mtlsesh<%p:%s>\n", mtlsesh,objc_typename(mtlsesh).c_str());
  auto mtldev = [mtlsesh performSelector: selector("metalDevice")];
  printf("mtldev<%p:%s>\n", mtldev,objc_typename(mtldev).c_str());
    objc_list_selectors(mtldev);
  auto mtldevname = [[mtldev performSelector: selector("name")] UTF8String];
  printf("mtldevname<%s>\n", mtldevname);
  //auto baseobj = [mtldev performSelector: selector("baseObject")];
  //printf("baseobj<%p:%s>\n", baseobj,objc_typename(baseobj).c_str());
  //assert(false);

  ///////////////////////////////////////
  CGError cgret;
  //CGGetDisplaysWithRect

    @autoreleasepool {
        // Setup code that might create autoreleased objects goes here.
    }
    return NSApplicationMain(argc, argv);


}
