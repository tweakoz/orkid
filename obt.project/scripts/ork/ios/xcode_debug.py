################################################################
# Orkid Media Engine
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License.
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################
# iOS Xcode Debugger Integration
# Create Xcode workspace for debugging iOS apps
################################################################

import os
import shutil
import xml.etree.ElementTree as ET
from xml.dom import minidom
from pathlib import Path

def xml_prettify(elem):
    """Return a pretty-printed XML string for the Element."""
    rough_string = ET.tostring(elem, 'utf-8')
    reparsed = minidom.parseString(rough_string)
    return reparsed.toprettyxml(indent="  ")

def create_ios_xcode_workspace(
    workspace_path,
    app_bundle_path,
    bundle_id,
    simulator_udid,
    env_vars=None,
    working_dir=None
):
    """Create an Xcode workspace with minimal project for debugging iOS app

    Args:
        workspace_path: Path where .xcworkspace will be created
        app_bundle_path: Path to .app bundle
        bundle_id: App bundle identifier (e.g., "com.tweakoz.orkid.test")
        simulator_udid: Simulator UDID to run on
        env_vars: Dictionary of environment variables (optional)
        working_dir: Custom working directory (optional)
    """
    workspace_path = Path(workspace_path)
    app_bundle_path = Path(app_bundle_path)

    if not app_bundle_path.exists():
        raise FileNotFoundError(f"App bundle not found: {app_bundle_path}")

    # Remove existing workspace/project if it exists
    if workspace_path.exists():
        shutil.rmtree(workspace_path)

    # Create a minimal .xcodeproj (iOS needs this for destination discovery)
    project_name = app_bundle_path.stem
    project_path = workspace_path.parent / f"{project_name}.xcodeproj"

    if project_path.exists():
        shutil.rmtree(project_path)

    project_path.mkdir(parents=True, exist_ok=True)

    # Create minimal project.pbxproj
    # This tells Xcode it's an iOS app project for simulator
    pbxproj_content = f"""// !$*UTF8*$!
{{
	archiveVersion = 1;
	classes = {{
	}};
	objectVersion = 56;
	objects = {{
		A1A1A1A1A1A1A1A1A1A1A1A1 /* {app_bundle_path.name} */ = {{isa = PBXFileReference; explicitFileType = wrapper.application; path = "{app_bundle_path.absolute()}"; sourceTree = "<absolute>"; }};
		B1B1B1B1B1B1B1B1B1B1B1B1 /* {project_name} */ = {{isa = PBXNativeTarget; buildConfigurationList = D1D1D1D1D1D1D1D1D1D1D1D1; buildPhases = (); buildRules = (); dependencies = (); name = "{project_name}"; productName = "{project_name}"; productReference = A1A1A1A1A1A1A1A1A1A1A1A1; productType = "com.apple.product-type.application"; }};
		C1C1C1C1C1C1C1C1C1C1C1C1 /* Project */ = {{isa = PBXProject; buildConfigurationList = E1E1E1E1E1E1E1E1E1E1E1E1; compatibilityVersion = "Xcode 14.0"; developmentRegion = en; hasScannedForEncodings = 0; knownRegions = (en, Base); mainGroup = F1F1F1F1F1F1F1F1F1F1F1F1; productRefGroup = F1F1F1F1F1F1F1F1F1F1F1F1; projectDirPath = ""; projectRoot = ""; targets = (B1B1B1B1B1B1B1B1B1B1B1B1); }};
		D1D1D1D1D1D1D1D1D1D1D1D1 /* Build configuration list for PBXNativeTarget "{project_name}" */ = {{isa = XCConfigurationList; buildConfigurations = (D2D2D2D2D2D2D2D2D2D2D2D2); defaultConfigurationIsVisible = 0; defaultConfigurationName = Debug; }};
		D2D2D2D2D2D2D2D2D2D2D2D2 /* Debug */ = {{isa = XCBuildConfiguration; buildSettings = {{CODE_SIGN_IDENTITY = "-"; CODE_SIGN_STYLE = Manual; DEPLOYMENT_POSTPROCESSING = NO; INFOPLIST_FILE = "{app_bundle_path.absolute()}/Info.plist"; IPHONEOS_DEPLOYMENT_TARGET = 18.0; PRODUCT_BUNDLE_IDENTIFIER = "{bundle_id}"; PRODUCT_NAME = "$(TARGET_NAME)"; SDKROOT = iphonesimulator; SKIP_INSTALL = YES; SUPPORTED_PLATFORMS = "iphonesimulator"; SUPPORTS_MACCATALYST = NO; TARGETED_DEVICE_FAMILY = "1,2"; }}; name = Debug; }};
		E1E1E1E1E1E1E1E1E1E1E1E1 /* Build configuration list for PBXProject "Project" */ = {{isa = XCConfigurationList; buildConfigurations = (E2E2E2E2E2E2E2E2E2E2E2E2); defaultConfigurationIsVisible = 0; defaultConfigurationName = Debug; }};
		E2E2E2E2E2E2E2E2E2E2E2E2 /* Debug */ = {{isa = XCBuildConfiguration; buildSettings = {{IPHONEOS_DEPLOYMENT_TARGET = 18.0; SDKROOT = iphonesimulator; SUPPORTED_PLATFORMS = "iphonesimulator"; SUPPORTS_MACCATALYST = NO; }}; name = Debug; }};
		F1F1F1F1F1F1F1F1F1F1F1F1 /* Main Group */ = {{isa = PBXGroup; children = (A1A1A1A1A1A1A1A1A1A1A1A1); sourceTree = "<group>"; }};
	}};
	rootObject = C1C1C1C1C1C1C1C1C1C1C1C1;
}}
"""

    pbxproj_path = project_path / "project.pbxproj"
    with open(pbxproj_path, 'w') as f:
        f.write(pbxproj_content)

    # Create workspace that references the project
    workspace_path.mkdir(parents=True, exist_ok=True)
    workspace_content = ET.Element("Workspace", version="1.0")
    file_ref = ET.SubElement(workspace_content, "FileRef", location=f"container:{project_path.absolute()}")

    workspace_file = workspace_path / 'contents.xcworkspacedata'
    with open(workspace_file, 'w') as f:
        f.write(xml_prettify(workspace_content))

    # Create directories for scheme
    shared_data_path = project_path / "xcshareddata"
    shared_schemes_path = shared_data_path / "xcschemes"

    for directory in [shared_data_path, shared_schemes_path]:
        directory.mkdir(parents=True, exist_ok=True)

    # Generate .xcscheme file
    scheme_name = project_name
    scheme_file = shared_schemes_path / f'{scheme_name}.xcscheme'
    scheme_content = ET.Element("Scheme", LastUpgradeVersion="1500", version="1.7")

    # BuildAction - required for Xcode to recognize iOS destinations
    build_action = ET.SubElement(scheme_content, "BuildAction", parallelizeBuildables="YES", buildImplicitDependencies="YES")
    build_action_entries = ET.SubElement(build_action, "BuildActionEntries")
    build_action_entry = ET.SubElement(
        build_action_entries,
        "BuildActionEntry",
        buildForTesting="NO",
        buildForRunning="YES",  # Back to YES so destinations work
        buildForProfiling="YES",
        buildForArchiving="YES",
        buildForAnalyzing="YES"
    )
    buildable_reference = ET.SubElement(
        build_action_entry,
        "BuildableReference",
        BuildableIdentifier="primary",
        BlueprintIdentifier="B1B1B1B1B1B1B1B1B1B1B1B1",
        BuildableName=app_bundle_path.name,
        BlueprintName=project_name,
        ReferencedContainer=f"container:{project_path.name}"
    )

    # LaunchAction
    launch_action = ET.SubElement(
        scheme_content,
        "LaunchAction",
        buildConfiguration="Debug",
        selectedDebuggerIdentifier="Xcode.DebuggerFoundation.Debugger.LLDB",
        selectedLauncherIdentifier="Xcode.DebuggerFoundation.Launcher.LLDB",
        launchStyle="0",
        useCustomWorkingDirectory="YES" if working_dir else "NO",
        ignoresPersistentStateOnLaunch="NO",
        debugDocumentVersioning="YES",
        debugServiceExtension="internal",
        allowLocationSimulation="YES"
    )

    if working_dir:
        launch_action.set('customWorkingDirectory', str(working_dir))

    # PathRunnable - point directly to prebuilt .app (like macOS version)
    path_runnable = ET.SubElement(
        launch_action,
        "PathRunnable",
        runnableDebuggingMode="0",
        FilePath=str(app_bundle_path.absolute())
    )

    # Environment variables (same as macOS version)
    if env_vars:
        env_vars_elem = ET.SubElement(launch_action, "EnvironmentVariables")
        for key, value in env_vars.items():
            if value:
                ET.SubElement(
                    env_vars_elem,
                    "EnvironmentVariable",
                    key=key,
                    value=str(value),
                    isEnabled="YES"
                )

    # Add CommandLineArguments (even if empty, for proper structure)
    cmd_line_args = ET.SubElement(launch_action, "CommandLineArguments")

    # ProfileAction
    profile_action = ET.SubElement(
        scheme_content,
        "ProfileAction",
        buildConfiguration="Debug",
        shouldUseLaunchSchemeArgsEnv="YES",
        savedToolIdentifier="",
        useCustomWorkingDirectory="YES" if working_dir else "NO",
        debugDocumentVersioning="YES"
    )

    if working_dir:
        profile_action.set('customWorkingDirectory', str(working_dir))

    profile_path_runnable = ET.SubElement(
        profile_action,
        "PathRunnable",
        runnableDebuggingMode="0",
        FilePath=str(app_bundle_path.absolute())
    )

    # Write scheme file
    with open(scheme_file, 'w') as f:
        f.write(xml_prettify(scheme_content))

    print(f"\n✓ Created Xcode workspace: {workspace_path}")
    print(f"  App: {app_bundle_path}")
    print(f"  Bundle ID: {bundle_id}")
    print(f"  Simulator: {simulator_udid}")

    # Boot simulator if not already booted
    print(f"\nBooting simulator {simulator_udid}...")
    boot_result = os.system(f"xcrun simctl boot {simulator_udid} 2>/dev/null")
    if boot_result == 0:
        print("Simulator booted")

    # Wait a moment for simulator to be ready
    import time
    time.sleep(2)

    # Install app to simulator first (so Xcode can find it)
    print(f"Installing app to simulator...")
    install_result = os.system(f"xcrun simctl install {simulator_udid} {app_bundle_path}")

    if install_result != 0:
        print(f"Warning: App installation may have failed")

    print(f"\nOpening Xcode...")
    print(f"Expected simulator: {simulator_udid}")
    print(f"\nIn Xcode:")
    print(f"  1. Select your simulator from the destination picker (top bar)")
    print(f"  2. Press Command-R to run with debugger")

    # Open the workspace in Xcode
    os.system(f"open {workspace_path}")
