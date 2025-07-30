# CDN Test Directory Setup

The Orkid Development CDN has been modified to use a dedicated test directory for serving content.

## Changes Made

1. **Content Directory**: The CDN now serves files from `obt.path.stage()/cdntest` instead of `ork.data`
   - This provides a controlled, isolated environment for CDN testing
   - Prevents accidental serving of the entire ork.data directory

2. **Automatic Test File Setup**: When the CDN is launched, it automatically:
   - Creates the `cdntest` directory if it doesn't exist
   - Copies a selection of test files from `ork.data` including:
     - Small bank files (CZ1, TX81Z) - 1-2KB
     - Medium database files - ~10KB
     - Shader files - 5-8KB
     - Texture files (if available)
     - Python scripts
   - Creates a `manifest.json` listing all copied files with sizes and descriptions

3. **Test Files Included**:
   - Banks: `banks/cz1_magicbell.bnk`, `banks/tx81z_pianobell.bnk`
   - Data: `data/gfxthumb64.db`
   - Shaders: `shaders/tanspace.glfx`, `shaders/gbuffer_skinned.glfx`
   - Textures: `textures/frogs_diffuse.dds` (if exists)
   - Scripts: `scripts/orkutil.py`

## Usage

1. Launch the CDN as usual:
   ```bash
   obt.docker.py --manifest ork-devcdn --launch
   ```

2. The CDN will:
   - Create `$OBT_STAGE/cdntest` directory (automatically)
   - Copy test files from ork.data using wildcard patterns
   - Start serving from the cdntest directory

## Directory Structure

- `$OBT_STAGE/cdntest/` - CDN content directory (created by CDN launch)
  - `banks/cz1/*.bnk` - CZ1 bank files
  - `banks/tx81z/*.bnk` - TX81Z bank files  
  - `shaders/*.glfx` - Shader files
  - `textures/*.dds` - Texture files
  - `scripts/*.py` - Python scripts
  - `manifest.json` - File listing with sizes

3. Test scripts have been updated to use the stage directory:
   - `test_cdn_api.py` - Tests CDN API endpoints
   - `test_actual_upload.py` - Tests file upload functionality
   - `test_cdn_cdntest.py` - Verifies cdntest directory setup

## Benefits

- **Isolation**: CDN only serves designated test files, not entire ork.data
- **Reproducibility**: Same set of test files every time
- **Performance**: Smaller directory to serve = faster startup
- **Safety**: No risk of accidentally exposing sensitive files
- **Flexibility**: Easy to add/remove test files by modifying the list

## File Manifest

After launching, check `$OBT_STAGE/cdntest/manifest.json` to see exactly which files were copied and their sizes.