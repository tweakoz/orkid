#!/usr/bin/env python3
################################################################################
# Orkid Asset Catalog Import Module
# Generalized asset import for packaging files into encrypted asset_paks
################################################################################

import os
import json
import fnmatch
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional, Union, List, Tuple

from orkengine import core
from ork import assets

#############################################
# Data Classes
#############################################

@dataclass
class AssetDefinition:
    """Explicit asset definition (one pak with specific files)."""
    id: str
    include: Union[str, List[str]]
    exclude: List[str] = field(default_factory=list)

    def __post_init__(self):
        # Normalize include to list
        if isinstance(self.include, str):
            self.include = [self.include]

@dataclass
class ImportConfig:
    """Complete import configuration."""
    namespace: str
    source_dir: str
    local_loc: str
    manifest: str

    # Optional with defaults
    encryption_key: Optional[str] = None
    platforms: List[str] = field(default_factory=lambda: ["mac", "linux"])
    priority: int = 0

    # Asset definitions
    assets: List[AssetDefinition] = field(default_factory=list)

@dataclass
class ImportResult:
    """Result of import operation."""
    success_count: int = 0
    failed_count: int = 0
    skipped_count: int = 0
    assets: List[Tuple[str, str]] = field(default_factory=list)  # (namespace, asset_id)
    errors: List[str] = field(default_factory=list)

#############################################
# Variable Resolution
#############################################

def resolve_variables(value: str, env: dict = None) -> str:
    """Expand ${VAR} and <stage>, <temp> patterns in a string."""
    if value is None:
        return None

    if env is None:
        env = os.environ

    result = value

    # Expand <stage> and <temp>
    from obt import path as obt_path
    result = result.replace("<stage>", str(obt_path.stage()))
    result = result.replace("<temp>", str(obt_path.temp()))

    # Expand ${VAR} patterns
    def replace_var(match):
        var_name = match.group(1)
        return env.get(var_name, match.group(0))  # Keep original if not found

    result = re.sub(r'\$\{([^}]+)\}', replace_var, result)

    return result

#############################################
# Config Loading
#############################################

def load_config(path: str) -> ImportConfig:
    """Load ImportConfig from JSON file with variable expansion."""
    with open(path, 'r') as f:
        data = json.load(f)

    # Build assets list if present
    assets_list = []
    if 'assets' in data:
        for asset_data in data['assets']:
            assets_list.append(AssetDefinition(
                id=asset_data['id'],
                include=asset_data['include'],
                exclude=asset_data.get('exclude', [])
            ))

    return ImportConfig(
        namespace=data['namespace'],
        source_dir=data['source_dir'],
        local_loc=data['local_loc'],
        manifest=data['manifest'],
        encryption_key=data.get('encryption_key'),
        platforms=data.get('platforms', ['mac', 'linux']),
        priority=data.get('priority', 0),
        assets=assets_list
    )

def config_from_args(args) -> ImportConfig:
    """Build ImportConfig from argparse namespace."""
    assets_list = []
    if args.asset:
        for asset_id, pattern in args.asset:
            assets_list.append(AssetDefinition(
                id=asset_id,
                include=pattern,
                exclude=args.exclude or []
            ))

    return ImportConfig(
        namespace=args.namespace,
        source_dir=args.source_dir,
        local_loc=args.local_loc,
        manifest=args.manifest,
        encryption_key=args.key,
        platforms=args.platforms,
        priority=0,
        assets=assets_list
    )

def export_config(config: ImportConfig, path: str):
    """Export ImportConfig to JSON file."""
    data = {
        "namespace": config.namespace,
        "source_dir": config.source_dir,
        "local_loc": config.local_loc,
        "manifest": config.manifest,
    }

    # Optional fields (only if non-default)
    if config.encryption_key:
        data["encryption_key"] = config.encryption_key
    if config.platforms != ["mac", "linux"]:
        data["platforms"] = config.platforms
    if config.priority != 0:
        data["priority"] = config.priority

    # Assets
    if config.assets:
        data["assets"] = []
        for a in config.assets:
            asset_data = {"id": a.id, "include": a.include}
            if a.exclude:
                asset_data["exclude"] = a.exclude
            data["assets"].append(asset_data)

    with open(path, 'w') as f:
        json.dump(data, f, indent=2)
        f.write('\n')

#############################################
# File Matching
#############################################

def match_patterns(file_path: Path, patterns: List[str], base_dir: Path) -> bool:
    """Check if file matches any of the glob patterns."""
    rel_path = str(file_path.relative_to(base_dir))

    for pattern in patterns:
        if fnmatch.fnmatch(rel_path, pattern):
            return True
        # Also try matching just the filename
        if fnmatch.fnmatch(file_path.name, pattern):
            return True

    return False

