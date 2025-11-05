// swift-tools-version: 5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "test1",
    platforms: [
        .macOS(.v14)
    ],
    products: [
        .executable(
            name: "test1",
            targets: ["test1"]
        )
    ],
    targets: [
        .executableTarget(
            name: "test1",
            dependencies: [],
            path: "Sources"
        )
    ]
)
