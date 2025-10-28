#!/usr/bin/env ork.python
"""
Font Browser 2 - View system monospace fonts with optional SDF rendering
"""

import sys
import json
import signal
import argparse
from pathlib import Path
import freetype

# Import OBT utilities
from obt import path as obt_path

# Import Orkid
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2

# Import ork font utilities
from ork import font as ork_font
from ork.font.atlas import generate_sdf_atlas, save_atlas

tokens = CrcStringProxy()

# Import font atlas generator for FontAtlasGenerator class
sys.path.insert(0, str(Path(__file__).parent))
import importlib.util
spec = importlib.util.spec_from_file_location("atlasgen", Path(__file__).parent / "ork.font.atlasgen.py")
atlasgen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(atlasgen)
FontAtlasGenerator = atlasgen.FontAtlasGenerator

################################################################################

def list_monospace_fonts():
    """List only monospace fonts from system directories"""
    # Use ork.font module to list
    search_dirs = ['/Library/Fonts', '/System/Library/Fonts']
    ork_font.list_monospace_fonts(search_dirs, verbose=True)

################################################################################

def find_font_by_name(font_name):
    """Find a font by family name"""
    search_dirs = ['/Library/Fonts', '/System/Library/Fonts']
    return ork_font.find_font_by_name(font_name, search_dirs, monospace_only=True)

################################################################################

def generate_and_register_fonts(font_path, font_family, sizes, ssaa=4, use_sdf=False):
    """Generate atlases for all sizes and register with FontManager"""

    # Create temp directory
    temp_dir = obt_path.stage() / "tempdir" / "fonttemp"
    temp_dir.mkdir(parents=True, exist_ok=True)

    mode_str = "SDF" if use_sdf else "Bitmap"
    print(f"\nGenerating {mode_str} font atlases:")
    print(f"  Font: {font_family}")
    print(f"  Sizes: {sizes}")
    if not use_sdf:
        print(f"  SSAA: {ssaa}x")
    print(f"  Output: {temp_dir}\n")

    registered_fonts = []

    for size in sizes:
        font_id = f"{font_family}{size}"

        print(f"  Generating {font_id}...", end='', flush=True)

        try:
            if use_sdf:
                # Generate SDF atlas
                atlas_img, metadata = generate_sdf_atlas(font_path, size)

                # Save using lev2.Image
                output_path = temp_dir / font_id
                atlas_img.writeToFile(str(output_path) + ".png")

                # Save metadata JSON
                json_path = temp_dir / f"{font_id}.json"
                with open(json_path, 'w') as f:
                    json.dump(metadata, f, indent=2)

            else:
                # Generate bitmap atlas (original method)
                gen = FontAtlasGenerator(str(font_path), pixel_size=size, dpi=96)
                atlas, metadata = gen.generate_f2i_style_atlas(grid_size=16, ssaa=ssaa)

                # Save to temp directory
                output_path = temp_dir / font_id
                save_atlas(atlas, metadata, str(output_path), add_grid=False)

            # Load metadata
            json_path = temp_dir / f"{font_id}.json"
            with open(json_path) as f:
                meta = json.load(f)

            # Create and populate FontDesc
            desc = lev2.FontDesc()
            ork_font.populate_fontdesc_from_metadata(
                desc, meta, font_id, str(output_path), size
            )

            # Register with FontManager
            lev2.FontManager.addFont(desc)

            registered_fonts.append(font_id)
            print(" ✓")

        except Exception as e:
            print(f" ✗ ({e})")
            import traceback
            traceback.print_exc()

    print(f"\nRegistered {len(registered_fonts)} fonts\n")
    return registered_fonts

################################################################################