def enumerate_files(base_dir: Path, patterns: List[str],
                    exclude: List[str] = None, recursive: bool = False) -> List[Path]:
    """Enumerate files matching patterns."""
    if exclude is None:
        exclude = []

    matched_files = []

    for pattern in patterns:
        # Use glob for pattern matching
        if recursive or '**' in pattern:
            glob_pattern = pattern if '**' in pattern else f"**/{pattern}"
            files = list(base_dir.glob(glob_pattern))
        else:
            files = list(base_dir.glob(pattern))

        for f in files:
            if f.is_file():
                # Check exclusions
                rel_path = str(f.relative_to(base_dir))
                excluded = False
                for ex_pattern in exclude:
                    if fnmatch.fnmatch(rel_path, ex_pattern) or fnmatch.fnmatch(f.name, ex_pattern):
                        excluded = True
                        break

                if not excluded and f not in matched_files:
                    matched_files.append(f)

    return sorted(matched_files)

#############################################
# Namespace Validation
#############################################

def validate_namespace(namespace: str) -> Tuple[bool, str]:
    """
    Check if namespace exists in config.json.
    Returns (is_valid, error_message).
    """
    try:
        # Get the catalog instance to check config
        catalog = core.AssetCatalog.instance
        cfgspc = catalog.config_space

        # Check if namespace has a remote location configured
        remote_loc = cfgspc.getNamespaceRemoteLocation(namespace)
        if not remote_loc:
            return False, (
                f"Namespace '{namespace}' not found in any config.json in $ORKID_ASSET_MANIFEST_DIRS.\n"
                f"Please add namespace configuration to your config.json:\n"
                f"{{\n"
                f"  \"namespaces\": {{\n"
                f"    \"{namespace}\": {{\n"
                f"      \"encryption_key\": \"your_key\",\n"
                f"      \"remote_location\": \"your_location\"\n"
                f"    }}\n"
                f"  }}\n"
                f"}}"
            )
        return True, ""
    except Exception as e:
        return False, f"Error validating namespace '{namespace}': {e}"

#############################################
# Core Importer
#############################################

