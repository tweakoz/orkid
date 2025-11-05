////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#import <Foundation/Foundation.h>

// Test function signature
typedef void (*OrkTestFunction)(void);

// Test info structure
@interface OrkTestInfo : NSObject
@property (nonatomic, strong) NSString* name;
@property (nonatomic, strong) NSString* testDescription;
@property (nonatomic, assign) OrkTestFunction testFunction;

+ (instancetype)testWithName:(NSString*)name description:(NSString*)desc function:(OrkTestFunction)func;
@end

// Test registry
@interface OrkTests : NSObject
+ (NSArray<OrkTestInfo*>*)availableTests;
@end

// Individual test declarations
void runMathTests(void);
void runDataflowTests(void);
void runKernelTests(void);
void runCurlTests(void);
void runLZ4Tests(void);
void runRapidJSONTests(void);
void runNlohmannTests(void);
void runZmqTests(void);
void runTarTests(void);
void runHashTests(void);
