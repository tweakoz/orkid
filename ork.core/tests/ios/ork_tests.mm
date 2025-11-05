////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#import <ork/test/ork_tests.h>

@implementation OrkTestInfo

+ (instancetype)testWithName:(NSString*)name description:(NSString*)desc function:(OrkTestFunction)func {
    OrkTestInfo* info = [[OrkTestInfo alloc] init];
    info.name = name;
    info.testDescription = desc;
    info.testFunction = func;
    return info;
}

@end

@implementation OrkTests

+ (NSArray<OrkTestInfo*>*)availableTests {
    return @[
        [OrkTestInfo testWithName:@"Math Tests"
                      description:@"Test vector, matrix, and quaternion operations"
                         function:runMathTests],
        [OrkTestInfo testWithName:@"Dataflow Tests"
                      description:@"Test dataflow graph execution and modules"
                         function:runDataflowTests],
        [OrkTestInfo testWithName:@"Kernel Tests"
                      description:@"Test operation queues, strings, and utilities"
                         function:runKernelTests],
        [OrkTestInfo testWithName:@"Curl Tests"
                      description:@"Test HTTP requests with libcurl"
                         function:runCurlTests],
        [OrkTestInfo testWithName:@"LZ4 Tests"
                      description:@"Test LZ4 compression and decompression"
                         function:runLZ4Tests],
        [OrkTestInfo testWithName:@"RapidJSON Tests"
                      description:@"Test JSON parsing and serialization"
                         function:runRapidJSONTests],
        [OrkTestInfo testWithName:@"Nlohmann Tests"
                      description:@"Test nlohmann/json parsing and serialization"
                         function:runNlohmannTests],
        [OrkTestInfo testWithName:@"ZeroMQ Tests"
                      description:@"Test ZeroMQ messaging patterns (TCP and inproc)"
                         function:runZmqTests],
        [OrkTestInfo testWithName:@"TAR Tests"
                      description:@"Test libtar archive creation and extraction"
                         function:runTarTests],
        [OrkTestInfo testWithName:@"Hash Tests"
                      description:@"Test MD5, CRC32, CRC64, XXHash64, and XXHash3"
                         function:runHashTests],
        [OrkTestInfo testWithName:@"Crypto Tests"
                      description:@"Test ChaCha20-Poly1305 encryption (libsodium)"
                         function:runCryptoTests],
        [OrkTestInfo testWithName:@"OPQ Tests"
                      description:@"Test operation queue parallelism and throughput"
                         function:runOpqTests],
        [OrkTestInfo testWithName:@"Catalog Tests"
                      description:@"Test asset catalog initialization with bundled manifests"
                         function:runCatalogTests],
    ];
}

@end