class AssetImporter:
    """Main importer class."""

    def __init__(self, config: ImportConfig):
        self.config = config
        self._resolved_source_dir = None
        self._resolved_local_loc = None
        self._resolved_manifest = None
        self._resolved_key = None

    def validate_config(self) -> Tuple[bool, List[str]]:
        """Validate configuration before running. Returns (is_valid, errors)."""
        errors = []

        # Check namespace exists in config.json
        is_valid, error_msg = validate_namespace(self.config.namespace)
        if not is_valid:
            errors.append(error_msg)

        return len(errors) == 0, errors

    def resolve_paths(self):
        """Resolve all path variables in config."""
        self._resolved_source_dir = Path(resolve_variables(self.config.source_dir))
        self._resolved_local_loc = resolve_variables(self.config.local_loc)
        self._resolved_manifest = Path(resolve_variables(self.config.manifest))

        # Resolve encryption key
        if self.config.encryption_key:
            self._resolved_key = resolve_variables(self.config.encryption_key)
        else:
            # Try default environment variable
            env_key = f"ORKID_KEY_{self.config.namespace.upper()}"
            self._resolved_key = os.environ.get(env_key)

    def get_asset_definitions(self) -> List[AssetDefinition]:
        """Get list of assets to import."""
        if not self.config.assets:
            raise ValueError("Config must have 'assets' list")
        return self.config.assets

    def enumerate_asset_files(self, asset_def: AssetDefinition) -> List[Path]:
        """Get files matching an asset definition's include/exclude."""
        return enumerate_files(
            self._resolved_source_dir,
            asset_def.include if isinstance(asset_def.include, list) else [asset_def.include],
            asset_def.exclude,
            recursive=False
        )

    def list_assets(self) -> List[Tuple[str, List[Path]]]:
        """List all assets and their files (for --list mode)."""
        self.resolve_paths()

        result = []
        for asset_def in self.get_asset_definitions():
            files = self.enumerate_asset_files(asset_def)
            result.append((asset_def.id, files))

        return result

    def package_asset(self, asset_def: AssetDefinition, verbose: bool = False) -> bool:
        """Package a single asset definition into a pak."""
        try:
            # Get the include patterns as a list
            filters = asset_def.include if isinstance(asset_def.include, list) else [asset_def.include]

            if verbose:
                print(f"  Source dir: {self._resolved_source_dir}")
                print(f"  Filters: {filters}")
                print(f"  Local loc: {self._resolved_local_loc}")

            result = assets.build_assetpak(
                namespace=self.config.namespace,
                output=str(self._resolved_manifest),
                asset_id=asset_def.id,
                source_dir=str(self._resolved_source_dir),
                filters=filters,
                priority=self.config.priority,
                local_loc=self.config.local_loc,  # Use unexpanded path for portability
                key=self._resolved_key,
                platforms=self.config.platforms,
                write_manifest=True
            )

            if verbose:
                print(f"  Storage hash: {result.get('storage_hash', 'N/A')}")
                print(f"  Content hash: {result.get('content_hash', 'N/A')}")

            return True

        except Exception as e:
            print(f"  ✗ Error packaging {asset_def.id}: {e}")
            return False

    def package_all(self, verbose: bool = False) -> ImportResult:
        """Package all assets."""
        self.resolve_paths()

        # Ensure manifest directory exists
        self._resolved_manifest.parent.mkdir(parents=True, exist_ok=True)

        result = ImportResult()
        asset_defs = self.get_asset_definitions()

        print(f"Packaging {len(asset_defs)} asset(s) to namespace '{self.config.namespace}'")
        print(f"Manifest: {self._resolved_manifest}")
        print("=" * 60)

        for i, asset_def in enumerate(asset_defs, 1):
            print(f"\n[{i}/{len(asset_defs)}] {asset_def.id}")

            if self.package_asset(asset_def, verbose=verbose):
                result.success_count += 1
                result.assets.append((self.config.namespace, asset_def.id))
                print(f"  ✓ Packaged successfully")
            else:
                result.failed_count += 1
                result.errors.append(f"Failed to package {asset_def.id}")

        print("\n" + "=" * 60)
        print(f"Packaging complete: {result.success_count} succeeded, {result.failed_count} failed")

        return result

    def upload_assets(self, result: ImportResult, verbose: bool = False) -> ImportResult:
        """Upload packaged assets to CDN."""
        if not result.assets:
            print("No assets to upload")
            return result

        print(f"\nUploading {len(result.assets)} asset(s) to CDN...")
        print("=" * 60)

        # Get catalog instance
        catalog = core.AssetCatalog.instance

        upload_success = 0
        upload_failed = 0

        for i, (namespace, asset_id) in enumerate(result.assets, 1):
            fqid = f"{namespace}|{asset_id}"
            print(f"\n[{i}/{len(result.assets)}] Uploading {fqid}...")

            try:
                receipt = catalog.uploadAsset(fqid)
                if receipt and receipt.success:
                    print(f"  ✓ Uploaded successfully")
                    upload_success += 1
                else:
                    print(f"  ✗ Upload failed")
                    upload_failed += 1
                    result.errors.append(f"Upload failed: {fqid}")
            except Exception as e:
                print(f"  ✗ Upload error: {e}")
                upload_failed += 1
                result.errors.append(f"Upload error for {fqid}: {e}")

        print("\n" + "=" * 60)
        print(f"Upload complete: {upload_success} succeeded, {upload_failed} failed")

        return result

    def run(self, upload: bool = False, dry_run: bool = False,
            list_only: bool = False, verbose: bool = False) -> ImportResult:
        """Execute full import pipeline."""
        self.resolve_paths()

        # Validate source directory exists
        if not self._resolved_source_dir.exists():
            print(f"Error: Source directory does not exist: {self._resolved_source_dir}")
            return ImportResult(failed_count=1, errors=["Source directory not found"])

        # Validate namespace configuration (skip for list-only mode)
        if not list_only:
            is_valid, errors = self.validate_config()
            if not is_valid:
                print("Error: Configuration validation failed:")
                for error in errors:
                    print(f"\n{error}")
                return ImportResult(failed_count=1, errors=errors)

        # List mode
        if list_only:
            print(f"Assets in namespace '{self.config.namespace}':")
            print(f"Source: {self._resolved_source_dir}")
            print("=" * 60)

            total_files = 0
            for asset_id, files in self.list_assets():
                print(f"\n{asset_id}:")
                for f in files:
                    rel_path = f.relative_to(self._resolved_source_dir)
                    print(f"  - {rel_path}")
                    total_files += 1

            print("\n" + "=" * 60)
            print(f"Total: {len(self.get_asset_definitions())} asset(s), {total_files} file(s)")
            return ImportResult()

        # Dry run mode
        if dry_run:
            print("[DRY RUN] Would perform the following actions:")
            print(f"  Namespace: {self.config.namespace}")
            print(f"  Source: {self._resolved_source_dir}")
            print(f"  Local loc: {self._resolved_local_loc}")
            print(f"  Manifest: {self._resolved_manifest}")
            print(f"  Platforms: {', '.join(self.config.platforms)}")
            print(f"  Encryption: {'Yes' if self._resolved_key else 'No'}")
            print()

            asset_defs = self.get_asset_definitions()
            print(f"Would package {len(asset_defs)} asset(s):")
            for asset_def in asset_defs:
                files = self.enumerate_asset_files(asset_def)
                print(f"  - {asset_def.id} ({len(files)} file(s))")

            if upload:
                print(f"\nWould upload {len(asset_defs)} asset(s) to CDN")

            return ImportResult()

        # Actual import
        result = self.package_all(verbose=verbose)

        if upload and result.success_count > 0:
            result = self.upload_assets(result, verbose=verbose)

        return result

#############################################
# Entry Point for Programmatic Use
#############################################

def run_import(config: ImportConfig, upload: bool = False, dry_run: bool = False,
               list_only: bool = False, verbose: bool = False) -> ImportResult:
    """Convenience function to run import from config."""
    importer = AssetImporter(config)
    return importer.run(upload=upload, dry_run=dry_run,
                        list_only=list_only, verbose=verbose)
