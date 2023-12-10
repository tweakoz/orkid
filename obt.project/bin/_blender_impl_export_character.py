# to be executed from ork.blender.export.character.py

from obt.path import Path
from ork import blender
import os

inp_file = Path(os.environ["BLENDER_FILE_PATH"])
out_file = Path(os.environ["EXPORT_PATH"])

blender.export_character_mesh(inp_file, out_file)
