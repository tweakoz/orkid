#pragma once
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <ApplicationServices/ApplicationServices.h>
#import <CoreGraphics/CoreGraphics.h>
#import <MetalKit/MetalKit.h>
#import <objc/runtime.h>
#include <vector>
#include <string>

#ifdef __METAL_VERSION__
#define NS_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
#define NSInteger metal::int32_t
#else
#import <Foundation/Foundation.h>
#endif

#include <simd/simd.h>

////////////////////////////////////////////////////////////////////////////////
template <typename T> inline T* objc_cast(id instance){
  if([instance isKindOfClass:[T class]]) {
    auto casted = (T*) instance;
    return casted;
  }
  else
  return nullptr;
}
////////////////////////////////////////////////////////////////////////////////
inline void objc_list_selectors(id instance){
  int i=0;
  unsigned int mc = 0;
  Method * mlist = class_copyMethodList(object_getClass(instance), &mc);
  NSLog(@"%d methods", mc);
  for(i=0;i<mc;i++)
    NSLog(@"Method no #%d: %s", i, sel_getName(method_getName(mlist[i])));
}
////////////////////////////////////////////////////////////////////////////////
inline std::string objc_typename(id instance){
  auto typeofinstance = NSStringFromClass([instance class]);
  return std::string([typeofinstance UTF8String]);
}
////////////////////////////////////////////////////////////////////////////////
inline SEL selector(const char* named){
  auto selname = [NSString stringWithUTF8String:named];
  SEL _selector = NSSelectorFromString(selname);
  return _selector;
}
////////////////////////////////////////////////////////////////////////////////
@interface AppDelegate : NSObject <NSApplicationDelegate>
  @property (assign) IBOutlet NSWindow *window;
@end
////////////////////////////////////////////////////////////////////////////////
@interface Renderer : NSObject <MTKViewDelegate>
  -(nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view;
@end
////////////////////////////////////////////////////////////////////////////////
@interface GameViewController : NSViewController

@end
////////////////////////////////////////////////////////////////////////////////
typedef NS_ENUM(NSInteger, BufferIndex)
{
  BufferIndexMeshPositions = 0,
  BufferIndexMeshGenerics  = 1,
  BufferIndexUniforms      = 2
};
////////////////////////////////////////////////////////////////////////////////
typedef NS_ENUM(NSInteger, VertexAttribute)
{
  VertexAttributePosition  = 0,
  VertexAttributeTexcoord  = 1,
};
////////////////////////////////////////////////////////////////////////////////
typedef NS_ENUM(NSInteger, TextureIndex)
{
  TextureIndexColor    = 0,
};
////////////////////////////////////////////////////////////////////////////////
typedef struct
{
  matrix_float4x4 projectionMatrix;
  matrix_float4x4 modelViewMatrix;
} Uniforms;
////////////////////////////////////////////////////////////////////////////////


