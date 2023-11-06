import hou
from obt import path, command

# Start a new Houdini session
hou.hipFile.clear(suppress_save_prompt=True)

# Create a geometry node and a cube inside it
geo = hou.node('/obj').createNode('geo', 'my_geo')
cube = geo.createNode('box')

# Create the output network if it doesn't exist
out = hou.node('/out')
if not out:
    out = hou.node('/').createNode('out')

# Add your custom ROP node to the 'out' network
my_rop = out.createNode('rop_test', 'my_test_rop')
output_file_path = path.temp()/"dump.rop"
print(output_file_path)
my_rop.parm('file').set(str(output_file_path))

# Render using the custom ROP
my_rop.render()

# Save the scene if you want
#hou.hipFile.save('/path/to/save/your_scene.hip')

print("Render complete.")

command.run(["cat", str(output_file_path)])