class FontBrowser:
    """Font browser application"""

    def __init__(self, font_name, sizes, ssaa, use_sdf):
        self.font_name = font_name
        self.sizes = sizes

        # Find font
        print(f"Searching for font: {font_name}")
        font_info = find_font_by_name(font_name)

        if not font_info:
            print(f"Error: Font '{font_name}' not found")
            print("Use --list to see available monospace fonts")
            sys.exit(1)

        print(f"Found: {font_info['family']} {font_info['style']}")
        print(f"Path: {font_info['path']}")

        # Generate and register fonts
        self.registered_fonts = generate_and_register_fonts(
            font_info['path'],
            font_info['family'].replace(" ", ""),  # Remove spaces for font ID
            sizes,
            ssaa,
            use_sdf
        )

        # Create UI
        self.ezapp = lev2.OrkEzApp.create(
            self,
            left=100,
            top=100,
            width=1400,
            height=1000
        )

        self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
        self.ezapp.topWidget.enableUiDraw()

        lg_group = self.ezapp.topLayoutGroup
        lg_group.clearColorGuide = vec4(0.2, 0.2, 0.25, 1)

        MARGIN = 6

        # Create grid based on number of sizes
        num_sizes = len(self.registered_fonts)
        grid_width = min(5, num_sizes)
        grid_height = (num_sizes + grid_width - 1) // grid_width

        self.griditems = lg_group.makeGrid(
            width=grid_width,
            height=grid_height,
            margin=MARGIN,
            uiclass=lev2.ui.TextBox,
            args=["label", vec4(.5, .5, .5, 1), "Sample"],
        )

        # Assign fonts to grid items
        for idx, font_id in enumerate(self.registered_fonts):
            if idx >= len(self.griditems):
                break

            font = lev2.FontManager.fontForId(font_id)
            if font:
                gitem = self.griditems[idx]
                gitem.widget.font = font

                # Extract size from font_id
                size_str = ''.join(c for c in font_id if c.isdigit())

                # Sample text: max 10 chars per line, 6 lines
                sample_text = f"Font  {size_str}pt\nABCDEFGHIJ\nabcdefghij\n0123456789\n!@#$%^&*()\n<>[]{{}}+-="
                gitem.widget.setText(sample_text)
                gitem.widget.valign = tokens.CENTER
                gitem.widget.halign = tokens.CENTER_ALL

        self.lg_group = lg_group
        lg_group.margin = MARGIN

        def onCtrlC(signum, frame):
            print("Exiting font browser...")
            self.ezapp.signalExit()

        signal.signal(signal.SIGINT, onCtrlC)

    def onGpuInit(self, ctx):
        pass

    def onUpdate(self, updinfo):
        pass

    def onUiEvent(self, uievent):
        return lev2.ui.HandlerResult()

################################################################################

def main():
    parser = argparse.ArgumentParser(
        description='Browse and view system monospace fonts (with optional SDF rendering)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --list                                # List all monospace fonts
  %(prog)s --show Inconsolata                    # Show Inconsolata font (bitmap)
  %(prog)s --show Inconsolata --sdf              # Show Inconsolata font (SDF)
  %(prog)s --show Monaco --sizes 14,18,24,32     # Custom sizes
  %(prog)s --show "JetBrains Mono" --ssaa 16     # Bitmap with 4x SSAA
        """
    )

    parser.add_argument('--list', action='store_true',
                       help='List all monospace fonts and exit')
    parser.add_argument('--show', type=str,
                       help='Font family name to display')
    parser.add_argument('--sizes', type=str,
                       help='Comma-separated list of even sizes (default: 12-40 even)')
    parser.add_argument('--ssaa', type=int, choices=[1, 4, 9, 16, 25], default=25,
                       help='SSAA level for bitmap mode: 1=off, 4=2x, 9=3x, 16=4x, 25=5x (default: 25)')
    parser.add_argument('--sdf', '-S', action='store_true',
                       help='Use SDF rendering instead of bitmap (ignores --ssaa)')

    args = parser.parse_args()

    # Handle --list
    if args.list:
        list_monospace_fonts()
        return

    # Handle --show
    if args.show:
        # Parse sizes
        if args.sizes:
            try:
                sizes = [int(s.strip()) for s in args.sizes.split(',')]
                # Validate even sizes
                for size in sizes:
                    if size % 2 != 0:
                        print(f"Warning: Size {size} is odd, font rendering works best with even sizes")
            except ValueError:
                print("Error: Invalid sizes format. Use comma-separated numbers like: 14,18,24,32")
                sys.exit(1)
        else:
            # Default: even sizes from 12 to 40
            sizes = list(range(12, 42, 2))

        # Create and run browser
        browser = FontBrowser(args.show, sizes, args.ssaa, args.sdf)
        browser.ezapp.mainThreadLoop()
    else:
        parser.print_help()

################################################################################

if __name__ == "__main__":
    main()
