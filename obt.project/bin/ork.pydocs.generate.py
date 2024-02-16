#!/usr/bin/env python3

import os, sys, subprocess, pydoc
from obt import path, command

# Constants for documentation generation
PROJECT_NAME = 'OrkEngine Documentation'
AUTHOR_NAME = 'Your Name'
DOCS_DIR = path.stage()/"pydocs-orkengine"
SOURCE_DIR = DOCS_DIR/'source'
BUILD_DIR = DOCS_DIR/'build'

command.run(["rm","-rf",DOCS_DIR])
command.run(["mkdir","-p",DOCS_DIR])

#pdoc_cmdlist = ["python","-m","pydoc", "-w"]
#command.run(pdoc_cmdlist+["orkengine"],working_dir=DOCS_DIR)
#command.run(pdoc_cmdlist+["orkengine.core"],working_dir=DOCS_DIR)
#command.run(pdoc_cmdlist+["orkengine.lev2"],working_dir=DOCS_DIR)
#command.run(pdoc_cmdlist+["orkengine.core._core"],working_dir=DOCS_DIR)
#command.run(pdoc_cmdlist+["orkengine.core._core.dataflow"],working_dir=DOCS_DIR)
#command.run(pdoc_cmdlist+["orkengine.lev2._lev2"],working_dir=DOCS_DIR)
#command.run(pdoc_cmdlist+["orkengine.lev2._lev2.singularity"],working_dir=DOCS_DIR)
#command.run(["open",DOCS_DIR/"orkengine.html"])

#import os
#import pydoc
#from obt import path, command

# Constants for documentation generation
DOCS_DIR = path.stage() / "pydocs-orkengine"

##########################################################
# Ensure the directories are set up
##########################################################

command.run(["rm", "-rf", DOCS_DIR])
command.run(["mkdir", "-p", DOCS_DIR])

##########################################################

def generate_docs(module_name, output_dir):
    """Generate HTML documentation for a specific module."""
    # Dynamically import the module
    __import__(module_name)
    module = sys.modules[module_name]

    # Use pydoc to generate the HTML documentation
    doc_html = pydoc.HTMLDoc().document(module)

    # Define the output file path
    output_file = os.path.join(output_dir, module_name + ".html")

    # Write the documentation to the output file
    with open(output_file, 'w') as f:
        f.write(doc_html)

    print(f"Documentation generated for {module_name} at {output_file}")

##########################################################

import orkengine.core
MODULES = ['orkengine',
           'orkengine.core', 
           'orkengine.core._core', 
           'orkengine.core._core.dataflow', 
           'orkengine.lev2',
           'orkengine.lev2._lev2',
            'orkengine.lev2._lev2.meshutil',
            'orkengine.lev2._lev2.orkidvr',
            'orkengine.lev2._lev2.primitives',
            'orkengine.lev2._lev2.particles',
            'orkengine.lev2._lev2.singularity',
            'orkengine.lev2._lev2.midi',
            'orkengine.lev2._lev2.scenegraph',
            'orkengine.lev2._lev2.ui',
]
for module in MODULES:
    generate_docs(module, DOCS_DIR)
    
command.run(["open",DOCS_DIR/"orkengine.html"])

